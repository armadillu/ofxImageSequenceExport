#include "ofApp.h"


void ofApp::setup(){

	ofSetFrameRate(60);
	ofSetVerticalSync(true);
	ofEnableAlphaBlending();
	ofBackground(22);

	exp.setup(ofGetWidth(), ofGetHeight(), "tiff", GL_RGB, 4);
	exp.setMaxThreads(6);
}


void ofApp::update(){
	exp.update(); 	//this will block for a few millisecond if the pending exports queue gets too long
					//you can control this behavior by setting setMaxPendingTasks() to a very large number
					//the only risk is running out of memory, as every frame is stored in RAM in the queue.
					//Allowing more threads (with setMaxThreads()) migh speed up the processing too
}


void ofApp::draw(){

	exp.begin();

		ofClear(0);

		ofMesh m;
		m.setMode(OF_PRIMITIVE_POINTS);
		float t = ofGetElapsedTimef() * 0.03;

		ofSeedRandom(1);
		for(int i = 0; i < 3000; i++){
			m.addColor(ofColor(ofRandom(10, 255)));
			m.addVertex(glm::vec3(ofGetWidth() * ofNoise(i * 0.03, t), ofGetHeight() * ofNoise(t, i * 0.03), 0));
		}
		glPointSize(20);
		m.draw();

	exp.end();

	exp.draw();
}


void ofApp::keyPressed(int key){

	if(key == 'e'){
		if(exp.isExporting()){
			exp.stopExport();
		}else{
			exp.setExportDir("exportFolder");
			exp.startExport(); //this will overwrite - careful!
		}
	}
}

