//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This file is part of CL-EyeMulticam SDK
//
// C++ CLEyeFaceTracker Sample Application
//
// For updates and file downloads go to: http://codelaboratories.com
//
// Copyright 2008-2012 (c) Code Laboratories, Inc. All rights reserved.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "header.h"

#define LOW_H			77
#define HIGH_H			78
#define LOW_S			0
#define HIGH_S			60
#define LOW_V			100
#define HIGH_V			110

// Sample camera capture and processing class
class CLEyeCameraCapture
{
	CHAR _windowName[256];
	GUID _cameraGUID;
	CLEyeCameraInstance _cam;
	CLEyeCameraColorMode _mode;
	CLEyeCameraResolution _resolution;
	float _fps;
	HANDLE _hThread;
	bool _running;
public:
	CLEyeCameraCapture(LPSTR windowName, GUID cameraGUID, CLEyeCameraColorMode mode, CLEyeCameraResolution resolution, float fps) :
	_cameraGUID(cameraGUID), _cam(NULL), _mode(mode), _resolution(resolution), _fps(fps), _running(false)
	{
		strcpy(_windowName, windowName);
	}
	bool StartCapture()
	{
		_running = true;
		cvNamedWindow(_windowName, CV_WINDOW_AUTOSIZE);
		// Start CLEye image capture thread
		_hThread = CreateThread(NULL, 0, &CLEyeCameraCapture::CaptureThread, this, 0, 0);
		if(_hThread == NULL)
		{
			MessageBox(NULL,"Could not create capture thread","CLEyeMulticamTest", MB_ICONEXCLAMATION);
			return false;
		}
		return true;
	}
	void StopCapture()
	{
		if(!_running)	return;
		_running = false;
		WaitForSingleObject(_hThread, 1000);
		cvDestroyWindow(_windowName);
	}
	void IncrementCameraParameter(int param)
	{
		if(!_cam)	return;
		CLEyeSetCameraParameter(_cam, (CLEyeCameraParameter)param, CLEyeGetCameraParameter(_cam, (CLEyeCameraParameter)param)+10);
	}
	void DecrementCameraParameter(int param)
	{
		if(!_cam)	return;
		CLEyeSetCameraParameter(_cam, (CLEyeCameraParameter)param, CLEyeGetCameraParameter(_cam, (CLEyeCameraParameter)param)-10);
	}
	void Run()
	{
		int w, h;
		IplImage *pCapImage;
		PBYTE pCapBuffer = NULL;
		// Create camera instance
		_cam = CLEyeCreateCamera(_cameraGUID, _mode, _resolution, _fps);
		if(_cam == NULL)		return;
		// Get camera frame dimensions
		CLEyeCameraGetFrameDimensions(_cam, w, h);
		// Depending on color mode chosen, create the appropriate OpenCV image
		if(_mode == CLEYE_COLOR_PROCESSED || _mode == CLEYE_COLOR_RAW)
			pCapImage = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, 4);
		else
			pCapImage = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, 1);

		// Set some camera parameters
		CLEyeSetCameraParameter(_cam, CLEYE_GAIN, 0);
		CLEyeSetCameraParameter(_cam, CLEYE_EXPOSURE, 10);

		// Start capturing
		CLEyeCameraStart(_cam);
	
		IplImage* image = cvCreateImage(cvSize(pCapImage->width, pCapImage->height), IPL_DEPTH_8U, 3);
		IplImage* image_region = cvCreateImage(cvSize(pCapImage->width, pCapImage->height), IPL_DEPTH_8U, 1);
		IplImage* image_h = cvCreateImage(cvSize(pCapImage->width, pCapImage->height), IPL_DEPTH_8U, 1);
		IplImage* image_h1 = cvCreateImage(cvSize(pCapImage->width, pCapImage->height), IPL_DEPTH_8U, 1);
		IplImage* image_h2 = cvCreateImage(cvSize(pCapImage->width, pCapImage->height), IPL_DEPTH_8U, 1);
		IplImage* image_s = cvCreateImage(cvSize(pCapImage->width, pCapImage->height), IPL_DEPTH_8U, 1);
		IplImage* image_v = cvCreateImage(cvSize(pCapImage->width, pCapImage->height), IPL_DEPTH_8U, 1);
		IplImage* image_label = cvCreateImage(cvSize(pCapImage->width, pCapImage->height), IPL_DEPTH_16S, 1);

	
		// image capturing loop
		while(_running)
		{
			cvGetRawData(pCapImage, &pCapBuffer);
			CLEyeCameraGetFrame(_cam, pCapBuffer);

			cvCvtColor(pCapImage,image,CV_BGR2HSV); //RGB系からHSV系に変換
			cvSplit(image,image_h,image_s,image_v,NULL);

			//それぞれのパラメーターに閾値を与える
			//Value:輝度
			cvThreshold(image_v, image_v, LOW_V, 255, CV_THRESH_BINARY);
			//Hue:色相
			cvThreshold(image_h, image_h1, LOW_H, 255, CV_THRESH_BINARY);					//下限
			cvThreshold(image_h, image_h2, HIGH_H, 255, CV_THRESH_BINARY_INV);				//上限	
			cvAnd(image_h1, image_h2, image_h);												//下限と上限の合成
			//Saturation:彩度
			cvThreshold(image_s, image_s, LOW_S, 255, CV_THRESH_BINARY);
			//それぞれのパラメータを合成
			cvAnd(image_h, image_s, image_region);				
			cvAnd(image_region, image_v, image_region);

			//Labeling
			cvThreshold(image_region, image_region, 0, 255, CV_THRESH_BINARY|CV_THRESH_OTSU);
			cvDilate(image_region,image_region,NULL,10);
			cvErode(image_region,image_region,NULL,15);
			LabelingBS label;
			label.Exec((uchar*)image_region->imageData,(short*)image_label->imageData,image_region->width,image_region->height,true,10);
			
			if(label.GetNumOfRegions() > 0){ //ラベリングした結果、面積を持つ部分がある場合の処理
				float x,y;
				RegionInfoBS *ri;
				ri = label.GetResultRegionInfo(0);
				ri->GetCenter(x,y);
				printf("%f,%f\n",x,y);
				cvCircle (pCapImage, cvPoint (cvRound (x), cvRound (y)), 15, CV_RGB (255, 0, 255), 5, 8, 0);
			}	
			
			printf("label number:%d \n",label.GetNumOfRegions());

			cvShowImage(_windowName, image_region);
		}
		cvReleaseImage(&image);
		cvReleaseImage(&image_region);
		cvReleaseImage(&image_s);
		cvReleaseImage(&image_v);
		cvReleaseImage(&image_h1);
		cvReleaseImage(&image_h2);
		cvReleaseImage(&image_h);
		cvReleaseImage(&image_label);

		// Stop camera capture
		CLEyeCameraStop(_cam);
		// Destroy camera object
		CLEyeDestroyCamera(_cam);
		// Destroy the allocated OpenCV image
		cvReleaseImage(&pCapImage);
		_cam = NULL;
	}
	static DWORD WINAPI CaptureThread(LPVOID instance)
	{
		// seed the rng with current tick count and thread id
		srand(GetTickCount() + GetCurrentThreadId());
		// forward thread to Capture function
		CLEyeCameraCapture *pThis = (CLEyeCameraCapture *)instance;
		pThis->Run();
		return 0;
	}
};

