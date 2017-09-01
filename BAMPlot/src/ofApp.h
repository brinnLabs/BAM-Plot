#pragma once

#include "ofMain.h"
#include "ofxSvg.h"
#include "ofxUIUtils.h"

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfplay.h>
#include <propkey.h>
#include <strsafe.h>
#include <assert.h>
#include <shlwapi.h>
#include <commdlg.h>


#define DIR_PIN				16
#define STEP_PIN			15

#define DIR_PIN2			18
#define STEP_PIN2			17

#define PEN_PIN				14

#define GO_BUT				2
#define STOP_BUT			3

#define JOYSTICK_UP			6
#define JOYSTICK_DOWN		8
#define JOYSTICK_RIGHT		5
#define JOYSTICK_LEFT		9

#define LIMIT_SWITCH		12
#define LIMIT_SWITCH2		19

#define in_to_mm(x) x*25.4
#define mm_to_in(x) x/25.4

#define BED_WIDTH_IN		12.25
#define BED_HEIGHT_IN		8.5

#define BED_WIDTH_MM		in_to_mm(BED_WIDTH_IN)
#define BED_HEIGHT_MM		in_to_mm(BED_HEIGHT_IN)

#define PIXELS_PER_INCH		72
#define PIXELS_PER_MM		mm_to_in(PIXELS_PER_INCH)

#define BED_WIDTH			BED_WIDTH_IN*PIXELS_PER_INCH
#define BED_HEIGHT			BED_HEIGHT_IN*PIXELS_PER_INCH

// currently this translates to about 50 steps per mm and 1250 steps per inch and 2 steps per pixel
#define STEPS_PER_PIXEL		WIDTH_STEPS/(BED_WIDTH)
#define STEPS_PER_INCH		WIDTH_STEPS/BED_WIDTH_IN
#define STEPS_PER_MM		WIDTH_STEPS/(BED_WIDTH_MM)

#define WIDTH_STEPS			37000

#define round(x)  floor(x + 0.5)

#define MAX_STEP_SPEED		12000

#define canvasXOffset		150
#define canvasYOffset		75

#define firmataTimeout		5000

#define SHIFTED 0x8000

enum stepTranslation {
	PIXELS,
	INCHES,
	MM,
	NONE
};

class ofApp : public ofBaseApp{

public:
	void setup();
	void update();
	void draw();

	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y );
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void windowResized(int w, int h);
	void dragEvent(ofDragInfo dragInfo);
	void gotMessage(ofMessage msg);

private:
	void setupArduino(const int & version);
	void digitalPinChanged(const int & pinNum);
	void analogPinChanged(const int & pinNum);
	void printStepDidFinish(const Stepper_Data & data);
	void stepperFinished(const Stepper_Data & data);
	void updateArduino();
	int lcm(int a, int b);
	int gcd(int a, int b);
	void moveMotors(float step1, float step2, stepTranslation);
	void stopPrint();
	bool isInside(ofImage image, int x, int y);

	void disableButtons();
	void enableButtons();

	bool xAxis;
	bool yAxis;

	void parseGCode(string filename);
	ofPoint XYposition(string lines);
	ofPoint IJposition(string lines);
	

	ofArduino	ard;
	bool		bSetupArduino;	

	vector<ofPoint>	mousePoints;
	vector<ofPolyline> mouseDraw;
	vector<ofPolyline> gcodePoly;
	vector<ofPolyline> outlines;
	vector<ofPolyline> printLines;

	ofPoint penLocation;

	bool penPos;
	void penPosition();
	
	bool svgLoaded;
	bool GCodeLoaded;

	bool limitSwitch;
	bool limitSwitch2;
	bool firstRun;
	bool arduinoAttached;

	
	vector<vector<ofPoint>> svgPoints;

	//used for scaling and movement of loaded images
	vector<vector<ofPoint>> originalSvgPoints;
	vector<ofPolyline> originalGcodePoly;

	bool isPrinting;
	bool stop;
	int numStop;
	int printCounter;
	int pointCounter;
	bool xStepperCompleted;
	bool yStepperCompleted;
	bool firstPrint;

	bool moveItem;
	ofPoint movLoc;
	ofPoint imgOffset;

	bool scaleItem;
	ofPoint scaleLoc;

	bool joyUp;
	bool joyDn;
	bool joyLt;
	bool joyRt;

	int xSent;
	int xReturned;
	int ySent;
	int yReturned;
	bool ignoreTimeout;
	int timesPrintCalled;
	int sampleResolution;

	long timer;

	ofxImgButton openButton;
	void openFile(const pair<bool, int> & state);
	ofxImgButton printButton;
	void print(const pair<bool, int> & state);
	void print();
	ofxImgButton homeButton;
	void home(const pair<bool, int> & state);
	ofxImgButton reconnectButton;
	void reconnect(const pair<bool, int> & state);
	ofxImgButton moveButton;
	void move(const pair<bool, int> & state);
	ofxImgButton scaleButton;
	void scale(const pair<bool, int> & state);
	ofxImgButton clearButton;
	void clear(const pair<bool, int> & state);
	
	bool openItem;

	bool debug;

	ofFbo canvas;
	ofFbo dirtyCanvas;
	bool canvasDrawn;
	bool redrawCanvas;
	void redrawDirtyCanvas();

	ofTrueTypeFont drawFont;

	string drawString;

	long lastCheck;
	void checkConnected();

	bool optimize;
	long optiTimer;
	void optimizePrint();

	int stepSpeed;
	ofxTextInputField speedField;
	ofTrueTypeFont textFont;
	ofxImgButton speedButton;
	void setSpeed(const pair<bool, int> & state);

};

inline HINSTANCE GetInstance()
{
	return (HINSTANCE)GetModuleHandle(NULL); 
}