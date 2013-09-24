#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
	ofSetLogLevel(OF_LOG_VERBOSE);
	width = WIDTH;
	height = HEIGHT;
	
	// Print the markers from the "AllBchThinMarkers.png" file in the data folder
#ifdef CAMERA_CONNECTED
	vidGrabber.setVerbose(true);
	vidGrabber.listDevices();
	vidGrabber.setDeviceID(0);
	vidGrabber.setDesiredFrameRate(30);
	vidGrabber.initGrabber(width, height);
#else
	vidPlayer.loadMovie("marker.mov");
	vidPlayer.play();
	vidPlayer.setLoopState(OF_LOOP_NORMAL);
#endif
	
	colorImage.allocate(width, height);
	grayImage.allocate(width, height);
	grayThres.allocate(width, height);
	
	/*
	// Load the image we are going to distort
	displayImage.loadImage("of.jpg");
	// Load the corners of the image into the vector
	int displayImageHalfWidth = displayImage.width / 2;
	int displayImageHalfHeight = displayImage.height / 2;	
	displayImageCorners.push_back(ofPoint(0, 0));
	displayImageCorners.push_back(ofPoint(displayImage.width, 0));
	displayImageCorners.push_back(ofPoint(displayImage.width, displayImage.height));
	displayImageCorners.push_back(ofPoint(0, displayImage.height));
	*/
	
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
	saveBegin=0.;
	saveEnd=1.;
	recordFbo.allocate(width,height);
	recordFbo.begin();
	ofClear(0,0);
	recordFbo.end();
	recordTimeLabel=0.;

	grid=false;
	gridFbo.allocate(width,height);
	gridRows=2;
	gridCols=2;
	for(int i=0;i<CORNERS;i++){
		gridCorners[i].set(0,0);
	}
	gridCornerGrabbed=0;

    controlGUI = new ofxUISuperCanvas("CONTROL", OFX_UI_FONT_MEDIUM);
    controlGUI->addFPS();
#ifdef CAMERA_CONNECTED
    controlGUI->addSpacer();
    controlGUI->addLabelButton("Camera Settings",false);
#endif
    controlGUI->addSpacer();
    controlGUI->addTextArea("controlGUIInstructions", "Control de parametros de ARTRacker");
    controlGUI->addSpacer();
    controlGUI->addLabelToggle("B&N",&byn);
    controlGUI->addSlider("B&N Threshold", 0, 255, &bynThreshold);
	controlGUI->addSpacer();
	controlGUI->addLabelToggle("Grid",&grid);
	controlGUI->addLabelToggle("Grid Corners", &gridOpen);
	controlGUI->addSlider("Grid Rows", 1, 30, &gridRows);
	controlGUI->addSlider("Grid Columns", 1, 30, &gridCols);
	controlGUI->addSpacer();
    controlGUI->addLabelToggle("Record",&record);
	controlGUI->addLabel("Record Time","Time: " + ofToString(recordTimeLabel,1) + " seg");
    controlGUI->autoSizeToFitWidgets();
    ofAddListener(controlGUI->newGUIEvent,this,&ofApp::controlGUIEvent);

	recordGUI = new ofxUISuperCanvas("GRABACIÓN", OFX_UI_FONT_MEDIUM);
	recordGUI->setPosition(0,height-120);
	recordGUI->setWidth(width);
	recordGUI->addSpacer();
	recordGUI->addTextArea("recordGUIInstructions", "Indique el fragmento de tiempo que desea guardar.");
	recordGUI->addSpacer();
	recordGUI->addRangeSlider("Record Limits", 0.,1., &saveBegin,&saveEnd);
	recordGUI->addSpacer();
	recordGUI->addLabelButton("Save",false);
	ofAddListener(recordGUI->newGUIEvent,this,&ofApp::recordGUIEvent);

	if(ofFile::doesFileExist("GUI/gridCorners.points")){
        ofBuffer buf=ofBufferFromFile("GUI/gridCorners.points");
        int i=0;
        //Read file line by line
        while (!buf.isLastLine() && i<CORNERS) {
            string line = buf.getNextLine();
            //Split line into strings
            vector<string> p = ofSplitString(line, ",");
            gridCorners[i++].set(ofToInt(p[0]),ofToInt(p[1]));
        }
	}

	if(ofFile::doesFileExist("GUI/guiSettings.xml"))
        controlGUI->loadSettings("GUI/guiSettings.xml");

	recordGUI->disable();
	recordGUI->setVisible(false);

	record=false;
    
    ofEnableAlphaBlending();
    ofSetFrameRate(30);
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
        float time=ofGetElapsedTimef();
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
			marker->lastTime=time;
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

		if(record){
			if((time-recordTimeLabel)>0.1){
				recordTimeLabel=time;
				( (ofxUILabel *) controlGUI->getWidget("Record Time") )->setLabel("Time: " + ofToString(recordTimeLabel-recordBegin,1) + " seg");
			}
		}
	}

	if(saveAlpha){
		saveAlpha-=2;
		if(saveAlpha<0)
			saveAlpha=0;
	}
}

