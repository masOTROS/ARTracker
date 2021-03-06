#pragma once

#include "ofxOpenCv.h"
#include "ofxARToolkitPlus.h"
#include "ofxUI.h"

#include "ofMain.h"

#define WIDTH 640
#define HEIGHT 480

// Uncomment this to use a camera instead of a video file
#define CAMERA_CONNECTED

#define CORNERS 4

typedef struct{
    int ID;
    vector<ofPoint> center;
    vector<ofPoint> corner[CORNERS];
    vector<float> time;

	ofPoint lastCenter;
    ofPoint lastCorner[CORNERS];
    float lastTime;
}ARMarker;

class ofApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();

		void keyPressed  (int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void windowResized(int w, int h);
    
        void exit();
	
		/* Size of the image */
		int width, height;
	
		/* Use either camera or a video file */
		#ifdef CAMERA_CONNECTED
		ofVideoGrabber vidGrabber;
		#else
		ofVideoPlayer vidPlayer;
		#endif

		/* ARToolKitPlus class */	
		ofxARToolkitPlus artk;
    
        /* ARToolKitPlus parameters */
        bool byn;
		int bynThreshold;
		/* OpenCV images */
		ofxCvColorImage colorImage;
		ofxCvGrayscaleImage grayImage;
		ofxCvGrayscaleImage	grayThres;
	
		/* Image to distort on to the marker */
		//ofImage displayImage;
		/* The four corners of the image */
		vector<ofPoint> displayImageCorners;
    
        vector< ARMarker > markers;
    
		ofxUISuperCanvas *      controlGUI;
		void controlGUIEvent(ofxUIEventArgs &e);

        ofxUISuperCanvas *      recordGUI;
        void recordGUIEvent(ofxUIEventArgs &e);

		bool record;
		float recordBegin,recordEnd;
		float saveBegin,saveEnd;
		ofFbo recordFbo;
		float recordTimeLabel;

		void save();
		string saveFile;
		int saveAlpha;

		bool grid;
		ofFbo gridFbo;
		bool gridOpen;
		int gridCornerGrabbed;
		ofPoint gridCorners[CORNERS];
		int gridRows,gridCols;
		void drawGrid();
};