#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
	width = 640;
	height = 480;
	
	// Print the markers from the "AllBchThinMarkers.png" file in the data folder
#ifdef CAMERA_CONNECTED
	vidGrabber.initGrabber(width, height);
#else
	vidPlayer.loadMovie("marker.mov");
	vidPlayer.play();
	vidPlayer.setLoopState(OF_LOOP_NORMAL);
#endif
	
	colorImage.allocate(width, height);
	grayImage.allocate(width, height);
	grayThres.allocate(width, height);
	
	// Load the image we are going to distort
	displayImage.loadImage("of.jpg");
	// Load the corners of the image into the vector
	int displayImageHalfWidth = displayImage.width / 2;
	int displayImageHalfHeight = displayImage.height / 2;	
	displayImageCorners.push_back(ofPoint(0, 0));
	displayImageCorners.push_back(ofPoint(displayImage.width, 0));
	displayImageCorners.push_back(ofPoint(displayImage.width, displayImage.height));
	displayImageCorners.push_back(ofPoint(0, displayImage.height));	
	
	// This uses the default camera calibration and marker file
	artk.setup(width, height);

	// The camera calibration file can be created using GML:
	// http://graphics.cs.msu.ru/en/science/research/calibration/cpp
	// and these instructions:
	// http://studierstube.icg.tu-graz.ac.at/doc/pdf/Stb_CamCal.pdf
	// This only needs to be done once and will aid with detection
	// for the specific camera you are using
	// Put that file in the data folder and then call setup like so:
	// artk.setup(width, height, "myCamParamFile.cal", "markerboard_480-499.cfg");
	
	// Set the threshold
	// ARTK+ does the thresholding for us
	// We also do it in OpenCV so we can see what it looks like for debugging
    byn=false;
	bynThreshold = 85;
	artk.setThreshold(bynThreshold);

	record=false;
	recordBegin=0.;
	recordEnd=1.;
	recordBeginTime=0.;
	recordEndTime=0.;
	recordFbo.allocate(width,height);
	recordFbo.begin();
	ofClear(0,0);
	recordFbo.end();

    gui = new ofxUISuperCanvas("ARTracker", OFX_UI_FONT_MEDIUM);
    gui->addFPS();
#ifdef CAMERA_CONNECTED
    gui->addSpacer();
    gui->addLabelButton("Camera Settings",false);
#endif
    gui->addSpacer();
    gui->addTextArea("CONTROL", "Control de parametros de ARTRacker");
    gui->addSpacer();
    gui->addLabelToggle("B&N",&byn);
    gui->addSlider("B&N Threshold", 0, 255, &bynThreshold);
	gui->addSpacer();
    gui->addLabelToggle("Record",&record);
    gui->addRangeSlider("Record Limits", 0., 1., &recordBegin,&recordEnd);
	gui->addSpacer();
    gui->addLabelButton("Save",false);
    gui->autoSizeToFitWidgets();
    ofAddListener(gui->newGUIEvent,this,&ofApp::guiEvent);
    
    if(ofFile::doesFileExist("GUI/guiSettings.xml"))
        gui->loadSettings("GUI/guiSettings.xml");

	record=false;
	recordBegin=0.;
	recordEnd=1.;
	//
	( (ofxUIRangeSlider *) gui->getWidget("Record Limits") )->setValueLow(0.);
	( (ofxUIRangeSlider *) gui->getWidget("Record Limits") )->setValueHigh(1.);
	//
    
    ofEnableAlphaBlending();
    ofSetFrameRate(60);
    
    lastMarkersCheck = 0.;
}

