#pragma once

#include "ofMain.h"
#include "ImageSequenceExport.h"

class ofApp : public ofBaseApp{

public:
	void setup();
	void update();
	void draw();

	void keyPressed(int key);



	// APP SETUP ////////////////////////////////////////////
	float p1;

	ImageSequenceExport exp;

};
