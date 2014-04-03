#include "primesensetest.h"

#define GL_WIN_SIZE_X	1280
#define GL_WIN_SIZE_Y	1024
#define TEXTURE_SIZE	512

#define DEFAULT_DISPLAY_MODE	DISPLAY_MODE_DEPTH

#define MIN_NUM_CHUNKS(data_size, chunk_size)	((((data_size)-1) / (chunk_size) + 1))
#define MIN_CHUNKS_SIZE(data_size, chunk_size)	(MIN_NUM_CHUNKS(data_size, chunk_size) * (chunk_size))

PrimeSenseTest::PrimeSenseTest(SensorType st, QWidget *parent)
	: QMainWindow(parent),
	m(Separate)
{

	ui.setupUi(this);

	init(st);

	colorView = new StreamViewer(this);
	depthView = new StreamViewer(this);

	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->addWidget(colorView);
	layout->addWidget(depthView);

	ui.centralWidget->setLayout(layout);

	connect(ui.actionRun, SIGNAL(triggered()), this, SLOT(startSensor()));
	connect(ui.actionScreenshot, SIGNAL(triggered()), this, SLOT(takeScreenshot()));
	connect(ui.actionColor_Over, SIGNAL(triggered()), this, SLOT(switchMode()));
	connect(&timer, SIGNAL(timeout()), this, SLOT(updateStream()));
}

PrimeSenseTest::~PrimeSenseTest()
{
	openni::OpenNI::shutdown();
}

bool PrimeSenseTest::init(SensorType st) {
	string desiredVendor;
	switch( st ) {
	case Kinect:
		desiredVendor = "Microsoft";
		break;
	case PrimeSense:
		desiredVendor = "PrimeSense";
		break;
	}

	openni::Status rc = openni::STATUS_OK;

	// initialize openni
	rc = openni::OpenNI::initialize();
	cout << "After initialization:" << openni::OpenNI::getExtendedError() << endl;

	openni::Array<openni::DeviceInfo> devices;
	openni::OpenNI::enumerateDevices(&devices);
	
	int selectedDeviceIdx = -1;

	for(int i=0;i<devices.getSize();i++) {
		auto x = devices[i];
		cout << "Device #" << i << ":" << endl
			 << x.getName() << endl
			 << x.getUri() << endl
			 << x.getVendor() << endl
			 << x.getUsbProductId() << endl
			 << x.getUsbVendorId() << endl;
		cout << endl;

		if( strcmp(x.getVendor(), desiredVendor.c_str()) == 0 ) {
			selectedDeviceIdx = i;
			cout << "Selected device #" << i << endl;
			break;
		}
	}

	if( selectedDeviceIdx == -1 ) {
		cerr << "No supported device found. Abort." << endl;
#ifdef WIN32
		::system("pause");
#endif
		exit(-1);
	}
	else {
		auto x = devices[selectedDeviceIdx];
		cout << endl << "Selected device:" << endl
			<< x.getName() << endl
			<< x.getUri() << endl
			<< x.getVendor() << endl
			<< x.getUsbProductId() << endl
			<< x.getUsbVendorId() << endl;

		initializeSensor(x.getUri());
		initFrames();
	}
}

