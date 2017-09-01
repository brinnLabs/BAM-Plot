#include "ofApp.h"



//--------------------------------------------------------------
void ofApp::setup() {
	ofSetVerticalSync(false);
	movLoc.set(canvasXOffset * PIXELS_PER_INCH / 72.0, canvasYOffset * PIXELS_PER_INCH / 72.0);

	ofBackground(80);
	arduinoAttached = false;
	ofSerial	serial;
	vector<string> devices;
	vector <ofSerialDeviceInfo> deviceList = serial.getDeviceList();
	for (int i = 0; i < deviceList.size(); i++) {
		if (strstr(deviceList.at(i).getDevicePath().c_str(), "Serial") != NULL || strstr(deviceList.at(i).getDevicePath().c_str(), "Arduino") != NULL) {
			cout << "Setting up firmata on " << deviceList.at(i).getDevicePath() << endl;
			arduinoAttached = ard.connect(deviceList.at(i).getDeviceName(), 57600); //teensy's friendly name is USB Serial max speed = 125000
		}
	}
	ofAddListener(ard.EInitialized, this, &ofApp::setupArduino);
	bSetupArduino = false;	// flag so we setup arduino when its ready, you don't need to touch this :)

	penLocation.set(0, 0);

	penPos = false;

	numStop = 0;
	printCounter = 0;
	pointCounter = 0;

	limitSwitch = false;
	limitSwitch2 = false;
	stop = false;
	joyUp = false;
	joyDn = false;
	joyLt = false;
	joyRt = false;

	moveItem = false;
	scaleItem = false;
	openItem = false;
	svgLoaded = false;
	GCodeLoaded = false;
	isPrinting = false;
	xStepperCompleted = true;
	yStepperCompleted = true;


	xSent = 0;
	ySent = 0;
	xReturned = 0;
	yReturned = 0;
	ignoreTimeout = false;
	timesPrintCalled = 0;

	openButton.setup("open.png");
	ofAddListener(openButton.buttonEvent, this, &ofApp::openFile);

	homeButton.setup("home.png");
	ofAddListener(homeButton.buttonEvent, this, &ofApp::home);

	reconnectButton.setup("reconnect.png");
	ofAddListener(reconnectButton.buttonEvent, this, &ofApp::reconnect);

	printButton.setup("print.png");
	ofAddListener(printButton.buttonEvent, this, &ofApp::print);

	clearButton.setup("clear.png");
	ofAddListener(clearButton.buttonEvent, this, &ofApp::clear);

	scaleButton.setup("scale.png", "scaleOn.png");
	scaleButton.setToggleMode(true);
	ofAddListener(scaleButton.buttonEvent, this, &ofApp::scale);

	moveButton.setup("move.png", "moveOn.png");
	moveButton.setToggleMode(true);
	ofAddListener(moveButton.buttonEvent, this, &ofApp::move);

	stepSpeed = MAX_STEP_SPEED;
	speedButton.setup("speed.png");
	ofAddListener(speedButton.buttonEvent, this, &ofApp::setSpeed);

	canvas.allocate(ofGetWidth(), ofGetHeight());

	canvas.begin();
	ofClear(255, 255, 255, 0);
	canvas.end();

	canvas.begin();
	openButton.draw(20, 15);
	homeButton.draw(20, 75);
	reconnectButton.draw(20, 135);
	printButton.draw(20, 195);
	clearButton.draw(20, 255);
	speedButton.draw(260, 15);
	ofRect(canvasXOffset, canvasYOffset, BED_WIDTH, BED_HEIGHT);
	ofSetColor(0);
	ofSetLineWidth(1);
	for (int i = 0; i < round(BED_WIDTH_IN); i++) {
		ofLine(canvasXOffset + i * 72, canvasYOffset, canvasXOffset + i * 72, canvasYOffset + 15);
		ofLine(canvasXOffset + 36 + i * 72, canvasYOffset, canvasXOffset + 36 + i * 72, canvasYOffset + 10);
	}
	for (int i = 0; i < round(BED_HEIGHT_IN); i++) {
		ofLine(canvasXOffset, canvasYOffset + i * 72, canvasXOffset + 15, canvasYOffset + i * 72);
		ofLine(canvasXOffset, canvasYOffset + 36 + i * 72, canvasXOffset + 10, canvasYOffset + 36 + i * 72);
	}
	ofSetColor(255);
	for (int i = 1; i < round(BED_WIDTH_IN); i++) {
		ofDrawBitmapString(ofToString(i), canvasXOffset + i * 72 - 5, canvasYOffset - 6);
	}
	for (int i = 1; i < round(BED_HEIGHT_IN); i++) {
		ofDrawBitmapString(ofToString(i), canvasXOffset - 15, canvasYOffset + i * 72 + 5);
	}
	canvas.end();

	dirtyCanvas.allocate(ofGetWidth(), ofGetHeight());

	dirtyCanvas.begin();
	ofClear(255, 255, 255, 0);
	dirtyCanvas.end();

	debug = false;
	drawFont.loadFont(OF_TTF_SANS, 32);

	sampleResolution = 1;
	lastCheck = ofGetElapsedTimeMillis();
	optimize = false;

	optiTimer = ofGetElapsedTimeMillis();

	speedField.setup();
	speedField.setText(ofToString(stepSpeed));
	speedField.setBounds(150, 15, 80, 35);

	textFont.loadFont(OF_TTF_SANS, 18);
	speedField.setFont(textFont);
}

//--------------------------------------------------------------
void ofApp::update() {
	if (arduinoAttached) {
		updateArduino();
		if (bSetupArduino && ofGetElapsedTimeMillis() - lastCheck > 1000) {
			checkConnected();
		}
		if (!isPrinting) { //the joystick only works when we aren't printing
			joyUp ? moveMotors(0, -10, PIXELS) : joyDn ? moveMotors(0, 10, PIXELS) : 0;
			joyRt ? moveMotors(10, 0, PIXELS) : joyLt ? moveMotors(-10, 0, PIXELS) : 0;
		}
		else {
			if (!ignoreTimeout && ofGetElapsedTimeMillis() - timer > firmataTimeout) {
				timer = ofGetElapsedTimeMillis();
				print();
			}
		}

	}
	if (optimize && ofGetElapsedTimeMillis() - optiTimer > 3500) {
		optimize = false;
	}
	if (debug) {

		int numCommands = 0;
		float totalDist = 0;
		for (int i = 0; i < svgPoints.size(); i++) {
			if (i == 0)
				totalDist += penLocation.distance(svgPoints.at(0).at(0));
			totalDist += outlines.at(i).getPerimeter();
			for (int j = 0; j < svgPoints.at(i).size(); j++) {
				numCommands++;
			}
			if (i < svgPoints.size() - 1)
				totalDist += svgPoints.at(i).at(0).distance(svgPoints.at(i + 1).at(0));
		}
		//total distance traveled in pixels * num of steps per pixel / target steps per second
		float timeTaken = (totalDist*STEPS_PER_PIXEL) / (stepSpeed*.1);
		drawString = "Print Commands: " + ofToString(numCommands);
		drawString += "\nSampling Resolution: " + ofToString(sampleResolution);
		drawString += "\nTotal Distance: " + ofToString(totalDist);
		char buf[64];
		sprintf(buf, "%.2i", (int)fmod(timeTaken, 60));
		drawString += "\nEstimated Time: " + ofToString((int)timeTaken / 60) + ":" + ofToString(buf);
		drawString += "\nxStepper: " + ofToString(xStepperCompleted) + "\nyStepper: " + ofToString(yStepperCompleted);
		//drawString += "\nis Connected: " + ofToString(ard.isAttached());
		drawString += "\nxs sent: " + ofToString(xSent) + "\nxs recieved: " + ofToString(xReturned) + "\nys sent: " + ofToString(ySent) + "\nys recieved: " + ofToString(yReturned);
		drawString += "\nxs dropped: " + ofToString(xSent - xReturned) + "\nys dropped: " + ofToString(ySent - yReturned);
		drawString += "\nerror %: " + ofToString(100 * (1 - (xReturned + yReturned + 1) / (float)(xSent + ySent + 1)));
		drawString += "\nTimes Print\nCalled: " + ofToString(timesPrintCalled);

	}

}

