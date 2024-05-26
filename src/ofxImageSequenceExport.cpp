//
//  ofxImageSequenceExport.cpp
//  BasicSketch
//
//  Created by Oriol Ferrer Mesià on 07/02/2018.
//
//

#include "ofxImageSequenceExport.h"
#include <sys/stat.h>

ofxImageSequenceExport::ofxImageSequenceExport(){}

ofxImageSequenceExport::~ofxImageSequenceExport(){
	stopExportAndWait();
}

void ofxImageSequenceExport::stopExportAndWait(){
	if(state.exporting){
		ofLogNotice("ofxImageSequenceExport") << "waiting for threads to finish...";
		stopExport();
		while (tasks.size()) {
			updateTasks();
		}
	}
}


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

void ofxImageSequenceExport::addFrame(ofFbo & fbo){

	if(state.exporting){

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


void ofxImageSequenceExport::startExport(int nFrames){
	if(!state.exporting){
		state.exporting = true;
		state.exportedFrameCounter = 0;
		state.avgExportTime = -1;
		state.avgFileSize = -1;
		state.expectedRenderLen = nFrames;
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
	state.avgExportTime = -1;
	state.avgFileSize = -1;
	state.expectedRenderLen = -1;
}

int ofxImageSequenceExport::getNumPendingJobs(){
	return pendingJobs.size();
}


void ofxImageSequenceExport::update(){

	updateTasks();
	int nSleeps = 0;

	int numSleepMS = 20;
	int maxSleepTimeMS = 500;
	//throttle down export a bit if too many jobs pending
	while(pendingJobs.size() > state.maxPending && nSleeps * numSleepMS < maxSleepTimeMS){
		ofSleepMillis(numSleepMS);
		updateTasks();
		nSleeps++;
	}

	if(nSleeps > 0){
		ofLogVerbose("ofxImageSequenceExport") << "too many pending tasks! blocked update() for " << numSleepMS * nSleeps << " ms";
	}
}


void ofxImageSequenceExport::updateTasks(){

	//check for finished tasks
	for(int i = tasks.size() - 1; i >= 0; i--){
		std::future_status status = tasks[i].wait_for(std::chrono::microseconds(0));
		if(status == std::future_status::ready){
			auto job = tasks[i].get();
			if(job.runTime > 0.0f){
				if(state.avgExportTime < 0){
					state.avgExportTime = job.runTime;
				}else{
					state.avgExportTime = ofLerp(state.avgExportTime, job.runTime, 0.1);
				}
			}

			if(job.fileSizeBytes > 0){
				float fileSize = float(job.fileSizeBytes) / float(1024 * 1024);
				if(state.avgFileSize < 0){
					state.avgFileSize = fileSize;
				}else{
					state.avgFileSize = ofLerp(state.avgFileSize, fileSize, 0.1);
				}
			}
			tasks.erase(tasks.begin() + i);
		}
	}

	//spawn new jobs if pending
	vector<size_t> spawnedJobs;
	for(int i = 0; i < pendingJobs.size(); i++){
		if(tasks.size() < state.maxThreads){
			try{
				tasks.push_back( std::async(std::launch::async, &ofxImageSequenceExport::runJob, this, pendingJobs[i]));
			}catch(std::exception e){
				ofLogError("ofxImageSequenceExport") << "Exception at async() " <<  e.what();
			}
			spawnedJobs.push_back(i);
		}else{
			break;
		}
	}

	//removed newly spawned jobs
	for(int i = spawnedJobs.size() - 1; i >= 0; i--){
		pendingJobs.erase(pendingJobs.begin() + spawnedJobs[i]);
	}
}

string ofxImageSequenceExport::getStatus(){

	string msg = "### ofxImageSequenceExport####\nExporting!\nExported Frames: " + ofToString(state.exportedFrameCounter) +
	"\nExport Queue Length: " + ofToString(pendingJobs.size()) + " ( max queue length is " + ofToString(state.maxPending) + ")" +
	"\nCurrently Executing Jobs: " + ofToString(tasks.size()) + " ( max of " + ofToString(state.maxThreads) + " concurrent jobs )";
	if (state.avgExportTime > 0){
		msg += "\nAvg. Frame Export Time: " + ofToString(state.avgExportTime * 1000, 1) + " ms";
	}
	if (state.avgFileSize > 0){
		msg += "\nAvg. Frame File Size: " + ofxImageSequenceExport::bytesToHumanReadable(state.avgFileSize * 1024 * 1024, 1);
	}
	if(state.expectedRenderLen > 0 && state.avgFileSize > 0){
		msg += "\nEstimated Total Sequence File Size: " + ofxImageSequenceExport::bytesToHumanReadable(state.expectedRenderLen * state.avgFileSize * 1024 * 1024 , 1);
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


ofxImageSequenceExport::ExportJob ofxImageSequenceExport::runJob(ExportJob j){

	#ifdef TARGET_WIN32
	#elif defined(TARGET_LINUX)
	pthread_setname_np(pthread_self(), ("ofxImageSequenceExport job " + ofToString(j.frameID)).c_str());
	#else
	pthread_setname_np(("ofxImageSequenceExport job " + ofToString(j.frameID)).c_str());
	#endif

	bool timeSample = (j.frameID%30 == 0);
	float t;
	if(timeSample) t = ofGetElapsedTimef();
	try{
		ofSaveImage(*j.pixels, j.fileName);
	}catch(std::exception e){
		ofLogError("ofxImageSequenceExport") << "Exception in ofSaveImage()! " << e.what();
	}
	if(timeSample){
		j.runTime = (ofGetElapsedTimef() - t);
		//check file size too
		struct stat stat_buf;
		int rc = stat(ofToDataPath(j.fileName,true).c_str(), &stat_buf);
		j.fileSizeBytes = rc == 0 ? stat_buf.st_size : -1;
	}
	delete j.pixels;
	j.pixels = nullptr;
	return j;
}


std::string ofxImageSequenceExport::bytesToHumanReadable(long long bytes, int decimalPrecision){
	std::string ret;
	if (bytes < 1024 ){ //if in bytes range
		ret = ofToString(bytes) + " bytes";
	}else{
		if (bytes < 1024 * 1024){ //if in kb range
			ret = ofToString(bytes / float(1024), decimalPrecision) + " KB";
		}else{
			if (bytes < (1024 * 1024 * 1024)){ //if in Mb range
				ret = ofToString(bytes / float(1024 * 1024), decimalPrecision) + " MB";
			}else{
				ret = ofToString(bytes / float(1024 * 1024 * 1024), decimalPrecision) + " GB";
			}
		}
	}
	return ret;
}
