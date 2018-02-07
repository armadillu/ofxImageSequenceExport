//
//  ImageSequenceExport.cpp
//  BasicSketch
//
//  Created by Oriol Ferrer Mesià on 07/02/2018.
//
//

#include "ImageSequenceExport.h"

ImageSequenceExport::ImageSequenceExport(){}


void ImageSequenceExport::setup(int width, int height, string fileExtension, GLint internalformat, int num_samples){

	fboSettings.width = width;
	fboSettings.height = height;
	fboSettings.internalformat = internalformat;
	fboSettings.numSamples = num_samples;
	fboSettings.useDepth = true;

	fbo.allocate(fboSettings);

	fbo.begin();
	ofClear(0);
	fbo.end();

	state.exporting = false;
	state.fileExtension = fileExtension;
}


void ImageSequenceExport::setNumThreads(int t){
	state.maxThreads = MAX(t,1);
}

void ImageSequenceExport::setMaxPendingTasks(int t){
	state.maxPending = MAX(t,1);
}


void ImageSequenceExport::begin(ofCamera& cam){
	if(state.exporting){
		ofPushView();
		fbo.begin();
		cam_ptr = &cam;
		cam_ptr->begin(ofRectangle(0, 0, fboSettings.width, fboSettings.height));
	}
}


void ImageSequenceExport::begin(){
	if(state.exporting){
		ofPushView();
		fbo.begin();
		ofViewport(0, 0, fboSettings.width, fboSettings.height);
		cam_ptr = NULL;
	}
}


void ImageSequenceExport::end(){

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


std::string ImageSequenceExport::fileNameForFrame(int frame){
	char buf[1024];
	sprintf(buf, fileNamePattern.c_str(), frame);
	return state.exportFolder + "/" + "frame_" + string(buf) + "." + state.fileExtension;
}


void ImageSequenceExport::startExport(string folder){
	if(!state.exporting){
		state.exporting = true;
		state.exportedFrameCounter = 0;
		state.exportFolder = folder;
		state.avgExportTime = -1;
		if(ofDirectory::doesDirectoryExist(folder)){
			ofDirectory::removeDirectory(folder, true);
		}
		ofDirectory::createDirectory(folder, true, true);
	}else{
		ofLogError("ImageSequenceExport") << "can't startExport() bc we already are exporting!";
	}
}


bool ImageSequenceExport::isExporting(){
	return state.exporting;
}


void ImageSequenceExport::stopExport(){
	state.exporting = false;
}

int ImageSequenceExport::getNumPendingJobs(){
	return pendingJobs.size();
}


void ImageSequenceExport::update(){

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
		ofLogWarning("ImageSequenceExport") << "too many pending tasks! blocked update() for " << numSleepMS * nSleeps << " ms";
	}
}


void ImageSequenceExport::updateTasks(){

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
			tasks.push_back( std::async(std::launch::async, &ImageSequenceExport::runJob, this, pendingJobs[i]));
			spawnedJobs.push_back(i);
		}else{
			break;
		}
	}

	//if(spawnedJobs.size()) ofLogNotice("ImageSequenceExport") << "spwned " << spawnedJobs.size() << " jobs";

	//removed newly spawned jobs
	for(int i = spawnedJobs.size() - 1; i >= 0; i--){
		pendingJobs.erase(pendingJobs.begin() + spawnedJobs[i]);
	}
}

void ImageSequenceExport::draw(){

	if(state.exporting || tasks.size() || pendingJobs.size()){
		if(state.exporting){
			fbo.draw(0,0);
		}
		string msg = "### ImageSequenceExport####\nExporting!\nExported Frames: " + ofToString(state.exportedFrameCounter) +
		"\nPending Export Jobs: " + ofToString(pendingJobs.size()) +
		"\nCurrent Threads: " + ofToString(tasks.size());
		if (state.avgExportTime > 0){
			msg += "\nAvg Frame Export Time: " + ofToString(state.avgExportTime * 1000, 1) + " ms.";
		}
		ofDrawBitmapStringHighlight(msg, glm::vec2(20, 20), ofColor::red, ofColor::black);
	}
}


float ImageSequenceExport::runJob(ExportJob j){
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


