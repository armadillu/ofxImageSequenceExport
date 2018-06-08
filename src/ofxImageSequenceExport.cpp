//
//  ofxImageSequenceExport.cpp
//  BasicSketch
//
//  Created by Oriol Ferrer Mesià on 07/02/2018.
//
//

#include "ofxImageSequenceExport.h"

ofxImageSequenceExport::ofxImageSequenceExport(){}


void ofxImageSequenceExport::setup(int width, int height, string fileExtension, GLint internalformat, int num_samples){

	ofFbo::Settings s;
	s.width = width;
	s.height = height;
	s.internalformat = internalformat;
	s.numSamples = num_samples;
	s.useDepth = true;

	setup(s, fileExtension);
}

void ofxImageSequenceExport::setup(const ofFbo::Settings & settings, const string & fileExtension){

	fboSettings = settings;
	fbo.allocate(settings);

	fbo.begin();
	ofClear(0);
	fbo.end();

	state.exporting = false;
	state.fileExtension = fileExtension;
}


void ofxImageSequenceExport::setMaxThreads(int t){
	state.maxThreads = MAX(t,1);
}

void ofxImageSequenceExport::setMaxPendingTasks(int t){
	state.maxPending = MAX(t,1);
}

void ofxImageSequenceExport::setExportDir(string dir){
	state.exportFolder = dir;
}

void ofxImageSequenceExport::begin(ofCamera& cam){
	if(state.exporting){
		ofPushView();
		fbo.begin();
		cam_ptr = &cam;
		cam_ptr->begin(ofRectangle(0, 0, fboSettings.width, fboSettings.height));
	}
}


void ofxImageSequenceExport::begin(){
	if(state.exporting){
		ofPushView();
		fbo.begin();
		ofViewport(0, 0, fboSettings.width, fboSettings.height);
		cam_ptr = NULL;
	}
}


void ofxImageSequenceExport::end(){

	if(state.exporting){

		if (cam_ptr){
			cam_ptr->end();
			cam_ptr = NULL;
		}

		fbo.end();
		ofPopView();

		ExportJob job;
		job.pixels = new ofPixels();
		fbo.readToPixels(*job.pixels);
		job.frameID = state.exportedFrameCounter;
		job.fileName = fileNameForFrame(job.frameID);
		state.exportedFrameCounter++;

		pendingJobs.push_back(job);
	}
}


std::string ofxImageSequenceExport::fileNameForFrame(int frame){
	char buf[1024];
	sprintf(buf, fileNamePattern.c_str(), frame);
	return state.exportFolder + "/" + "frame_" + string(buf) + "." + state.fileExtension;
}


void ofxImageSequenceExport::startExport(){
	if(!state.exporting){
		state.exporting = true;
		state.exportedFrameCounter = 0;
		state.avgExportTime = -1;
		if(ofDirectory::doesDirectoryExist(state.exportFolder)){
			ofDirectory::removeDirectory(state.exportFolder, true);
		}
		ofDirectory::createDirectory(state.exportFolder, true, true);
	}else{
		ofLogError("ofxImageSequenceExport") << "can't startExport() bc we already are exporting!";
	}
}


bool ofxImageSequenceExport::isExporting(){
	return state.exporting;
}


void ofxImageSequenceExport::stopExport(){
	state.exporting = false;
}

int ofxImageSequenceExport::getNumPendingJobs(){
	return pendingJobs.size();
}


void ofxImageSequenceExport::update(){

	updateTasks();
	int nSleeps = 0;

	int numSleepMS = 5;
	//throttle down export a bit if too many jobs pending
	while(pendingJobs.size() > state.maxPending && nSleeps < 10){
		ofSleepMillis(numSleepMS);
		updateTasks();
		nSleeps++;
	}

	if(nSleeps > 0){
		ofLogWarning("ofxImageSequenceExport") << "too many pending tasks! blocked update() for " << numSleepMS * nSleeps << " ms";
	}
}


void ofxImageSequenceExport::updateTasks(){

	//check for finished tasks
	for(int i = tasks.size() - 1; i >= 0; i--){
		std::future_status status = tasks[i].wait_for(std::chrono::microseconds(0));
		if(status == std::future_status::ready){
			float runTime = tasks[i].get();
			if(runTime > 0.0f){
				if(state.avgExportTime < 0){
					state.avgExportTime = runTime;
				}else{
					state.avgExportTime = ofLerp(state.avgExportTime, runTime, 0.15);
				}
			}
			tasks.erase(tasks.begin() + i);
		}
	}

	//spawn new jobs if pending
	vector<size_t> spawnedJobs;
	for(int i = 0; i < pendingJobs.size(); i++){
		if(tasks.size() < state.maxThreads){
			tasks.push_back( std::async(std::launch::async, &ofxImageSequenceExport::runJob, this, pendingJobs[i]));
			spawnedJobs.push_back(i);
		}else{
			break;
		}
	}

	//if(spawnedJobs.size()) ofLogNotice("ofxImageSequenceExport") << "spwned " << spawnedJobs.size() << " jobs";

	//removed newly spawned jobs
	for(int i = spawnedJobs.size() - 1; i >= 0; i--){
		pendingJobs.erase(pendingJobs.begin() + spawnedJobs[i]);
	}
}

string ofxImageSequenceExport::getStatus(){

	string msg = "### ofxImageSequenceExport####\nExporting!\nExported Frames: " + ofToString(state.exportedFrameCounter) +
	"\nPending Export Jobs: " + ofToString(pendingJobs.size()) +
	"\nCurrent Threads: " + ofToString(tasks.size());
	if (state.avgExportTime > 0){
		msg += "\nAvg Frame Export Time: " + ofToString(state.avgExportTime * 1000, 1) + " ms.";
	}
	return msg;
}

void ofxImageSequenceExport::drawStatus(int x, int y){
	if(state.exporting || tasks.size() || pendingJobs.size()){
		ofDrawBitmapStringHighlight(getStatus(), glm::vec2(x, y), ofColor::red, ofColor::black);
	}
}

void ofxImageSequenceExport::draw(){

	if(state.exporting || tasks.size() || pendingJobs.size()){
		if(state.exporting){
			fbo.draw(0,0);
		}
		ofDrawBitmapStringHighlight(getStatus(), glm::vec2(20, 20), ofColor::red, ofColor::black);
	}
}


float ofxImageSequenceExport::runJob(ExportJob j){
	bool timeSample = (j.frameID%30 == 0);
	float t;
	if(timeSample) t = ofGetElapsedTimef();
	ofSaveImage(*j.pixels, j.fileName);
	if(timeSample){
		j.runTime = (ofGetElapsedTimef() - t);
	}
	delete j.pixels;
	return j.runTime;
}


