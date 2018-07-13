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
	void setup(const ofFbo::Settings & settings, const string & fileExtension);

	void begin();
	void begin(ofCamera& cam);
	void end();

	void draw();
	void drawStatus(int x, int y);
	string getStatus();
	ofFbo & getFbo(){return fbo;};

	void startExport(int nFrames = -1); //nFrames is only used to print guestimates of how much space you will need to store the sequence
										//no need to supply it at all - will not stop render when the # is reached either
	bool isExporting();
	void stopExport();

	void setExportDir(string dir);
	void setMaxThreads(int t);
	void setMaxPendingTasks(int m);
	void update(); //this call usually doesnt block, but it will when the queue is too long
					//so that bg threads can catch up

	int getNumPendingJobs();

	static std::string bytesToHumanReadable(long long bytes, int decimalPrecision);

protected:

	ofFbo::Settings fboSettings;
	ofFbo fbo;
	ofCamera* cam_ptr = NULL;

	struct ExportState{
		bool exporting;
		string exportFolder = "ImgSequenceExport";
		string fileExtension;
		int exportedFrameCounter;
		int maxThreads = std::max((int)std::thread::hardware_concurrency() - 1,(int)1);
		int maxPending = maxThreads;
		int expectedRenderLen = -1;
		float avgExportTime = -1;
		float avgFileSize = -1; //in MegaBytes!
	};

	struct ExportJob{
		ofPixels * pixels;
		int frameID;
		string fileName;
		float runTime = -1;
		long long fileSizeBytes = -1;
	};

	ExportState state;

	vector<ExportJob> pendingJobs;
	vector<std::future<ExportJob>> tasks;

	std::string fileNameForFrame(int frame);

	ExportJob runJob(ExportJob j);
	void updateTasks();

};