bool PrimeSenseTest::initializeSensor(const char* uri) {
	openni::Status rc = openni::STATUS_OK;

	rc = sensor.open(uri);
	if (rc != openni::STATUS_OK)
	{
		printf("PrimeSenseManager: Device open failed:\n%s\n", openni::OpenNI::getExtendedError());
		openni::OpenNI::shutdown();
		return false;
	}
	else cout << "Device opened." << endl;

	// get correct video mode for depth stream
	const openni::SensorInfo* dinfo = sensor.getSensorInfo(openni::SENSOR_DEPTH);
	const openni::Array<openni::VideoMode>& dmodes = dinfo->getSupportedVideoModes();
	openni::VideoMode dvm;
	for(int i=0;i<dmodes.getSize();i++) {
		auto x = dmodes[i];
		cout << x.getResolutionX() << "x" << x.getResolutionY() << " @ " << x.getFps() <<"fps, " << x.getPixelFormat() << endl;
		if( x.getResolutionX() == 640 && x.getResolutionY() == 480 && x.getFps() == 30 && x.getPixelFormat() == openni::PIXEL_FORMAT_DEPTH_1_MM ) dvm = x;
	}
	
	rc = depthStream.create(sensor, openni::SENSOR_DEPTH);
	if (rc == openni::STATUS_OK)
	{
		rc = depthStream.setVideoMode(dvm);
		if (rc != openni::STATUS_OK) cerr << "Failed to set video mode for depth stream." << endl;

		rc = depthStream.start();
		if (rc != openni::STATUS_OK)
		{
			printf("PrimeSenseManager: Couldn't start depth stream:\n%s\n", openni::OpenNI::getExtendedError());
			depthStream.destroy();
		}
	}
	else
	{
		printf("PrimeSenseManager: Couldn't find depth stream:\n%s\n", openni::OpenNI::getExtendedError());
	}

	const openni::SensorInfo* cinfo = sensor.getSensorInfo(openni::SENSOR_COLOR);
	const openni::Array<openni::VideoMode>& cmodes = cinfo->getSupportedVideoModes();
	openni::VideoMode cvm;
	for(int i=0;i<cmodes.getSize();i++) {
		auto x = cmodes[i];
		cout << x.getResolutionX() << "x" << x.getResolutionY() << " @ " << x.getFps() <<"fps, " << x.getPixelFormat() << endl;
		if( x.getResolutionX() == 640 && x.getResolutionY() == 480 && x.getFps() == 30 && x.getPixelFormat() == openni::PIXEL_FORMAT_RGB888 ) cvm = x;
	}
	rc = colorStream.create(sensor, openni::SENSOR_COLOR);
	if (rc == openni::STATUS_OK)
	{
		colorStream.setVideoMode(cvm);
		if (rc != openni::STATUS_OK) cerr << "Failed to set video mode for color stream." << endl;

		rc = colorStream.start();
		if (rc != openni::STATUS_OK)
		{
			printf("PrimeSenseManager: Couldn't start color stream:\n%s\n", openni::OpenNI::getExtendedError());
			colorStream.destroy();
		}
	}
	else
	{
		printf("PrimeSenseManager: Couldn't find color stream:\n%s\n", openni::OpenNI::getExtendedError());
	}

	sensor.setImageRegistrationMode(openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR);

	if (!depthStream.isValid() || !colorStream.isValid())
	{
		printf("PrimeSenseManager: No valid streams. Exiting\n");
		openni::OpenNI::shutdown();
		return false;
	}
	else return true;
}

bool PrimeSenseTest::initFrames() {
	openni::VideoMode depthVideoMode;
	openni::VideoMode colorVideoMode;

	if (depthStream.isValid() && colorStream.isValid())
	{
		depthVideoMode = depthStream.getVideoMode();
		colorVideoMode = colorStream.getVideoMode();

		int depthWidth = depthVideoMode.getResolutionX();
		int depthHeight = depthVideoMode.getResolutionY();
		int colorWidth = colorVideoMode.getResolutionX();
		int colorHeight = colorVideoMode.getResolutionY();

		if (depthWidth == colorWidth &&
			depthHeight == colorHeight)
		{
			width = depthWidth;
			height = depthHeight;

			cout << "Stream size = " << width << "x" << height << endl;
		}
		else
		{
			printf("Error - expect color and depth to be in same resolution: D: %dx%d, C: %dx%d\n",
				depthWidth, depthHeight,
				colorWidth, colorHeight);
			return false;
		}
	}
	else
	{
		printf("Error - expects both of the streams to be valid...\n");
		return false;
	}

	// Texture map init
	nTexMapX = MIN_CHUNKS_SIZE(width, TEXTURE_SIZE);
	nTexMapY = MIN_CHUNKS_SIZE(height, TEXTURE_SIZE);
	depthBuffer.resize(nTexMapX * nTexMapY);
	colorBuffer.resize(nTexMapX * nTexMapY);

	depthValues.resize(width*height*4);
	colorValues.resize(width*height*4);

	streams[0] = &colorStream;
	streams[1] = &depthStream;
}

bool PrimeSenseTest::retrieveColorData() {
	int readyIdx;
	openni::Status rc = openni::OpenNI::waitForAnyStream(streams, 2, &readyIdx);
	if (rc != openni::STATUS_OK)
	{
		printf("Wait failed\n");
		return false;
	}	

	rc = colorStream.readFrame(&colorFrame);
	return (rc == openni::STATUS_OK);
}

bool PrimeSenseTest::retrieveDepthData() {
	int readyIdx;
	openni::Status rc = openni::OpenNI::waitForAnyStream(streams, 2, &readyIdx);
	if (rc != openni::STATUS_OK)
	{
		printf("Wait failed\n");
		return false;
	}

	rc = depthStream.readFrame(&depthFrame);
	return (rc == openni::STATUS_OK);
}