//--------------------------------------------------------------
void ofApp::draw() {

	ofSetColor(255);
	scaleButton.draw(20, 315);
	moveButton.draw(20, 375);
	speedField.draw();
	canvas.draw(0, 0);

	dirtyCanvas.draw(0, 0);

	ofTranslate(canvasXOffset, canvasYOffset);

	if (isPrinting && printCounter < printLines.size()) {
		ofSetLineWidth(2);
		ofSetColor(ofColor::red);
		printLines[printCounter].draw();
		if (printCounter + 1 < printLines.size()) {
			ofSetColor(ofColor::blue);
			printLines[printCounter + 1].draw();
		}

	}
	ofSetColor(ofColor::aqua);
	ofEllipse(penLocation, 10, 10);
	ofTranslate(-canvasXOffset, -canvasYOffset);
	if (debug) {
		ofSetColor(0);
		ofDrawBitmapString(drawString, 775, 400);
	}
	ofDrawBitmapString(ofToString((int)ofGetFrameRate()), 1055, 685);
	arduinoAttached ? ofSetColor(ofColor::green) : ofSetColor(ofColor::red);
	arduinoAttached ? drawFont.drawString("Arduino Attached", 585, 40) : drawFont.drawString("Arduino Not Attached", 585, 40);

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	if (GetKeyState(VK_SHIFT) & SHIFTED) {
		switch (key)
		{
		case 'H':
			home(pair<bool, int>(true, 0));
			penLocation.set(0, 0);
			break;
		case 'R':
		{
			ard.disconnect();
			ofSleepMillis(1000);
			ofSerial	serial;
			vector<string> devices;
			vector <ofSerialDeviceInfo> deviceList = serial.getDeviceList();
			vector<string> temp = serial.getDeviceFriendlyNames();
			for (int i = 0; i < temp.size(); i++) {
				if (strstr(temp.at(i).c_str(), "Serial") != NULL) {
					cout << "Setting up firmata on " << deviceList.at(i).getDeviceName() << endl;
					arduinoAttached = ard.connect(deviceList.at(i).getDeviceName(), 57600); //teensy's friendly name is USB Serial max speed = 125000
				}
			}
			ofAddListener(ard.EInitialized, this, &ofApp::setupArduino);
			bSetupArduino = false;
			ofSleepMillis(1000);
			ard.sendFirmwareVersionRequest();
			break;
		}
		case 'O':
			openFile(pair<bool, int>(true, 0));
			break;
		case 'P':
			if (!isPrinting) {
				cout << "printing beginning" << endl;
				printLines.clear();
				if (mouseDraw.size() > 0) {
					printLines = mouseDraw;
				}
				if (gcodePoly.size() > 0) {
					for (int i = 0; i < gcodePoly.size(); i++) {
						printLines.push_back(gcodePoly.at(i));
					}
				}
				if (outlines.size() > 0) {
					for (int i = 0; i < outlines.size(); i++) {
						printLines.push_back(outlines.at(i));
					}
				}
				isPrinting = true;
				firstPrint = true;
				print();
			}
			break;
		case 'Q':
			mouseDraw.clear();
			outlines.clear();
			gcodePoly.clear();
			svgLoaded = false;
			GCodeLoaded = false;
			originalGcodePoly.clear();
			originalSvgPoints.clear();
			xSent = 0;
			ySent = 0;
			xReturned = 0;
			yReturned = 0;
			timesPrintCalled = 0;
			movLoc.set(canvasXOffset * PIXELS_PER_INCH / 72.0, canvasYOffset * PIXELS_PER_INCH / 72.0);
			redrawDirtyCanvas();
			break;
		case 'M':
			moveItem = !moveItem;
			break;
		case 'S':
			scaleItem = !scaleItem;
			if (svgLoaded) {
				originalSvgPoints = svgPoints;
				float xoffset = 10000;
				float yoffset = 10000;
				for (int i = 0; i < svgPoints.size(); i++) {
					for (int j = 0; j < svgPoints.at(i).size(); j++) {
						svgPoints.at(i).at(j).x < xoffset ? xoffset = svgPoints.at(i).at(j).x : 1;
						svgPoints.at(i).at(j).y < yoffset ? yoffset = svgPoints.at(i).at(j).y : 1;
					}
				}
				xoffset <= 0 ? xoffset = abs(xoffset - 5) : xoffset *= -1;
				yoffset <= 0 ? yoffset = abs(yoffset - 5) : yoffset *= -1;
				// iterate over it again and shift everthing over by the offset amount
				for (int i = 0; i < svgPoints.size(); i++) {
					for (int j = 0; j < svgPoints.at(i).size(); j++) {
						originalSvgPoints.at(i).at(j).x += xoffset;
						originalSvgPoints.at(i).at(j).y += yoffset;
					}
				}
			}
			if (GCodeLoaded) {
				originalGcodePoly = gcodePoly;
				float xoffset = 10000;
				float yoffset = 10000;
				for (int i = 0; i < gcodePoly.size(); i++) {
					for (int j = 0; j < gcodePoly.at(i).size(); j++) {
						gcodePoly.at(i)[j].x < xoffset ? xoffset = gcodePoly.at(i)[j].x : 1;
						gcodePoly.at(i)[j].y < yoffset ? yoffset = gcodePoly.at(i)[j].y : 1;
					}
				}
				xoffset <= 0 ? xoffset = abs(xoffset - 5) : xoffset *= -1;
				yoffset <= 0 ? yoffset = abs(yoffset - 5) : yoffset *= -1;
				// iterate over it again and shift everthing over by the offset amount
				for (int i = 0; i < gcodePoly.size(); i++) {
					for (int j = 0; j < gcodePoly.at(i).size(); j++) {
						originalGcodePoly.at(i)[j].x += xoffset;
						originalGcodePoly.at(i)[j].y += yoffset;
					}
				}
			}
			scaleLoc.x = ofGetMouseX() - canvasXOffset;
			redrawDirtyCanvas();
			break;
		}
	}
	if (!speedField.isEditing()) {
		switch (key)
		{
		case OF_KEY_LEFT:
			moveMotors(10, 0, PIXELS);
			break;
		case OF_KEY_RIGHT:
			moveMotors(-10, 0, PIXELS);
			break;
		case OF_KEY_UP:
			moveMotors(0, -10, PIXELS);
			break;
		case OF_KEY_DOWN:
			moveMotors(0, 10, PIXELS);
			break;

		case ' ':
			ard.sendDigital(PEN_PIN, ARD_HIGH);
			break;
		case OF_KEY_BACKSPACE:
			if (mouseDraw.size() > 0)
				mouseDraw.pop_back();
			redrawDirtyCanvas();
			break;
		case 'f':
			ofToggleFullscreen();
			break;
		case 'd':
			debug = !debug;
			break;
		case '-':
			sampleResolution = ofClamp(--sampleResolution, 1, 1000);
			break;
		case '=':
			sampleResolution = ofClamp(++sampleResolution, 1, 1000);
			break;
		default:
			break;
		}
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key) {
	if (key == ' ')
		ard.sendDigital(PEN_PIN, ARD_LOW);
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y) {
	if (moveItem) {
		if (svgLoaded) {
			float xoffset = x*PIXELS_PER_INCH / 72.0 - movLoc.x;
			float yoffset = y*PIXELS_PER_INCH / 72.0 - movLoc.y;
			movLoc.x += xoffset;
			movLoc.y += yoffset;
			// iterate over it again and shift everthing over by the offset amount
			for (int i = 0; i < svgPoints.size(); i++) {
				for (int j = 0; j < svgPoints.at(i).size(); j++) {
					svgPoints.at(i).at(j).x += xoffset;
					svgPoints.at(i).at(j).y += yoffset;
				}
			}
			//do the same for the lines
			outlines.clear();
			for (int i = 0; i < svgPoints.size(); i++) {
				ofPolyline temp;
				for (int j = 0; j < svgPoints.at(i).size(); j++) {
					temp.addVertex(svgPoints.at(i).at(j));
				}
				outlines.push_back(temp);
			}
		}
		else if (GCodeLoaded) {
			float xoffset = x*PIXELS_PER_INCH / 72.0 - movLoc.x;
			float yoffset = y*PIXELS_PER_INCH / 72.0 - movLoc.y;
			movLoc.x += xoffset;
			movLoc.y += yoffset;
			// iterate over it again and shift everthing over by the offset amount
			for (int i = 0; i < gcodePoly.size(); i++) {
				for (int j = 0; j < gcodePoly.at(i).size(); j++) {
					gcodePoly.at(i)[j].x += xoffset;
					gcodePoly.at(i)[j].y += yoffset;
				}
			}
		}
		redrawDirtyCanvas();
	}
	else if (scaleItem) {
		if (svgLoaded) {

			float scale = -1 * ((scaleLoc.x - x) / 100.0)*PIXELS_PER_INCH / 72.0;
			// iterate over it again and shift everthing over by the offset amount
			for (int i = 0; i < svgPoints.size(); i++) {
				for (int j = 0; j < svgPoints.at(i).size(); j++) {
					svgPoints.at(i).at(j).x = (originalSvgPoints.at(i).at(j).x*scale) + movLoc.x - canvasXOffset;
					svgPoints.at(i).at(j).y = (originalSvgPoints.at(i).at(j).y*scale) + movLoc.y - canvasYOffset;
				}
			}
			//do the same for the lines
			outlines.clear();
			for (int i = 0; i < svgPoints.size(); i++) {
				ofPolyline temp;
				for (int j = 0; j < svgPoints.at(i).size(); j++) {
					temp.addVertex(svgPoints.at(i).at(j));
				}
				outlines.push_back(temp);
			}
		}
		else if (GCodeLoaded) {
			float scale = -1 * ((scaleLoc.x - x) / 100.0)*PIXELS_PER_INCH / 72.0;
			// iterate over it again and shift everthing over by the offset amount
			for (int i = 0; i < gcodePoly.size(); i++) {
				for (int j = 0; j < gcodePoly.at(i).size(); j++) {
					gcodePoly.at(i)[j].x = (originalGcodePoly.at(i)[j].x*scale) + movLoc.x - canvasXOffset;
					gcodePoly.at(i)[j].y = (originalGcodePoly.at(i)[j].y*scale) + movLoc.y - canvasYOffset;
				}
			}
		}
		redrawDirtyCanvas();
	}
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {
	if (x > canvasXOffset && x<BED_WIDTH + canvasXOffset && y>canvasYOffset && y < BED_HEIGHT + canvasYOffset && !moveItem && !scaleItem && !openItem)
		mousePoints.push_back(ofPoint(x - canvasXOffset, y - canvasYOffset));
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {

	if (!moveItem && !scaleItem && !openItem) {
		if (x > canvasXOffset && x<BED_WIDTH + canvasXOffset && y>canvasYOffset && y < BED_HEIGHT + canvasYOffset)
			mousePoints.push_back(ofPoint(x - canvasXOffset, y - canvasYOffset));
	}
	else if (moveItem) {
		if (svgLoaded) {
			float xoffset = x*PIXELS_PER_INCH / 72.0 - movLoc.x;
			float yoffset = y*PIXELS_PER_INCH / 72.0 - movLoc.y;
			movLoc.x += xoffset;
			movLoc.y += yoffset;

			// iterate over it again and shift everthing over by the offset amount
			for (int i = 0; i < svgPoints.size(); i++) {
				for (int j = 0; j < svgPoints.at(i).size(); j++) {
					svgPoints.at(i).at(j).x += xoffset;
					svgPoints.at(i).at(j).y += yoffset;
				}
			}
			//do the same for the lines
			outlines.clear();
			for (int i = 0; i < svgPoints.size(); i++) {
				ofPolyline temp;
				for (int j = 0; j < svgPoints.at(i).size(); j++) {
					temp.addVertex(svgPoints.at(i).at(j));
				}
				outlines.push_back(temp);
			}
		}
		else if (GCodeLoaded) {
			float xoffset = x*PIXELS_PER_INCH / 72.0 - movLoc.x;
			float yoffset = y*PIXELS_PER_INCH / 72.0 - movLoc.y;
			movLoc.x += xoffset;
			movLoc.y += yoffset;
			// iterate over it again and shift everthing over by the offset amount
			for (int i = 0; i < gcodePoly.size(); i++) {
				for (int j = 0; j < gcodePoly.at(i).size(); j++) {
					gcodePoly.at(i)[j].x += xoffset;
					gcodePoly.at(i)[j].y += yoffset;
				}
			}
		}
		moveItem = false;
		moveButton.setToggle(false);
		redrawDirtyCanvas();
	}
	else if (scaleItem) {
		if (svgLoaded) {
			float scale = -1 * ((scaleLoc.x - x) / 100.0)*PIXELS_PER_INCH / 72.0;
			// iterate over it again and shift everthing over by the offset amount
			for (int i = 0; i < svgPoints.size(); i++) {
				for (int j = 0; j < svgPoints.at(i).size(); j++) {
					svgPoints.at(i).at(j).x = (originalSvgPoints.at(i).at(j).x*scale) + movLoc.x - canvasXOffset;
					svgPoints.at(i).at(j).y = (originalSvgPoints.at(i).at(j).y*scale) + movLoc.y - canvasYOffset;
				}
			}
			//do the same for the lines
			outlines.clear();
			for (int i = 0; i < svgPoints.size(); i++) {
				ofPolyline temp;
				for (int j = 0; j < svgPoints.at(i).size(); j++) {
					temp.addVertex(svgPoints.at(i).at(j));
				}
				outlines.push_back(temp);
			}
		}
		else if (GCodeLoaded) {
			float scale = -1 * ((scaleLoc.x - x) / 100.0)*PIXELS_PER_INCH / 72.0;
			// iterate over it again and shift everthing over by the offset amount
			for (int i = 0; i < gcodePoly.size(); i++) {
				for (int j = 0; j < gcodePoly.at(i).size(); j++) {
					gcodePoly.at(i)[j].x = (originalGcodePoly.at(i)[j].x*scale) + movLoc.x - canvasXOffset;
					gcodePoly.at(i)[j].y = (originalGcodePoly.at(i)[j].y*scale) + movLoc.y - canvasYOffset;
				}
			}
		}
		scaleItem = false;
		scaleButton.setToggle(false);
		redrawDirtyCanvas();
	}
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {


	if (!moveItem && !scaleItem && !openItem) {
		if (x > canvasXOffset && x<BED_WIDTH + canvasXOffset && y>canvasYOffset && y < BED_HEIGHT + canvasYOffset)
			mousePoints.push_back(ofPoint(x - canvasXOffset, y - canvasYOffset));
		if (mousePoints.size() > 0) {
			/*oldloc = newloc;
			newloc = mousePoints[0];*/
			ofPolyline temp;
			for (int i = 0; i < mousePoints.size(); i++)
			{
				temp.addVertex(mousePoints.at(i).x, mousePoints.at(i).y);
			}
			mouseDraw.push_back(temp);
			mousePoints.clear();
		}
		redrawDirtyCanvas();
	}
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h) {

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg) {

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) {

}

void ofApp::setupArduino(const int & version) {

	// remove listener because we don't need it anymore
	ofRemoveListener(ard.EInitialized, this, &ofApp::setupArduino);

	// printf( firmware name and version to the console
	ofLogNotice() << ard.getFirmwareName();
	ofLogNotice() << "firmata v" << ard.getMajorFirmwareVersion() << "." << ard.getMinorFirmwareVersion();

	ard.sendReset();

	ard.sendStepper2Wire(DIR_PIN, STEP_PIN, 200, LIMIT_SWITCH, LIMIT_SWITCH2);
	ard.sendStepper2Wire(DIR_PIN2, STEP_PIN2);

	ard.sendDigitalPinMode(PEN_PIN, ARD_OUTPUT);

	ard.sendDigitalPinMode(LIMIT_SWITCH, ARD_INPUT_PULLUP);
	ard.sendDigitalPinMode(LIMIT_SWITCH2, ARD_INPUT_PULLUP);

	ard.sendDigitalPinMode(GO_BUT, ARD_INPUT_PULLUP);
	ard.sendDigitalPinMode(STOP_BUT, ARD_INPUT_PULLUP);
	ard.sendDigitalPinMode(JOYSTICK_UP, ARD_INPUT_PULLUP);
	ard.sendDigitalPinMode(JOYSTICK_DOWN, ARD_INPUT_PULLUP);
	ard.sendDigitalPinMode(JOYSTICK_RIGHT, ARD_INPUT_PULLUP);
	ard.sendDigitalPinMode(JOYSTICK_LEFT, ARD_INPUT_PULLUP);



	// Listen for changes on the digital and analog pins
	ofAddListener(ard.EDigitalPinChanged, this, &ofApp::digitalPinChanged);
	ofAddListener(ard.EAnalogPinChanged, this, &ofApp::analogPinChanged);
	ofAddListener(ard.EStepperDataReceived, this, &ofApp::stepperFinished);

	// it is now safe to send commands to the Arduino
	bSetupArduino = true;
}

//--------------------------------------------------------------
void ofApp::updateArduino() {
	// update the arduino, get any data or messages.
	// the call to ard.update() is required
	ard.update();
	//arduinoAttached = 
	if (bSetupArduino) {
		if (ard.getDigital(STOP_BUT) == ARD_HIGH && numStop > 0) numStop--;
		else if (ard.getDigital(STOP_BUT) == ARD_LOW) numStop++;
		if (firstRun) {
			firstRun = false;
			penLocation.x = -1300;
			//moveMotors(-1300, 0, PIXELS);
			home(pair<bool, int>(false, 0));
		}
		if (numStop > 5) {
			numStop = 0;
			stop = true;
		}
	}

}

// digital pin event handler, called whenever a digital pin value has changed
// note: if an analog pin has been set as a digital pin, it will be handled
// by the digitalPinChanged function rather than the analogPinChanged function.

//--------------------------------------------------------------
void ofApp::digitalPinChanged(const int & pinNum) {
	if (pinNum == LIMIT_SWITCH && ard.getDigital(LIMIT_SWITCH) == ARD_LOW) {
		limitSwitch = true;
	}
	if (pinNum == LIMIT_SWITCH2 && ard.getDigital(LIMIT_SWITCH2) == ARD_LOW) {
		limitSwitch2 = true;
	}
	if (pinNum == LIMIT_SWITCH && ard.getDigital(LIMIT_SWITCH) == ARD_HIGH) {
		limitSwitch = false;
	}
	if (pinNum == LIMIT_SWITCH2 && ard.getDigital(LIMIT_SWITCH2) == ARD_HIGH) {
		limitSwitch2 = false;
	}
	if (pinNum == GO_BUT && ard.getDigital(GO_BUT) == ARD_LOW) {
		penPos = true;
		penPosition();
	}
	if (pinNum == GO_BUT && ard.getDigital(GO_BUT) == ARD_HIGH) {
		penPos = false;
		penPosition();
	}
	if (pinNum == STOP_BUT && ard.getDigital(STOP_BUT) == ARD_LOW) {
		numStop += 2;
	}
	if (pinNum == JOYSTICK_UP) {
		joyUp = ard.getDigital(JOYSTICK_UP) == ARD_LOW ? true : false;
	}
	if (pinNum == JOYSTICK_DOWN) {
		joyDn = ard.getDigital(JOYSTICK_DOWN) == ARD_LOW ? true : false;
	}
	if (pinNum == JOYSTICK_RIGHT) {
		joyRt = ard.getDigital(JOYSTICK_RIGHT) == ARD_LOW ? true : false;
	}
	if (pinNum == JOYSTICK_LEFT) {
		joyLt = ard.getDigital(JOYSTICK_LEFT) == ARD_LOW ? true : false;
	}
}

// 
// analog pin event handler, called whenever an analog pin value has changed
//--------------------------------------------------------------
void ofApp::analogPinChanged(const int & pinNum) {
	// do something with the analog input. here we're simply going to printf( the pin number and
	// value to the screen each time it changes
}
void ofApp::penPosition() {
	if (penPos)
		ard.sendDigital(PEN_PIN, ARD_HIGH);
	else
		ard.sendDigital(PEN_PIN, ARD_LOW);
}
int ofApp::lcm(int a, int b)
{
	return (a*b) / gcd(a, b);
}
int ofApp::gcd(int a, int b)
{
	if (b == 0)
		return a;
	else
		return gcd(b, a%b);
}
void ofApp::moveMotors(float stepf1, float stepf2, stepTranslation type) {
	if (arduinoAttached) {
		float spp = STEPS_PER_PIXEL;
		stepf1 = limitSwitch ? 0 : stepf1;
		stepf2 = limitSwitch2 ? 0 : stepf2;
		switch (type)
		{
		case PIXELS:
			if (!isPrinting) {
				penLocation.x -= (stepf1 / spp) * 10.78;
				penLocation.y -= (stepf2 / spp) * 10.78;
			}
			stepf1 *= STEPS_PER_PIXEL;
			stepf2 *= STEPS_PER_PIXEL;
			break;
		case INCHES:
			if (!isPrinting) {
				penLocation.x -= stepf1*PIXELS_PER_INCH;
				penLocation.y -= stepf2*PIXELS_PER_INCH;
			}
			stepf1 *= STEPS_PER_INCH;
			stepf2 *= STEPS_PER_INCH;
			break;
		case MM:
			if (!isPrinting) {
				penLocation.x -= stepf1*PIXELS_PER_MM;
				penLocation.y -= stepf2*PIXELS_PER_MM;
			}
			stepf1 *= STEPS_PER_MM;
			stepf2 *= STEPS_PER_MM;
			break;
		default:
			break;
		}
		//determine direction by sign
		int dir1 = stepf1 >= 0 ? CW : CCW;
		int dir2 = stepf2 >= 0 ? CW : CCW;
		//there are no negative steps we need to work in the positive realm
		stepf1 = abs(stepf1);
		stepf2 = abs(stepf2);
		//no matter how small the step is unless it is explicitly 0 it needs to move
		int step1 = ceil(stepf1);
		int step2 = ceil(stepf2);

		/*if (!xStepperCompleted)
			step2 = 0;
			if (!yStepperCompleted)
			step1 = 0;*/

		if (step1 == 0) { //no x movment  
			yStepperCompleted = false;
			ard.sendStepperMove(1, dir2, step2, stepSpeed, 5);
			ySent++;
			//ofAddListener(ard.EStepperIsDone, this, &ofApp::stepperFinished);
		}
		else if (step2 == 0) { //no y movement
			xStepperCompleted = false;
			ard.sendStepperMove(0, dir1, step1, stepSpeed, 5);
			xSent++;
			//ofAddListener(ard.EStepperIsDone, this, &ofApp::stepperFinished);
		}
		else if (step1 != 0 && step2 != 0) { //we need to interpolate diagonal movement
			xStepperCompleted = false;
			yStepperCompleted = false;
			xSent++;
			ySent++;
			/*ofAddListener(ard.EStepperIsDone, this, &ofApp::stepperFinished);
			ofAddListener(ard.EStepperIsDone, this, &ofApp::stepperFinished);*/
			if (step1 > step2) {
				ard.sendStepperMove(0, dir1, step1, stepSpeed);
				ard.sendStepperMove(1, dir2, step2, round(stepSpeed * (step2 / (float)step1)));
			}
			else if (step1 > step2) {
				ard.sendStepperMove(0, dir1, step1, round(stepSpeed * (step1 / (float)step2)));
				ard.sendStepperMove(1, dir2, step2, stepSpeed);
			}
			else {
				ard.sendStepperMove(0, dir1, step1, stepSpeed);
				ard.sendStepperMove(1, dir2, step2, stepSpeed);
			}
		}

	}
}
void ofApp::printStepDidFinish(const Stepper_Data & data) {
	if (data.type == STEPPER_DONE) {
		if (data.id == 0) {
			xReturned++;
			xStepperCompleted = true;
		}
		if (data.id == 1) {
			yReturned++;
			yStepperCompleted = true;
		}
		else if (data.type == STEPPER_LIMIT_SWITCH_A) {
			limitSwitch = data.data;
		}
		else if (data.type == STEPPER_LIMIT_SWITCH_B) {
			limitSwitch2 = data.data;
		}
	}
	if (xStepperCompleted && yStepperCompleted) {
		if (printLines.size() > 0 && printCounter < printLines.size()) {
			timer = ofGetElapsedTimeMillis();
			print();
		}
		else {
			isPrinting = false;
		}
	}
}
void ofApp::stepperFinished(const Stepper_Data & data) {
	if (isPrinting) {
		printStepDidFinish(data);
	}
	else {
		if (data.type == STEPPER_DONE) {
			if (data.id == 0) {
				xReturned++;
				xStepperCompleted = true;
			}
			if (data.id == 1) {
				yReturned++;
				yStepperCompleted = true;
			}
		}
		else if (data.type == STEPPER_LIMIT_SWITCH_A) {
			limitSwitch = data.data;
		}
		else if (data.type == STEPPER_LIMIT_SWITCH_B) {
			limitSwitch2 = data.data;
		}
	}
}
void ofApp::parseGCode(string filename) {
	float old_x_pos = 0;
	float x_pos = 0;
	float old_y_pos = 0;
	float y_pos = 0;
	float i_pos = 0;
	float j_pos = 0;
	ofBuffer buffer = ofBufferFromFile(filename);
	string line = buffer.getFirstLine(); //file name of the G code commands
	gcodePoly.clear();
	ofPolyline tempPolyline;
	bool inch = false;
	bool mm = true;
	while (!buffer.isLastLine()) {

		if (line.find("G90") != -1)
			printf("started parsing gcode");

		if (line.find("G20") != -1) {// working in inch;
			ofLogNotice("G-Code") << "Working in Inches";
			inch = true;
			mm = false;
		}

		if (line.find("G21") != -1)// working in mm;
		{
			ofLogNotice("G-Code") << "Working in Millimeters";
			inch = false;
			mm = true;
		}

		if (line.find("M05") != -1) {}
		if (line.find("M03") != -1) {}
		if (line.find("M02") != -1) {}
		if (line.find("G1F") != -1 || line.find("G1 F") != -1) {}
		if (line.find("G00") != -1 || line.find("G0 ") != -1 || line.find("G1 ") != -1 || line.find("G01") != -1) {
			if (line.find("X") != -1 && line.find("Y") != -1) { //Ignore line not dealing with XY plane
				//linear engraving movement
				if (line.find("G00") != -1 || line.find("G0 ") != -1) {
					if (tempPolyline.size() > 0)
						gcodePoly.push_back(tempPolyline);
					tempPolyline.clear();//make new polyline
				}

				ofPoint xy = XYposition(line);
				if (inch) {
					x_pos = PIXELS_PER_INCH*xy.x;
					y_pos = PIXELS_PER_INCH*xy.y;
				}
				else {
					x_pos = PIXELS_PER_MM*xy.x;
					y_pos = PIXELS_PER_MM*xy.y;
				}
				tempPolyline.addVertex(x_pos, y_pos); //add vertex. if g0 start new polyline
			}
		}

		if (line.find("G02") != -1 || line.find("G03") != -1) { //circular interpolation
			if (line.find("X") != -1 && line.find("Y") != -1 && line.find("I") != -1 && line.find("J") != -1) {
				old_x_pos = x_pos;
				old_y_pos = y_pos;

				ofPoint xy = XYposition(line);
				ofPoint ij = IJposition(line);
				if (inch) {
					x_pos = PIXELS_PER_INCH*xy.x;
					y_pos = PIXELS_PER_INCH*xy.y;
					i_pos = PIXELS_PER_INCH*ij.x;
					j_pos = PIXELS_PER_INCH*ij.y;
				}
				else {
					x_pos = PIXELS_PER_MM*xy.x;
					y_pos = PIXELS_PER_MM*xy.y;
					i_pos = PIXELS_PER_MM*ij.x;
					j_pos = PIXELS_PER_MM*ij.y;
				}
				float xcenter = old_x_pos + ij.x;   //center of the circle for interpolation
				float ycenter = old_y_pos + ij.y;


				float Dx = x_pos - xcenter;
				float Dy = y_pos - ycenter;     //vector [Dx,Dy] points from the circle center to the new position

				float r = sqrt(pow(i_pos, 2) + pow(j_pos, 2));   // radius of the circle

				ofPoint e1 = ofPoint(-i_pos, -j_pos); //pointing from center to current position
				ofPoint e2;
				if (line.find("G02") != -1) //clockwise
					e2 = ofPoint(e1[1], -e1[0]);      //perpendicular to e1. e2 and e1 forms x-y system (clockwise)
				else                   //counterclockwise
					e2 = ofPoint(-e1[1], e1[0]);      //perpendicular to e1. e1 and e2 forms x-y system (counterclockwise)

				//[Dx,Dy]=e1*cos(theta)+e2*sin(theta), theta is the open angle

				float costheta = (Dx*e1[0] + Dy*e1[1]) / pow(r, 2);
				float sintheta = (Dx*e2[0] + Dy*e2[1]) / pow(r, 2);        //theta is the angule spanned by the circular interpolation curve

				if (costheta > 1)  // there will always be some numerical errors! Make sure abs(costheta)<=1
					costheta = 1;
				else if (costheta < -1)
					costheta = -1;

				float theta = acos(costheta);
				if (sintheta < 0)
					theta = 2.0*PI - theta;

				int no_step = int(round(r*theta / 5.0));   // number of point for the circular interpolation

				for (int i = 1; i < no_step + 1; i++) {
					float tmp_theta = i*theta / no_step;
					float tmp_x_pos = xcenter + e1[0] * cos(tmp_theta) + e2[0] * sin(tmp_theta);
					float tmp_y_pos = ycenter + e1[1] * cos(tmp_theta) + e2[1] * sin(tmp_theta);
					tempPolyline.addVertex(tmp_x_pos, tmp_y_pos); //add vertex to polyline

				}
			}
		}
		line = buffer.getNextLine();

	}
	//we need to check for any x/y offset into the negative area so iterate over the polylines
	float xoffset = 10000;
	float yoffset = 10000;
	for (int i = 0; i < gcodePoly.size(); i++) {
		for (int j = 0; j < gcodePoly.at(i).size(); j++) {
			gcodePoly.at(i)[j].x < xoffset ? xoffset = gcodePoly.at(i)[j].x : 1;
			gcodePoly.at(i)[j].y < yoffset ? yoffset = gcodePoly.at(i)[j].y : 1;
		}
	}
	xoffset = abs(xoffset - 5);
	yoffset = abs(yoffset - 5);
	// iterate over it again and shift everthing over by the offset amount
	for (int i = 0; i < gcodePoly.size(); i++) {
		for (int j = 0; j < gcodePoly.at(i).size(); j++) {
			gcodePoly.at(i)[j].x += xoffset;
			gcodePoly.at(i)[j].y += yoffset;
		}
	}
}
ofPoint ofApp::XYposition(string lines) {
	//given a movement command line, return the X Y position
	int xchar_loc = lines.find('X');
	int i = xchar_loc + 1;
	while (lines[i] > 47 && lines[i] < 58 || lines[i] == '-' || lines[i] == '.') {
		i++;
	}
	float x_pos = ofToFloat(lines.substr(xchar_loc + 1, i - xchar_loc));

	int ychar_loc = lines.find('Y');
	i = ychar_loc + 1;
	while (lines[i] > 47 && lines[i] < 58 || lines[i] == '-' || lines[i] == '.') {
		i++;
	}
	float y_pos = ofToFloat(lines.substr(ychar_loc + 1, i - ychar_loc));

	return ofPoint(x_pos, y_pos);
}
ofPoint ofApp::IJposition(string lines) {
	//given a G02 or G03 movement command line, return the I J position
	int ichar_loc = lines.find('I');
	int i = ichar_loc + 1;
	while (lines[i] > 47 && lines[i] < 58 || lines[i] == '-' || lines[i] == '.') {
		i++;
	}
	float i_pos = ofToFloat(lines.substr(ichar_loc + 1, i - ichar_loc));

	int jchar_loc = lines.find('J');
	i = jchar_loc + 1;
	while (lines[i] > 47 && lines[i] < 58 || lines[i] == '-' || lines[i] == '.') {
		i++;
	}
	float j_pos = ofToFloat(lines.substr(jchar_loc + 1, i - jchar_loc));

	return ofPoint(i_pos, j_pos);
}
void ofApp::openFile(const pair<bool, int> & state)
{
	disableButtons();
	if (state.first) {
		openItem = true;
		const WCHAR *lpstrFilter =
			L"Vector Images\0*.svg;\0"
			L"GCode Files\0*.nc;*.ngc;*.cnc;*.gcode\0"
			L"All files\0*.*\0";

		HRESULT hr = S_OK;

		OPENFILENAME ofn;
		ZeroMemory(&ofn, sizeof(ofn));

		WCHAR szFileName[MAX_PATH];
		szFileName[0] = L'\0';

		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.hInstance = GetInstance();
		ofn.lpstrFilter = lpstrFilter;
		ofn.lpstrFile = szFileName;
		ofn.nMaxFile = MAX_PATH;
		ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;

		if (GetOpenFileName(&ofn))
		{
			char ch[260];
			char DefChar = ' ';
			WideCharToMultiByte(CP_ACP, 0, szFileName, -1, ch, 260, &DefChar, NULL);

			string fileName = ofToString(ch);
			string fileExt = fileName.substr(fileName.find_last_of('.'), fileName.length() - fileName.find_last_of('.'));
			if (fileExt.compare(".svg") == 0) {
				svgLoaded = true;
				ofxSVG tempSvg;
				tempSvg.load(fileName);
				outlines.clear();
				svgPoints.clear();
				for (int i = 0; i < tempSvg.getNumPath(); i++) {
					ofPath p = tempSvg.getPathAt(i);
					//svg defaults to non zero winding which doesn't look so good as contours
					p.setPolyWindingMode(OF_POLY_WINDING_ODD);
					vector<ofPolyline>& lines = p.getOutline();
					for (int j = 0; j < (int)lines.size(); j++) {
						outlines.push_back(lines[j].getResampledBySpacing(sampleResolution)); //we need to adjust dimensions
					}

				}
				for (int i = 0; i < outlines.size(); i++) {
					svgPoints.push_back(outlines.at(i).getVertices());
				}
				float xoffset = 10000;
				float yoffset = 10000;
				for (int i = 0; i < svgPoints.size(); i++) {
					for (int j = 0; j < svgPoints.at(i).size(); j++) {
						svgPoints.at(i).at(j).x < xoffset ? xoffset = svgPoints.at(i).at(j).x : 1;
						svgPoints.at(i).at(j).y < yoffset ? yoffset = svgPoints.at(i).at(j).y : 1;
					}
				}
				xoffset <= 0 ? xoffset = abs(xoffset - 5) : xoffset *= -1;
				yoffset <= 0 ? yoffset = abs(yoffset - 5) : yoffset *= -1;
				// iterate over it again and shift everthing over by the offset amount
				for (int i = 0; i < svgPoints.size(); i++) {
					for (int j = 0; j < svgPoints.at(i).size(); j++) {
						svgPoints.at(i).at(j).x += xoffset;
						svgPoints.at(i).at(j).y += yoffset;
					}
				}
				//scale it up to the higher resolution
				for (int i = 0; i < svgPoints.size(); i++) {
					for (int j = 0; j < svgPoints.at(i).size(); j++) {
						svgPoints.at(i).at(j).x *= PIXELS_PER_INCH / 72.0;
						svgPoints.at(i).at(j).y *= PIXELS_PER_INCH / 72.0;
					}
				}
				outlines.clear();
				for (int i = 0; i < svgPoints.size(); i++) {
					ofPolyline temp;
					for (int j = 0; j < svgPoints.at(i).size(); j++) {
						temp.addVertex(svgPoints.at(i).at(j));
					}
					outlines.push_back(temp);
				}
			}
			if (fileExt.compare(".ngc") == 0 || fileExt.compare(".gcode") == 0 || fileExt.compare(".cnc") == 0 || fileExt.compare(".nc") == 0) {
				GCodeLoaded = true;
				parseGCode(fileName);
			}
			ofLogNotice("Open File") << "Opened File Successfully";
		}
		else
		{
			// GetOpenFileName can return FALSE because the user cancelled,
			// or because it failed. Check for errors.

			ofLogNotice("Open File") << "Failed to Load File";
		}
		openItem = false;
	}
	redrawDirtyCanvas();
	enableButtons();
}
void ofApp::home(const pair<bool, int> & state) {
	if (state.first) {
		if (arduinoAttached) {
			moveMotors(penLocation.x, penLocation.y, PIXELS);
			ard.homeStepper(0);
			//moveMotors(-100, 0, PIXELS);
		}
		penLocation.set(0, 0);
	}
}
void ofApp::print(const pair<bool, int> & state) {
	if (state.first) {
		if (arduinoAttached) {
			if (!isPrinting) {
				printLines.clear();
				if (mouseDraw.size() > 0) {
					printLines = mouseDraw;
				}
				if (gcodePoly.size() > 0) {
					for (int i = 0; i < gcodePoly.size(); i++) {
						printLines.push_back(gcodePoly.at(i));
					}
				}
				if (outlines.size() > 0) {
					for (int i = 0; i < outlines.size(); i++) {
						printLines.push_back(outlines.at(i));
					}
				}
				optimize = true;
				optiTimer = ofGetElapsedTimeMillis();
				optimizePrint();
			}
		}
	}
}
void ofApp::print() {
	timesPrintCalled++;
	if (arduinoAttached) {
		if (printLines.size() > 0 && printCounter < printLines.size()) {
			if (stop) {
				stopPrint();
			}
			else {
				if (firstPrint || pointCounter >= printLines[printCounter].size()) {
					if (!firstPrint)
						printCounter++;
					firstPrint = false;
					penPos = false;
					penPosition();
					if (printCounter < printLines.size()) {
						ignoreTimeout = true;
						float deltax = penLocation.x - printLines[printCounter][0].x;
						float deltay = penLocation.y - printLines[printCounter][0].y;
						moveMotors(deltax, deltay, PIXELS);
						pointCounter = 0;
						penLocation = printLines[printCounter][0];
					}
				}
				else {
					ignoreTimeout = false;
					penPos = true;
					penPosition();
					moveMotors((printLines[printCounter][pointCounter - 1].x - printLines[printCounter][pointCounter].x), (printLines[printCounter][pointCounter - 1].y - printLines[printCounter][pointCounter].y), PIXELS);
					penLocation = printLines[printCounter][pointCounter];
				}


				pointCounter++;
			}
		}
		else {
			stopPrint();
		}
	}
	else {
		stopPrint();
	}
}

void ofApp::stopPrint() {
	penPos = false;
	penPosition();
	stop = false;
	isPrinting = false;
	printCounter = 0;
	pointCounter = 0;
}
bool ofApp::isInside(ofImage image, int x, int y) {
	//if (x < image.width && y < image.height){
	if (x > 0 && y > 0) {
		if (image.width - x > 0 && image.height - y > 0) {
			return true;
		}
	}
	return false;
}

void ofApp::checkConnected() {
	if (ard.isAttached()) {
		//we are attached, good reset the timer
		lastCheck = ofGetElapsedTimeMillis();
	}
	else {
		ard.disconnect();
		arduinoAttached = false;
		bSetupArduino = false;

	}
}

void ofApp::optimizePrint() {

	vector<ofPolyline> tempLines = printLines;
	printLines.clear();
	printLines.push_back(tempLines.front());
	tempLines.erase(tempLines.begin());
	while (tempLines.size() > 0) {
		//check distance off all dots
		int index = 0;
		bool isTail;
		float compDistance = 100000000;
		ofPoint point1 = printLines.back()[printLines.back().size() - 1];
		for (int i = 0; i < tempLines.size(); i++) {
			float dist1 = point1.distance(tempLines.at(i)[0]);
			float dist2 = point1.distance(tempLines.at(i)[tempLines.at(i).size() - 1]);
			if (dist1 > dist2) {
				if (dist1 < compDistance) {
					compDistance = dist1;
					index = i;
					isTail = false;
				}
			}
			else {
				if (dist2 < compDistance) {
					compDistance = dist2;
					index = i;
					isTail = true;
				}
			}
		}
		//ok we've found our next closest point, if its a tail we need to reverse all the points
		//add the point and delete
		if (isTail) {
			ofPolyline temp;
			for (int i = tempLines.at(index).size() - 1; i >= 0; i--) {
				temp.addVertex(tempLines.at(index)[i]);
			}
			printLines.push_back(temp);
			tempLines.erase(tempLines.begin() + index);
		}
		else {
			printLines.push_back(tempLines.at(index));
			tempLines.erase(tempLines.begin() + index);
		}

	}

	isPrinting = true;
	firstPrint = true;
}
void ofApp::reconnect(const pair<bool, int> & state) {
	if (state.first) {
		if (arduinoAttached)
			ard.disconnect();
		ard.reset();
		ofSleepMillis(1000);
		vector<string> devices;
		ofSerial serial;
		vector <ofSerialDeviceInfo> deviceList = serial.getDeviceList();
		vector<string> temp = serial.getDeviceFriendlyNames();
		for (int i = 0; i < temp.size(); i++) {
			if (strstr(temp.at(i).c_str(), "Serial") != NULL || strstr(temp.at(i).c_str(), "Arduino") != NULL) {
				cout << "Setting up firmata on " << deviceList.at(i).getDeviceName() << endl;
				arduinoAttached = ard.connect(deviceList.at(i).getDevicePath(), 57600); //teensy's friendly name is USB Serial max speed = 125000
			}
		}
		ofAddListener(ard.EInitialized, this, &ofApp::setupArduino);
		bSetupArduino = false;
		ofSleepMillis(1000);
	}
}

void ofApp::clear(const pair<bool, int> & state) {
	if (state.first) {
		mouseDraw.clear();
		outlines.clear();
		svgPoints.clear();
		gcodePoly.clear();
		svgLoaded = false;
		GCodeLoaded = false;
		originalGcodePoly.clear();
		originalSvgPoints.clear();
		xSent = 0;
		ySent = 0;
		xReturned = 0;
		yReturned = 0;
		timesPrintCalled = 0;
		movLoc.set(canvasXOffset * PIXELS_PER_INCH / 72.0, canvasYOffset * PIXELS_PER_INCH / 72.0);
	}
}
void ofApp::move(const pair<bool, int> & state) {
	moveItem = state.first;
}
void ofApp::scale(const pair<bool, int> & state) {
	scaleItem = state.first;
	if (svgLoaded) {
		originalSvgPoints = svgPoints;
		float xoffset = 10000;
		float yoffset = 10000;
		for (int i = 0; i < svgPoints.size(); i++) {
			for (int j = 0; j < svgPoints.at(i).size(); j++) {
				svgPoints.at(i).at(j).x < xoffset ? xoffset = svgPoints.at(i).at(j).x : 1;
				svgPoints.at(i).at(j).y < yoffset ? yoffset = svgPoints.at(i).at(j).y : 1;
			}
		}
		xoffset <= 0 ? xoffset = abs(xoffset - 5) : xoffset *= -1;
		yoffset <= 0 ? yoffset = abs(yoffset - 5) : yoffset *= -1;
		// iterate over it again and shift everthing over by the offset amount
		for (int i = 0; i < svgPoints.size(); i++) {
			for (int j = 0; j < svgPoints.at(i).size(); j++) {
				originalSvgPoints.at(i).at(j).x += xoffset;
				originalSvgPoints.at(i).at(j).y += yoffset;
			}
		}
	}
	if (GCodeLoaded) {
		originalGcodePoly = gcodePoly;
		float xoffset = 10000;
		float yoffset = 10000;
		for (int i = 0; i < gcodePoly.size(); i++) {
			for (int j = 0; j < gcodePoly.at(i).size(); j++) {
				gcodePoly.at(i)[j].x < xoffset ? xoffset = gcodePoly.at(i)[j].x : 1;
				gcodePoly.at(i)[j].y < yoffset ? yoffset = gcodePoly.at(i)[j].y : 1;
			}
		}
		xoffset <= 0 ? xoffset = abs(xoffset - 5) : xoffset *= -1;
		yoffset <= 0 ? yoffset = abs(yoffset - 5) : yoffset *= -1;
		// iterate over it again and shift everthing over by the offset amount
		for (int i = 0; i < gcodePoly.size(); i++) {
			for (int j = 0; j < gcodePoly.at(i).size(); j++) {
				originalGcodePoly.at(i)[j].x += xoffset;
				originalGcodePoly.at(i)[j].y += yoffset;
			}
		}
	}
	scaleLoc.x = ofGetMouseX() - canvasXOffset;
}

void ofApp::redrawDirtyCanvas() {
	dirtyCanvas.begin();
	ofClear(255, 255, 255, 0);
	ofSetColor(255);

	ofTranslate(canvasXOffset, canvasYOffset);
	ofSetLineWidth(1);
	for (int i = 0; i < outlines.size(); i++) {
		ofSetColor(ofColor::chartreuse);
		ofEllipse(svgPoints[0][0], 5, 5);
		ofSetColor(0);
		outlines.at(i).draw(); //
	}
	ofSetColor(0);
	ofSetLineWidth(4);
	for (int j = 0; j < mouseDraw.size(); j++) {
		mouseDraw[j].getSmoothed(3).draw();
	}
	for (int j = 0; j < gcodePoly.size(); j++) {
		gcodePoly[j].draw();
	}
	for (int i = 0; i < mousePoints.size(); i++)
	{
		ofEllipse(mousePoints.at(i).x, mousePoints.at(i).y, 3, 3);
	}
	ofTranslate(-canvasXOffset, -canvasYOffset);
	dirtyCanvas.end();
}

void ofApp::setSpeed(const pair<bool, int> & state) {
	if (state.first) {
		if (arduinoAttached) {
			ard.setStepperSpeed(0, ofToInt(speedField.text));
			ard.setStepperSpeed(1, ofToInt(speedField.text));
		}
	}
}

void ofApp::disableButtons() {
	openButton.setClickable(false);
	homeButton.setClickable(false);
	reconnectButton.setClickable(false);
	printButton.setClickable(false);
	clearButton.setClickable(false);
	scaleButton.setClickable(false);
	moveButton.setClickable(false);
	speedButton.setClickable(false);
}
void ofApp::enableButtons() {
	openButton.setClickable(true);
	homeButton.setClickable(true);
	reconnectButton.setClickable(true);
	printButton.setClickable(true);
	clearButton.setClickable(true);
	scaleButton.setClickable(true);
	moveButton.setClickable(true);
	speedButton.setClickable(true);
}