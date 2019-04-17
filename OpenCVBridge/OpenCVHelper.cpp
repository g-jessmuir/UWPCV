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
	cvtColor(bw, outputMat, COLOR_GRAY2BGRA);

	Mat hierarchy;

	std::vector<std::vector<cv::Point>> contours;

	findContours(bw, contours, hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE,
		cv::Point(0, 0));

	for (int idx = 0; idx < contours.size(); idx++) {
		Scalar color = Scalar(25, 255, 25);
		std::vector<cv::Point> contour = contours.at(idx);
		std::vector<cv::Point> apxCurve;
		cv::Rect rect = boundingRect(contour);
		if (rect.height < 15) continue;
		double ratio = abs(1 - (double)rect.width / rect.height);
		double contourArea = cv::contourArea(contour);
		approxPolyDP(contour, apxCurve, arcLength(contour, true) * 0.02, true);
		if (contourArea / rect.area() > 0.6 && ratio > 2.5 && ratio < 3.5)
		{
			rectangle(outputMat, rect, Scalar(255, 0, 255, 255), 3);
			bool newR = true;
			for (cv::Rect r2 : r)
			{
				cv::Rect r3 = rect & r2;
				if (r3.area() > rect.area()*0.7 || r3.area() > r2.area()*0.7) {
					newR = false;
					break;
				}
			}
			if (newR) {
				r.push_back(rect);
				cv::Mat rTemp;
				std::string s = getOCR(rect, inputMat, rTemp);
				//rectangle(outputMat, rTemp, Scalar(255, 255, 0, 255));
				cvtColor(rTemp, outputMat, COLOR_GRAY2BGRA);
				if (s.length() > 0) {
					//putText(outputMat, s, cv::Point(50, 50), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 4);
				}
			}
		}
	}
}

std::string OpenCVHelper::getOCR(cv::Rect r, Mat m, cv::Mat& rTemp)
{
	//Ptr<text::OCRTesseract> ocr = text::OCRTesseract::create("./Assets/");

	std::vector<cv::Rect> rects;
	std::vector<std::string> strings;
	std::string output;
	std::vector<float> confid;

	Mat subx = Mat(m, r);

	Mat sub;
	Mat sub3;
	cv::Rect r2 = cv::Rect(subx.cols / 6, 0, subx.cols / 2.3, subx.rows);
	Mat sub2 = Mat(subx, r2);
	Mat sub4 = brighten(sub2);
	sub4 = sub2.clone();

	resize(sub4, sub2, cv::Size(0, 0), 5, 5, INTER_LANCZOS4);

	sub = processWordImage(sub2);
	resize(sub, rTemp, cv::Size(896, 504)); //return to show
	if (sub.empty()) return "nothing";

	//Mat forOCR;
	//cvtColor(subx, forOCR, COLOR_BGRA2GRAY);
	//ocr->setWhiteList("0123456789");
	//output = ocr->run(sub, 0);
	if (output.size() > 0)
	{
		output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
		output.erase(std::remove(output.begin(), output.end(), ' '), output.end());
	}
	else {
		return "size: " + std::to_string(sub.cols) + "," + std::to_string(sub.rows);
	}

	return output;
}

Mat OpenCVHelper::brighten(Mat image)
{
	double alpha = 1.5; /*< Simple contrast control */
	int beta = 10;       /*< Simple brightness control */

	Mat new_image = Mat::zeros(image.size(), image.type());

	for (int y = 0; y < image.rows; y++) {
		for (int x = 0; x < image.cols; x++) {
			for (int c = 0; c < 3; c++) {
				new_image.at<Vec3b>(y, x)[c] =
					saturate_cast<uchar>(alpha*(image.at<Vec3b>(y, x)[c]) + beta);
			}
		}

	}
	return new_image;
}

