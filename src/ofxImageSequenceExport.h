//
//  ofxImageSequenceExport.h
//  BasicSketch
//
//  Created by Oriol Ferrer Mesià on 07/02/2018.
//
//

#pragma once
#include "ofMain.h"
#include <future>

class ofxImageSequenceExport{

	const string fileNamePattern = "%08i"; //8 digits
public:
	
	ofxImageSequenceExport();

	void setup(int width, int height, string fileExtension = "png", GLint internalformat = GL_RGB, int num_samples = 0);

	void begin();
	void begin(ofCamera& cam);
	void end();

	void draw();


	void startExport();
	bool isExporting();
	void stopExport();

	void setExportDir(string dir);
	void setMaxThreads(int t);
	void setMaxPendingTasks(int m);
	void update();

	int getNumPendingJobs();
	
protected:

	ofFbo::Settings fboSettings;
	ofFbo fbo;
	ofCamera* cam_ptr = NULL;

	struct ExportState{
		bool exporting;
		string exportFolder = "ImgSequenceExport";
		string fileExtension;
		int exportedFrameCounter;
		int maxThreads = std::thread::hardware_concurrency();
		int maxPending = std::thread::hardware_concurrency() * 2;
		float avgExportTime = -1;
	};

	struct ExportJob{
		ofPixels * pixels;
		int frameID;
		string fileName;
		float runTime = -1;
	};

	ExportState state;

	vector<ExportJob> pendingJobs;
	vector<std::future<float>> tasks;

	std::string fileNameForFrame(int frame);

	float runJob(ExportJob j);
	void updateTasks();

};