//--------------------------------------------------------------
void ofApp::update(){
#ifdef CAMERA_CONNECTED
	vidGrabber.update();
	bool bNewFrame = vidGrabber.isFrameNew();
#else
	vidPlayer.update();
	bool bNewFrame = vidPlayer.isFrameNew();
#endif
	
	if(bNewFrame) {
		
#ifdef CAMERA_CONNECTED
		colorImage.setFromPixels(vidGrabber.getPixels(), width, height);
#else
		colorImage.setFromPixels(vidPlayer.getPixels(), width, height);
#endif
		
		// convert our camera image to grayscale
		grayImage = colorImage;
            
		// Pass in the new image pixels to artk
		artk.update(grayImage.getPixels());
        
        if(byn){
            // apply a threshold so we can see what is going on
            grayThres = grayImage;
            grayThres.threshold(bynThreshold);
        }
        
        // Find out how many markers have been detected
        int numDetected = artk.getNumDetectedMarkers();
        // Draw for each marker discovered
        for(int i=0; i<numDetected; i++) {
            int artkID = artk.getMarkerID(i);

            ARMarker * marker = NULL;
            for(int j=0; j<markers.size(); j++){
                if(markers[j].ID==artkID){
                    marker=&markers[j];
                    break;
                }
            }
            if(marker==NULL){
                markers.push_back(ARMarker());
                marker = &markers[markers.size()-1];
            }
            marker->ID=artkID;
			marker->lastTime=ofGetElapsedTimef();
			marker->lastCenter=artk.getDetectedMarkerCenter(i);
			vector<ofPoint> corners;
			artk.getDetectedMarkerOrderedCorners(i,corners);
			for(int j=0;j<CORNERS;j++){
				marker->lastCorner[j]=corners[j];
			}
			if(record){
				marker->time.push_back(marker->lastTime);
				marker->center.push_back(marker->lastCenter);
				for(int j=0;j<CORNERS;j++){
					marker->corner[j].push_back(marker->lastCorner[j]);
				}

				recordFbo.begin();
				ofSetColor(0,255,0);
				ofCircle(marker->center.back(),5);
				recordFbo.end();
			}
        }
	}
    
	if(record){
		if(ofGetElapsedTimef()>(lastMarkersCheck+1.)){
			for(int i=markers.size()-1; i>=0; i--){
				if(ofGetElapsedTimef()>(markers[i].lastTime+2.))
					markers.erase(markers.begin()+i);
			}
			lastMarkersCheck=ofGetElapsedTimef();
		}
	}
}

//--------------------------------------------------------------
void ofApp::draw(){
	
	// Main image
	ofSetHexColor(0xffffff);
    if(byn)
        grayThres.draw(0, 0);
    else
        colorImage.draw(0, 0);	
	ofDrawBitmapString(ofToString(artk.getNumDetectedMarkers()) + " marker(s) found", 10, 20);

	// ARTK draw
	// An easy was to see what is going on
	// Draws the marker location and id number
	artk.draw(0, 0);
	
	// ARTK 2D stuff
	// See if marker ID '0' was detected
	// and draw blue corners on that marker only
	int myIndex = artk.getMarkerIndex(0);
	if(myIndex >= 0) {	
		// Get the corners
		vector<ofPoint> corners;
		artk.getDetectedMarkerBorderCorners(myIndex, corners);
		// Can also get the center like this:
		// ofPoint center = artk.getDetectedMarkerCenter(myIndex);
		ofSetHexColor(0x0000ff);
		for(int i=0;i<corners.size();i++) {
			ofCircle(corners[i].x, corners[i].y, 10);
		}
        
        // Homography
        // Here we feed in the corners of an image and get back a homography matrix
        ofMatrix4x4 homo = artk.getHomography(myIndex, displayImageCorners);
        // We apply the matrix and then can draw the image distorted on to the marker
        ofPushMatrix();
        glMultMatrixf(homo.getPtr());
        ofSetHexColor(0xffffff);
        displayImage.draw(0, 0);
        ofPopMatrix();
	}
	ofPushStyle();
    ofPushView();
    ofPushMatrix();
	// ARTK 3D stuff
	// This is another way of drawing objects aligned with the marker
	// First apply the projection matrix once
	artk.applyProjectionMatrix();
	// Find out how many markers have been detected
	int numDetected = artk.getNumDetectedMarkers();
	// Draw for each marker discovered
	for(int i=0; i<numDetected; i++) {
		// Set the matrix to the perspective of this marker
		// The origin is in the middle of the marker	
		artk.applyModelMatrix(i);		
		// Draw a line from the center out
		ofNoFill();
		ofSetLineWidth(5);
		ofSetHexColor(0xffffff);
//		glBegin(GL_LINES);
//		glVertex3f(0, 0, 0); 
//		glVertex3f(0, 0, 50);
//		glEnd();
		// Draw a stack of rectangles by offseting on the z axis
		ofNoFill();
		ofEnableSmoothing();
		ofSetColor(255, 255, 0, 50);	
		for(int i=0; i<10; i++) {		
			ofRect(-25, -25, 50, 50);
			ofTranslate(0, 0, i*1);
		}
	}
    ofPopMatrix();
    ofPopView();
    ofPopStyle();

	ofSetColor(255,255);
	recordFbo.draw(0,0);

	ofFill();
	ofSetColor(0,125,125);
    for(int i=0;i<markers.size();i++){
		for(int j=0;j<CORNERS;j++){
			ofCircle(markers[i].lastCorner[j],15);
		}
    }
	ofSetColor(0,255,255);
    for(int i=0;i<markers.size();i++){
        ofDrawBitmapString(ofToString(markers[i].ID),markers[i].lastCenter);
    }
}

