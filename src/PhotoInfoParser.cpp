#include "PhotoInfoParser.h"
#include <stdexcept>
#include <cmath>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

PhotoInfoParser::PhotoInfoParser(const std::string& xmlFile) : xmlFile(xmlFile) {}

std::vector<PhotoInfo> PhotoInfoParser::parsePhotoInfo() {
	std::vector<PhotoInfo> photoInfos;
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLError error = doc.LoadFile(xmlFile.c_str());
	if (error != tinyxml2::XML_SUCCESS) {
		throw std::runtime_error("Failed to load XML file: " + xmlFile);
	}

	tinyxml2::XMLElement* blockElement = doc.FirstChildElement("BlocksExchange")->FirstChildElement("Block");
	if (!blockElement) {
		throw std::runtime_error("Block element not found in XML file: " + xmlFile);
	}

	int srsId = std::stoi(blockElement->FirstChildElement("SRSId")->GetText());
	if (srsId != 1) {
		throw std::runtime_error("SRSId is not ENU, skipping this block.");
	}

	tinyxml2::XMLElement* photogroupElement = blockElement->FirstChildElement("Photogroups")->FirstChildElement("Photogroup");
	while (photogroupElement) {
		double sensorSize = std::stod(photogroupElement->FirstChildElement("SensorSize")->GetText());
		int imageWidth = std::stoi(photogroupElement->FirstChildElement("ImageDimensions")->FirstChildElement("Width")->GetText());
		int imageHeight = std::stoi(photogroupElement->FirstChildElement("ImageDimensions")->FirstChildElement("Height")->GetText());

		// 解析主点坐标
		tinyxml2::XMLElement* principalPointElement = photogroupElement->FirstChildElement("PrincipalPoint");
		if (!principalPointElement) {
			throw std::runtime_error("PrincipalPoint element not found in XML file: " + xmlFile);
		}
		double principalPointX = std::stod(principalPointElement->FirstChildElement("x")->GetText());
		double principalPointY = std::stod(principalPointElement->FirstChildElement("y")->GetText());

		// 解析畸变系数
		tinyxml2::XMLElement* distortionElement = photogroupElement->FirstChildElement("Distortion");
		if (!distortionElement) {
			throw std::runtime_error("Distortion element not found in XML file: " + xmlFile);
		}
		DistortionCoefficients distortion;
		distortion.k1 = std::stod(distortionElement->FirstChildElement("K1")->GetText());
		distortion.k2 = std::stod(distortionElement->FirstChildElement("K2")->GetText());
		distortion.k3 = std::stod(distortionElement->FirstChildElement("K3")->GetText());
		distortion.p1 = std::stod(distortionElement->FirstChildElement("P1")->GetText());
		distortion.p2 = std::stod(distortionElement->FirstChildElement("P2")->GetText());

		tinyxml2::XMLElement* photoElement = photogroupElement->FirstChildElement("Photo");

		while (photoElement) {
			PhotoInfo info;
			info.imagePath = photoElement->FirstChildElement("ImagePath")->GetText();

			info.imageWidth = imageWidth;
			info.imageHeight = imageHeight;

			// 使用 Photo 元素中的焦距
			tinyxml2::XMLElement* exifDataElement = photoElement->FirstChildElement("ExifData");
			if (!exifDataElement) {
				throw std::runtime_error("ExifData element not found for photo: " + info.imagePath);
			}
			double focalLength = std::stod(exifDataElement->FirstChildElement("FocalLength")->GetText());
			double focalLength35mmEq = std::stod(exifDataElement->FirstChildElement("FocalLength35mmEq")->GetText());

			// 计算裁剪因子
			double cropFactor = focalLength35mmEq / focalLength;
			// 估计传感器尺寸（假设35mm全幅传感器尺寸为36mm x 24mm）
			double sensorWidthMM = 36.0 / cropFactor;
			double sensorHeightMM = 24.0 / cropFactor;
			//std::cout << "Photo: " << info.imagePath << " Sensor size: " << sensorWidthMM << " x " << sensorHeightMM << std::endl;
			// 计算视场角
			double halfWidth = info.imageWidth / 2.0;
			double halfHeight = info.imageHeight / 2.0;
			info.fovY = 2.0 * atan((halfHeight * sensorHeightMM / info.imageHeight) / focalLength) * (180.0 / M_PI);
			info.fovX = 2.0 * atan((halfWidth * sensorWidthMM / info.imageWidth) / focalLength) * (180.0 / M_PI);
			//std::cout << "Photo: " << info.imagePath << " FoV: " << info.fovX << " x " << info.fovY << std::endl;
			// 计算宽高比
			info.aspectRatio = static_cast<double>(info.imageWidth) / info.imageHeight;

			// 计算内参矩阵的 fx 和 fy
			double fx = focalLength * (imageWidth / sensorWidthMM);
			double fy = focalLength * (imageHeight / sensorHeightMM);

			double cx = principalPointX;
			double cy = principalPointY;
			info.intrinsicMatrix[0][0] = fx;
			info.intrinsicMatrix[0][1] = 0;
			info.intrinsicMatrix[0][2] = cx;
			info.intrinsicMatrix[1][0] = 0;
			info.intrinsicMatrix[1][1] = fy;
			info.intrinsicMatrix[1][2] = cy;
			info.intrinsicMatrix[2][0] = 0;
			info.intrinsicMatrix[2][1] = 0;
			info.intrinsicMatrix[2][2] = 1;

			// 使用 photogroup 中解析的畸变系数
			info.distortion = distortion;
			// 使用 photogroup 中解析的principalPoint
			info.principalPointX = principalPointX;
			info.principalPointY = principalPointY;

			// 解析相机姿态
			tinyxml2::XMLElement* poseElement = photoElement->FirstChildElement("Pose");
			for (int i = 0; i < 3; ++i) {
				for (int j = 0; j < 3; ++j) {
					std::string elementName = "M_" + std::to_string(i) + std::to_string(j);
					double value = std::stod(poseElement->FirstChildElement("Rotation")->FirstChildElement(elementName.c_str())->GetText());
					info.pose.rotationMatrix[i][j] = value;
				}
			}
			info.pose.center[0] = std::stod(poseElement->FirstChildElement("Center")->FirstChildElement("x")->GetText());
			info.pose.center[1] = std::stod(poseElement->FirstChildElement("Center")->FirstChildElement("y")->GetText());
			info.pose.center[2] = std::stod(poseElement->FirstChildElement("Center")->FirstChildElement("z")->GetText());

			photoInfos.push_back(info);
			photoElement = photoElement->NextSiblingElement("Photo");
		}

		photogroupElement = photogroupElement->NextSiblingElement("Photogroup");
	}

	return photoInfos;
}