bool PrimeSenseTest::updateStream() {
	if( !retrieveColorData() ) {
		throw "Unable to get color data!";
	}

	if( !retrieveDepthData() ) {
		throw "Unable to get depth data!";
	}

	switch( m ) {
	case Separate:
		directMapColor();
		directMapDepth();
		break;
	case WarpDepth:
		directMapColor();
		warpDepthToColor();
		break;
	case WarpColor:
		directMapDepth();
		warpColorToDepth();
		break;
	}

	colorView->bindStreamData(&colorValues[0]);
	depthView->bindStreamData(&depthValues[0]);	

	return true;
}

void PrimeSenseTest::directMapColor() {
	memset(&colorBuffer[0], 0, nTexMapX*nTexMapY*sizeof(openni::RGB888Pixel));

	const openni::RGB888Pixel* pImageRow = (const openni::RGB888Pixel*)colorFrame.getData();
	openni::RGB888Pixel* pTexRow = &colorBuffer[0] + colorFrame.getCropOriginY() * nTexMapX;
	int rowSize = colorFrame.getStrideInBytes() / sizeof(openni::RGB888Pixel);

	cout << colorFrame.getCropOriginX() << ", " << colorFrame.getCropOriginY() << endl;

	for (int y = 0, idx=0; y < colorFrame.getHeight(); ++y)
	{
		const openni::RGB888Pixel* pImage = pImageRow;
		openni::RGB888Pixel* pTex = pTexRow + colorFrame.getCropOriginX();

		for (int x = 0; x < colorFrame.getWidth(); ++x, ++pImage, ++pTex)
		{
			*pTex = *pImage;

			colorValues[idx++] = pTex->b;
			colorValues[idx++] = pTex->g;
			colorValues[idx++] = pTex->r;
			colorValues[idx++] = 0xff;
		}

		pImageRow += rowSize;
		pTexRow += nTexMapX;
	}
}

void PrimeSenseTest::directMapDepth() {
	memset(&depthBuffer[0], 0, nTexMapX*nTexMapY*sizeof(openni::RGB888Pixel));

	const openni::DepthPixel* pDepthRow = (const openni::DepthPixel*)depthFrame.getData();
	openni::RGB888Pixel* pTexRow = &depthBuffer[0] + depthFrame.getCropOriginY() * nTexMapX;
	int rowSize = depthFrame.getStrideInBytes() / sizeof(openni::DepthPixel);

	cout << depthFrame.getCropOriginX() << ", " << depthFrame.getCropOriginY() << endl;

	for (int y = 0, idx=0; y < depthFrame.getHeight(); ++y)
	{
		const openni::DepthPixel* pDepth = pDepthRow;
		openni::RGB888Pixel* pTex = pTexRow + depthFrame.getCropOriginX();

		for (int x = 0; x < depthFrame.getWidth(); ++x, ++pDepth, ++pTex)
		{
			if (*pDepth != 0)
			{
				USHORT depth = *pDepth;
				pTex->r = (uint8_t) (depth%256);
				pTex->g = (uint8_t) (depth/256);
				pTex->b = 0;

				depthValues[idx++] = pTex->b;
				depthValues[idx++] = pTex->g;
				depthValues[idx++] = pTex->r;
				depthValues[idx++] = 0xff;
			}
			else {
				depthValues[idx++] = 0;
				depthValues[idx++] = 0;
				depthValues[idx++] = 0;
				depthValues[idx++] = 0xff;
			}
		}

		pDepthRow += rowSize;
		pTexRow += nTexMapX;
	}
}

void PrimeSenseTest::warpDepthToColor() {
}

void PrimeSenseTest::warpColorToDepth() {
}

void PrimeSenseTest::startSensor() {
	timer.start(34);
}

QImage toQImage(unsigned char* data, int w, int h)
{
	QImage qimg(w, h, QImage::Format_ARGB32);
	for(int i=0, idx=0;i<h;i++)
	{
		for(int j=0;j<w;j++, idx+=4)
		{
			unsigned char r = data[idx+2];
			unsigned char g = data[idx+1];
			unsigned char b = data[idx];
			unsigned char a = 255;
			QRgb qp = qRgba(r, g, b, a);
			qimg.setPixel(j, i, qp);
		}
	}
	return qimg;
}

void PrimeSenseTest::takeScreenshot() {
	QImage rgbImg, dImg, irImg;

	if( !updateStream() ) {
		cerr << "Failed to update stream." << endl;
		return;
	}

	colorView->bindStreamData(&colorValues[0]);
	depthView->bindStreamData(&depthValues[0]);	

	// create QImages from the data
	rgbImg = toQImage(&colorValues[0], width, height);
	dImg = toQImage(&depthValues[0], width, height);

	rgbImg.save("color.png");
	dImg.save("depth.png");
}

void PrimeSenseTest::switchMode() {
	m = Mode((m+1)%2);
}