//--------------------------------------------------------------
void ofApp::guiEvent(ofxUIEventArgs &e){
    ofxUIWidget* widget=e.widget;
    if(widget->getName()=="B&N Threshold"){
        artk.setThreshold(bynThreshold);
    }
	else if(widget->getName()=="Record"){
		if(record){
			for(int i=0;i<markers.size();i++){
				markers[i].center.clear();
				for(int j=0;j<CORNERS;j++){
					markers[i].corner[j].clear();
				}
				markers[i].time.clear();
			}
			recordBegin=0.;
			recordEnd=1.;
			//
			( (ofxUIRangeSlider *) gui->getWidget("Record Limits") )->setValueLow(0.);
			( (ofxUIRangeSlider *) gui->getWidget("Record Limits") )->setValueHigh(1.);
			//
			recordBeginTime=ofGetElapsedTimef();
			recordEndTime=ofGetElapsedTimef();
			recordFbo.begin();
			ofClear(0,0);
			recordFbo.end();
		}
		else{
			recordEndTime=ofGetElapsedTimef();
		}
    }
	else if(widget->getName()=="Record Limits"){
        if(record){
			recordBegin=0.;
			recordEnd=1.;
		}
		else{
			float timeBegin=recordBeginTime+recordBegin*(recordEndTime-recordBeginTime);
			float timeEnd=recordBeginTime+recordEnd*(recordEndTime-recordBeginTime);
			recordFbo.begin();
			ofClear(0,0);
			for(int i=0;i<markers.size();i++){
				for(int j=0;j<markers[i].time.size();j++){
					if(markers[i].time[j]>timeBegin && markers[i].time[j]<timeEnd)
						ofSetColor(0,255,0);
					else
						ofSetColor(255,0,0);
					ofCircle(markers[i].center[j],5);
				}
			}
			recordFbo.end();
		}
    }
	else if(widget->getName()=="Save"){
		if(record){
			recordEndTime=ofGetElapsedTimef();
			record=false;
		}
		ofxUILabelButton * lb = (ofxUILabelButton *)widget;
        if(!lb->getValue()){
            save();
        }
    }
#ifdef CAMERA_CONNECTED
    if(widget->getName()=="Camera Settings"){
        ofxUILabelButton * lb = (ofxUILabelButton *)widget;
        if(lb->getValue()){
            vidGrabber.videoSettings();
        }
    }
#endif
}

//--------------------------------------------------------------
void ofApp::save(){
	if(markers.size()){
		ofFileDialogResult saveFileResult = ofSystemSaveDialog(ofGetTimestampString() + ".csv", "Save your file");
		if (saveFileResult.bSuccess){
			float timeBegin=recordBeginTime+recordBegin*(recordEndTime-recordBeginTime);
			float timeEnd=recordBeginTime+recordEnd*(recordEndTime-recordBeginTime);
			for(int i=0;i<markers.size();i++){
				ofBuffer buffer;
				for(int j=0;j<markers[i].time.size();j++){
					if(markers[i].time[j]>timeBegin && markers[i].time[j]<timeEnd){
						buffer.append(ofToString(markers[i].time[j]-timeBegin));
						buffer.append(",");
						buffer.append(ofToString(markers[i].center[j]));
						for(int k=0;k<CORNERS;k++){
							buffer.append(",");
							buffer.append(ofToString((markers[i].corner[k])[j]));
						}
						buffer.append("\n");
					}
				}
				//ofBufferToFile(ofToString(markers[i].ID)+"_"+saveFileResult.filePath,buffer);
				ofBufferToFile(saveFileResult.filePath,buffer);
				ofLogNotice()<<"Data saved to: "<<saveFileResult.filePath;
			}
		}
	}
	else{
		ofLogNotice()<<"No marker data available to save.";
	}
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::exit(){
    gui->saveSettings("GUI/guiSettings.xml");
    delete gui;
}