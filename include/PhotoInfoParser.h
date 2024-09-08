#ifndef PHOTOINFOPARSER_H
#define PHOTOINFOPARSER_H

#include <string>
#include <vector>
#include "tinyxml2.h"

struct DistortionCoefficients {
	double k1, k2, k3, p1, p2;
};

struct CameraPose {
	double rotationMatrix[3][3];
	double center[3];
};

struct PhotoInfo {
	std::string imagePath;
	int imageWidth, imageHeight;
	double focalLength;
	double principalPointX, principalPointY;
	DistortionCoefficients distortion;
	CameraPose pose;
	double intrinsicMatrix[3][3];
	double fovX, fovY;  // 水平和垂直视场角
	double aspectRatio; // 纵横比
};

class PhotoInfoParser {
public:
	PhotoInfoParser(const std::string& xmlFile);
	std::vector<PhotoInfo> parsePhotoInfo();
	void calculateFoVandAspectRatio(PhotoInfo& photoInfo);

private:
	std::string xmlFile;
};

#endif // PHOTOINFOPARSER_H
