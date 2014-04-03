#ifndef PRIMESENSETEST_H
#define PRIMESENSETEST_H

#include <QtWidgets/QMainWindow>
#include "ui_primesensetest.h"

#include "phgutils.h"
#include "OpenNI.h"

#include "StreamViewer.h"
#include <QTimer>
#include <QHBoxLayout>

class PrimeSenseTest : public QMainWindow
{
	Q_OBJECT

public:
	enum Mode{
		Separate = 0,	// separate color and depth images
		WarpDepth,		// warp depth to match color
		WarpColor,		// warp color to match depth
		ModeCount
	};

	enum SensorType {
		Kinect,
		PrimeSense
	};

	PrimeSenseTest(SensorType st = PrimeSense, QWidget *parent = 0);
	~PrimeSenseTest();

public slots:
	void startSensor();
	void takeScreenshot();
	void switchMode();
	bool updateStream();

protected:
	bool init(SensorType st);
	bool initializeSensor(const char* uri);
	bool initFrames();

	bool retrieveColorData();
	bool retrieveDepthData();

	void directMapColor();
	void directMapDepth();
	void warpDepthToColor();
	void warpColorToDepth();

private:
	Ui::PrimeSenseTestClass ui;


private:
	Mode m;

	openni::Device sensor;
	openni::VideoStream depthStream, colorStream;
	openni::VideoStream* streams[2];

	int width, height;
	openni::VideoFrameRef depthFrame, colorFrame;
	
	unsigned int nTexMapX, nTexMapY;
	vector<openni::RGB888Pixel> depthBuffer, colorBuffer;
	vector<unsigned char> depthValues, colorValues;

private:
	QTimer timer;
	StreamViewer* colorView;
	StreamViewer* depthView;
};

#endif // PRIMESENSETEST_H