int _tmain(int argc, _TCHAR* argv[])
{
	CLEyeCameraCapture *cam = NULL;
	// Query for number of connected cameras
	int numCams = CLEyeGetCameraCount();
	if(numCams == 0)
	{
		printf("No PS3Eye cameras detected\n");
		return -1;
	}
	char windowName[64];
	// Query unique camera uuid
	GUID guid = CLEyeGetCameraUUID(0);
	printf("Camera GUID: [%08x-%04x-%04x-%02x%02x%02x%02x%02x%02x%02x%02x]\n", 
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2],
		guid.Data4[3], guid.Data4[4], guid.Data4[5],
		guid.Data4[6], guid.Data4[7]);
	sprintf(windowName, "Face Tracker Window");
	// Create camera capture object
	// Randomize resolution and color mode
	cam = new CLEyeCameraCapture(windowName, guid, CLEYE_COLOR_PROCESSED, CLEYE_VGA, 50);
	printf("Starting capture\n");
	cam->StartCapture();

	printf("Use the following keys to change camera parameters:\n"
		"\t'g' - select gain parameter\n"
		"\t'e' - select exposure parameter\n"
		"\t'z' - select zoom parameter\n"
		"\t'r' - select rotation parameter\n"
		"\t'+' - increment selected parameter\n"
		"\t'-' - decrement selected parameter\n");
	// The <ESC> key will exit the program
	CLEyeCameraCapture *pCam = NULL;
	int param = -1, key;
	while((key = cvWaitKey(0)) != 0x1b)
	{
		switch(key)
		{
		case 'g':	case 'G':	printf("Parameter Gain\n");		param = CLEYE_GAIN;		break;
		case 'e':	case 'E':	printf("Parameter Exposure\n");	param = CLEYE_EXPOSURE;	break;
		case 'z':	case 'Z':	printf("Parameter Zoom\n");		param = CLEYE_ZOOM;		break;
		case 'r':	case 'R':	printf("Parameter Rotation\n");	param = CLEYE_ROTATION;	break;
		case '+':	if(cam)		cam->IncrementCameraParameter(param);					break;
		case '-':	if(cam)		cam->DecrementCameraParameter(param);					break;
		}
	}
	cam->StopCapture();
	delete cam;
	return 0;
}