//--------------------------------------------------------------
void ofApp::draw(){
	
	// Main image
	ofSetColor(255);
    if(byn)
        grayThres.draw(0, 0);
    else
        colorImage.draw(0, 0);
	
	if(grid)
		gridFbo.draw(0,0);

	ofDrawBitmapString(ofToString(artk.getNumDetectedMarkers()) + " marker(s) found", 10, 20);

	// ARTK draw
	// An easy was to see what is going on
	// Draws the marker location and id number
	artk.draw(0, 0);
	
	/*
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
	*/

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

	if(saveAlpha){
		ofSetColor(255,saveAlpha);
		ofDrawBitmapString("Archivo guardado: " + saveFile, 10, 40);
	}
}

//--------------------------------------------------------------
void ofApp::controlGUIEvent(ofxUIEventArgs &e){
    ofxUIWidget* widget=e.widget;
    if(widget->getName()=="B&N Threshold"){
        artk.setThreshold(bynThreshold);
    }
	else if(widget->getName()=="Record"){
		if(record){
			for(int i=markers.size()-1; i>=0; i--){
				if(ofGetElapsedTimef()>(markers[i].lastTime+2.)){
					markers.erase(markers.begin()+i);
				}
				else{
					markers[i].center.clear();
					for(int j=0;j<CORNERS;j++){
						markers[i].corner[j].clear();
					}
					markers[i].time.clear();
				}
			}
			recordGUI->disable();
			recordGUI->setVisible(false);
			recordBegin=ofGetElapsedTimef();
			recordEnd=ofGetElapsedTimef();
			recordTimeLabel=0.;
			( (ofxUILabel *) controlGUI->getWidget("Record Time") )->setLabel("Time: " + ofToString(recordTimeLabel,1) + " seg");
			recordFbo.begin();
			ofClear(0,0);
			recordFbo.end();
		}
		else{
			recordEnd=ofGetElapsedTimef();
			recordGUI->setVisible(true);
			recordGUI->enable();
			//
			( (ofxUIRangeSlider *) recordGUI->getWidget("Record Limits") )->setMaxAndMin(recordEnd-recordBegin,0.);
			//
			saveBegin=0.;
			saveEnd=recordEnd-recordBegin;
			//
			( (ofxUIRangeSlider *) recordGUI->getWidget("Record Limits") )->setValueLow(saveBegin);
			( (ofxUIRangeSlider *) recordGUI->getWidget("Record Limits") )->setValueHigh(saveEnd);
			//
		}
    }
	else if(widget->getName()=="Grid" || widget->getName()=="Grid Rows" || widget->getName()=="Grid Columns"){
		drawGrid();
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
void ofApp::recordGUIEvent(ofxUIEventArgs &e){
    ofxUIWidget* widget=e.widget;
	if(widget->getName()=="Record Limits"){
        if(record){
			saveBegin=0.;
			saveEnd=recordEnd-recordBegin;
		}
		else{
			float timeBegin=saveBegin+recordBegin;
			float timeEnd=saveEnd+recordBegin;
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
			recordEnd=ofGetElapsedTimef();
			saveBegin=0.;
			saveEnd=recordEnd-recordBegin;
			record=false;
		}
		ofxUILabelButton * lb = (ofxUILabelButton *)widget;
        if(!lb->getValue()){
            save();
        }
    }
}

//--------------------------------------------------------------
void ofApp::save(){
	if(markers.size()){
		ofFileDialogResult saveFileResult = ofSystemSaveDialog(ofGetTimestampString() + ".csv", "Save your file");
		if (saveFileResult.bSuccess){
			saveFile=ofFilePath::getFileName(saveFileResult.filePath,true);
			float timeBegin=saveBegin+recordBegin;
			float timeEnd=saveEnd+recordBegin;
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
				//ofBufferToFile(ofToString(markers[i].ID)+"_"+saveFile,buffer);
				ofBufferToFile(saveFile,buffer);
				ofLogNotice()<<"Data saved to: "<<saveFileResult.filePath;
			}
			saveAlpha=500;
		}
	}
	else{
		ofLogNotice()<<"No marker data available to save.";
	}
}

//--------------------------------------------------------------
void ofApp::drawGrid(){
	gridFbo.begin();
	ofClear(0,0);
	ofSetColor(0,0,255,200);
	ofSetLineWidth(2);
	for(int x=0;x<gridCols;x++){
		float pct=(float)x/gridCols;
		ofLine(gridCorners[0]+pct*(gridCorners[1]-gridCorners[0]),gridCorners[3]+pct*(gridCorners[2]-gridCorners[3]));
		if((x%5)==0){
			ofCircle(gridCorners[0]+pct*(gridCorners[1]-gridCorners[0]),3);
			ofCircle(gridCorners[3]+pct*(gridCorners[2]-gridCorners[3]),3);
		}
	}
	for(int y=0;y<gridRows;y++){
		float pct=(float)y/gridRows;
		ofLine(gridCorners[0]+pct*(gridCorners[3]-gridCorners[0]),gridCorners[1]+pct*(gridCorners[2]-gridCorners[1]));
		if((y%5)==0){
			ofCircle(gridCorners[0]+pct*(gridCorners[3]-gridCorners[0]),3);
			ofCircle(gridCorners[1]+pct*(gridCorners[2]-gridCorners[1]),3);
		}
	}
	ofLine(gridCorners[1],gridCorners[2]);
	ofLine(gridCorners[2],gridCorners[3]);
	ofSetColor(0,0,255,240);
	for(int i=0;i<CORNERS;i++){
		ofCircle(gridCorners[i],4);
	}
	gridFbo.end();
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
	ofxUIRectangle * guiWindow = controlGUI->getRect();
    if(gridOpen && !guiWindow->inside(x,y)){
		gridCorners[gridCornerGrabbed].set(x,y);
		drawGrid();
	}
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
	ofxUIRectangle * guiWindow = controlGUI->getRect();
    if(gridOpen && !guiWindow->inside(x,y)){
		ofPoint mouse(x,y);
		for(int i=0;i<CORNERS;i++){
			if(gridCorners[i].distanceSquared(mouse)<gridCorners[gridCornerGrabbed].distanceSquared(mouse))
				gridCornerGrabbed=i;
		}
		gridCorners[gridCornerGrabbed].set(x,y);
		drawGrid();
	}
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::exit(){
	ofBuffer buf;
    for(int i=0;i<CORNERS;i++){
        buf.append(ofToString(gridCorners[i])+"\n");
    }
    ofBufferToFile("GUI/gridCorners.points",buf);

    controlGUI->saveSettings("GUI/guiSettings.xml");
    delete controlGUI;
	delete recordGUI;
}