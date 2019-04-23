#include "pch.h"
#include "OpenCVHelper.h"
#include "MemoryBuffer.h"
#include <opencv2\text.hpp>

#include <iostream>
#include <algorithm>

using namespace OpenCVBridge;
using namespace Platform;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Foundation;
using namespace Microsoft::WRL;
using namespace Windows::Media;
using namespace cv;

bool OpenCVHelper::GetPointerToPixelData(SoftwareBitmap^ bitmap, unsigned char** pPixelData, unsigned int* capacity)
{
	BitmapBuffer^ bmpBuffer = bitmap->LockBuffer(BitmapBufferAccessMode::ReadWrite);
	IMemoryBufferReference^ reference = bmpBuffer->CreateReference();

	ComPtr<IMemoryBufferByteAccess> pBufferByteAccess;
	if ((reinterpret_cast<IInspectable*>(reference)->QueryInterface(IID_PPV_ARGS(&pBufferByteAccess))) != S_OK)
	{
		return false;
	}

	if (pBufferByteAccess->GetBuffer(pPixelData, capacity) != S_OK)
	{
		return false;
	}
	return true;
}

bool OpenCVHelper::TryConvert(SoftwareBitmap^ from, Mat& convertedMat)
{
	unsigned char* pPixels = nullptr;
	unsigned int capacity = 0;
	if (!GetPointerToPixelData(from, &pPixels, &capacity))
	{
		return false;
	}

	Mat mat(from->PixelHeight,
		from->PixelWidth,
		CV_8UC4, // assume input SoftwareBitmap is BGRA8
		(void*)pPixels);

	// shallow copy because we want convertedMat.data = pPixels
	// don't use .copyTo or .clone
	convertedMat = mat;
	return true;
}

void OpenCVHelper::Blur(SoftwareBitmap^ input, SoftwareBitmap^ output)
{
	Mat inputMat, outputMat;
	if (!(TryConvert(input, inputMat) && TryConvert(output, outputMat)))
	{
		return;
	}
	blur(inputMat, outputMat, cv::Size(15, 15));
}

void OpenCVHelper::Flip(SoftwareBitmap^ input, SoftwareBitmap^ output)
{
	Mat inputMat, outputMat;
	if (!(TryConvert(input, inputMat) && TryConvert(output, outputMat)))
	{
		return;
	}
	flip(inputMat, outputMat, 1);
}

void OpenCVHelper::Process(SoftwareBitmap^ input, SoftwareBitmap^ output)
{
	Mat inputMat, outputMat;
	if (!(TryConvert(input, inputMat) && TryConvert(output, outputMat)))
	{
		return;
	}

	std::vector<cv::Rect> r;

	Mat m2;
	cvtColor(inputMat, m2, COLOR_RGBA2GRAY);
	threshold(m2, m2, 90.0, 200.0, THRESH_BINARY | THRESH_OTSU);
	Mat bw;
	int thresh = 90;
	Canny(inputMat, bw, thresh, thresh * 2);
	cvtColor(bw, outputMat, COLOR_GRAY2RGBA);
}

void OpenCVHelper::GrayScale(SoftwareBitmap^ input, SoftwareBitmap^ output)
{
	Mat inputMat, outputMat;
	if (!(TryConvert(input, inputMat) && TryConvert(output, outputMat)))
	{
		return;
	}

	Mat gray;
	cvtColor(inputMat, gray, COLOR_BGRA2GRAY);
	cvtColor(gray, outputMat, COLOR_GRAY2BGRA);

	return;
}