Mat OpenCVHelper::processWordImage(Mat subImg) {
	Mat subUp;

	subUp = subImg.clone();

	Mat kernel = (Mat_<float>(2, 3) <<
		1, 1, 1,
		1, -8, 1,
		1, 1, 1);
	cv::Point anchor = cv::Point(-1, -1);
	int delta = 0;
	int ddepth = CV_16S;
	Mat dst = subUp;
	Mat src = subImg;
	Mat dst2 = subUp;
	Mat src_gray;
	int kernel_size = 3;
	int scale = 1;

	//	filter2D(src, dst, ddepth, kernel, anchor, delta, BORDER_DEFAULT);

		//im = filter2D(im, -1, kernel)

	Mat kernel2 = (Mat_<float>(3, 3) <<
		1, 1, 1,
		1, 8, 1,
		1, 1, 1);

	Mat gry;
	cvtColor(dst, gry, COLOR_RGBA2GRAY);
	threshold(gry, subUp, 90.0, 255.0, THRESH_BINARY | THRESH_OTSU);

	int cx = 0;
	int cy = 0;
	int cx2 = 0;
	int cy2 = 0;
	for (int row = 0; subUp.rows > 0 && row < subUp.rows; row++) {
		if (countNonZero(subUp.row(row)) > 0.25 * subUp.cols
			&&
			subUp.cols - countNonZero(subUp.row(row)) < 0.75 * subUp.cols
			) {
			cy = row;
			break;
		}

	}
	for (int row = subUp.rows - 1; row >= 0; row--) {
		if (countNonZero(subUp.row(row)) > 0.25 * subUp.cols
			&&
			subUp.cols - countNonZero(subUp.row(row)) < 0.75 * subUp.cols
			) {
			cy2 = row;
			break;
		}

	}
	for (int col = 0; col < subUp.cols; col++) {
		if (countNonZero(subUp.col(col)) > 0.25 * subUp.rows
			&&
			subUp.rows - countNonZero(subUp.col(col)) < 0.75 * subUp.rows)
		{
			cx = col;
			break;
		}

	}
	for (int col = subUp.cols - 1; col >= 0; col--) {
		if (countNonZero(subUp.col(col)) > 0.25 * subUp.rows
			&&
			subUp.rows - countNonZero(subUp.col(col)) < 0.75 * subUp.rows) {
			cx2 = col;
			break;
		}

	}
	Mat z;

	if (cx < cx2 && cy < cy2 && cx2 > 0 && cy2 > 0)
	{
		subUp = subUp(cv::Rect(cx, cy, cx2 - cx, cy2 - cy));
		std::vector<cv::Rect> l = getLetters(subUp);
		cv::Rect lRx;
		if (l.size() > 0) {
			lRx = l.at(0);
			for (int i = 1; i < l.size(); i++)
			{
				cv::Rect lR2 = lRx | l.at(i);
				lRx = lR2;
			}
			int rx = cx + lRx.x - 5;
			int ry = cy + lRx.y - 5;
			int rx2 = rx + lRx.width + 10;
			int ry2 = ry + lRx.height + 10;
			rx = rx < 0 ? 0 : rx;
			ry = ry < 0 ? 0 : ry;
			rx2 = rx2 > gry.cols ? gry.cols : rx2;
			ry2 = ry2 > gry.rows ? gry.rows : ry2;

			if (rx2 > rx && ry2 > ry)
			{
				subUp = gry(cv::Rect(rx, ry, rx2 - rx, ry2 - ry));
			}
			else subUp = z;
		}

	}
	else subUp = z;

	return subUp;
}

std::vector<cv::Rect> OpenCVHelper::getLetters(Mat m)
{
	std::vector<cv::Rect> r1;
	int thresh = 40;
	Mat bw;
	Canny(m, bw, thresh, thresh * 2);
	Mat hierarchy;

	std::vector<std::vector<cv::Point>> contours;

	findContours(bw, contours, hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE,
		cv::Point(0, 0));

	for (int idx = 0; idx < contours.size(); idx++) {
		Scalar color = Scalar(25, 255, 25);
		std::vector<cv::Point> contour = contours.at(idx);
		std::vector<cv::Point> apxCurve;
		cv::Rect rect = boundingRect(contour);
		if (rect.height < (bw.rows*0.5)) continue;
		double ratio = abs(1 - (double)rect.width / rect.height);
		double contourArea = cv::contourArea(contour);
		approxPolyDP(contour, apxCurve, arcLength(contour, true) * 0.02, true);
		//	cout << ratio << endl;
		if (ratio > 0.2 && ratio < 1)
		{
			//	rectangle(m, rect, color);
			r1.push_back(rect);

		}

	}
	return r1;
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