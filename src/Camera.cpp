#include <osg/MatrixTransform>
#include <osg/PrimitiveSet>
#include "Camera.h"
#include <iostream>
#include <osg/Matrixd>
#include <osg/Vec3d>
#include <dirent.h>
#include <osg/Plane>
#include <osg/ShapeDrawable>
#include <osg/Geode>
#include <osg/Matrixd>
#include <osg/Notify>
#include <TileIntersectionCalculator.h>

static void printMatrix(const osg::Matrixd& matrix) {
	osg::notify(osg::NOTICE) << "Matrix: \n";
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			osg::notify(osg::NOTICE) << matrix(row, col) << " ";
		}
		osg::notify(osg::NOTICE) << std::endl;
	}
}

// 构造函数
Camera::Camera(const PhotoInfo& photoInfo)
	: photoInfo(photoInfo),
	fx(photoInfo.intrinsicMatrix[0][0]),
	fy(photoInfo.intrinsicMatrix[1][1]),
	cx(photoInfo.intrinsicMatrix[0][2]),
	cy(photoInfo.intrinsicMatrix[1][2]),
	cameraCenter(osg::Vec3d(photoInfo.pose.center[0], photoInfo.pose.center[1], photoInfo.pose.center[2])) {
	// Calculate the inverse of the intrinsic matrix
	intrinsicMatrixInverse = osg::Matrixd(
		1.0 / fx, 0, -cx / fx, 0,
		0, 1.0 / fy, -cy / fy, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);
	// Compute the inverse of the rotation matrix
	osg::Matrixd rotationMatrix(
		photoInfo.pose.rotationMatrix[0][0], photoInfo.pose.rotationMatrix[0][1], photoInfo.pose.rotationMatrix[0][2], 0,
		photoInfo.pose.rotationMatrix[1][0], photoInfo.pose.rotationMatrix[1][1], photoInfo.pose.rotationMatrix[1][2], 0,
		photoInfo.pose.rotationMatrix[2][0], photoInfo.pose.rotationMatrix[2][1], photoInfo.pose.rotationMatrix[2][2], 0,
		0, 0, 0, 1
	);

	// Initialize the ENU to OSG rotation matrix （x,y,z）--> (x,-z, y)
	enuToOsgRotation.set(
		1, 0, 0, 0,
		0, 0, -1, 0,
		0, 1, 0, 0,
		0, 0, 0, 1
	);
}

// 计算相机的四个角的射线
std::vector<osg::Vec3d> Camera::calculateCornerRays() const {
	// Coordinates of the four corners of the image
	std::vector<osg::Vec2d> corners = {
		osg::Vec2d(0, 0),
		osg::Vec2d(photoInfo.imageWidth - 1, 0),
		osg::Vec2d(0, photoInfo.imageHeight - 1),
		osg::Vec2d(photoInfo.imageWidth - 1, photoInfo.imageHeight - 1)
	};
	std::vector<osg::Vec3d> cornerRays;
	for (const auto& corner : corners) {
		osg::Vec3d normalizedCoords = pixelToNormalizedImageCoordinates(corner.x(), corner.y());
		osg::Vec3d cameraRay = normalizedImageCoordinatesToRay(normalizedCoords);
		cornerRays.push_back(cameraRay);
	}
	return cornerRays;
}

// 计算相机的部分像素射线
//std::vector<osg::Vec3d> Camera::calculatePartialPixelRays(int step) const {
//    std::vector<osg::Vec3d> pixelRays;
//    pixelRays.reserve((photoInfo.imageWidth / step) * (photoInfo.imageHeight / step));
//    for (int y = 0; y < photoInfo.imageHeight; y += step) {
//        for (int x = 0; x < photoInfo.imageWidth; x += step) {
//            osg::Vec3d normalizedCoords = pixelToNormalizedImageCoordinates(x, y);
//            osg::Vec3d cameraRay = normalizedImageCoordinatesToRay(normalizedCoords);
//            pixelRays.push_back(cameraRay);
//        }
//    }
//    return pixelRays;
//}
std::vector<std::pair<osg::Vec3d, osg::Vec3d>> Camera::calculatePartialPixelRays(int step, double length) const {
	std::vector<std::pair<osg::Vec3d, osg::Vec3d>> pixelRays;
	pixelRays.reserve((photoInfo.imageWidth / step) * (photoInfo.imageHeight / step));

	for (int y = 0; y < photoInfo.imageHeight; y += step) {
		for (int x = 0; x < photoInfo.imageWidth; x += step) {
			osg::Vec3d normalizedCoords = pixelToNormalizedImageCoordinates(x, y);
			osg::Vec3d position = getCameraCenter();
			osg::Vec3d direction = normalizedImageCoordinatesToRay(normalizedCoords);
			osg::Vec3d endPoint = position + direction * length;
			pixelRays.emplace_back(position, endPoint);
		}
	}
	return pixelRays;
}

// 将像素坐标转换为归一化图像坐标
osg::Vec3d Camera::pixelToNormalizedImageCoordinates(double x, double y) const {
	// Calculate the normalized image coordinates
	osg::Vec3d pixelCoords(x, y, 1.0);
	osg::Vec3d normalizedCoords;
	normalizedCoords.x() = intrinsicMatrixInverse(0, 0) * pixelCoords.x() + intrinsicMatrixInverse(0, 1) * pixelCoords.y() + intrinsicMatrixInverse(0, 2) * pixelCoords.z();
	normalizedCoords.y() = intrinsicMatrixInverse(1, 0) * pixelCoords.x() + intrinsicMatrixInverse(1, 1) * pixelCoords.y() + intrinsicMatrixInverse(1, 2) * pixelCoords.z();
	normalizedCoords.z() = intrinsicMatrixInverse(2, 0) * pixelCoords.x() + intrinsicMatrixInverse(2, 1) * pixelCoords.y() + intrinsicMatrixInverse(2, 2) * pixelCoords.z();

	return normalizedCoords;
}

// 将归一化图像坐标转换为射线
osg::Vec3d Camera::normalizedImageCoordinatesToRay(const osg::Vec3d& normalizedCoords) const {
	osg::Vec3d cameraRay = normalizedCoords;
	cameraRay.x() = photoInfo.pose.rotationMatrix[0][0] * normalizedCoords.x() + photoInfo.pose.rotationMatrix[1][0] * normalizedCoords.y() + photoInfo.pose.rotationMatrix[2][0] * normalizedCoords.z();
	cameraRay.y() = photoInfo.pose.rotationMatrix[0][1] * normalizedCoords.x() + photoInfo.pose.rotationMatrix[1][1] * normalizedCoords.y() + photoInfo.pose.rotationMatrix[2][1] * normalizedCoords.z();
	cameraRay.z() = photoInfo.pose.rotationMatrix[0][2] * normalizedCoords.x() + photoInfo.pose.rotationMatrix[1][2] * normalizedCoords.y() + photoInfo.pose.rotationMatrix[2][2] * normalizedCoords.z();

	// Applying ENU to OSG rotation transformation manually
	osg::Vec3d transformedRay;
	transformedRay.x() = enuToOsgRotation(0, 0) * cameraRay.x() + enuToOsgRotation(0, 1) * cameraRay.y() + enuToOsgRotation(0, 2) * cameraRay.z();
	transformedRay.y() = enuToOsgRotation(1, 0) * cameraRay.x() + enuToOsgRotation(1, 1) * cameraRay.y() + enuToOsgRotation(1, 2) * cameraRay.z();
	transformedRay.z() = enuToOsgRotation(2, 0) * cameraRay.x() + enuToOsgRotation(2, 1) * cameraRay.y() + enuToOsgRotation(2, 2) * cameraRay.z();
	return transformedRay;
}

// 添加视锥体的边
void Camera::addFrustumEdges(osg::ref_ptr<osg::DrawElementsUInt> edges) const {
	static const unsigned int indices[] = {
		0, 1, 1, 2, 2, 3, 3, 0, // Near plane
		4, 5, 5, 6, 6, 7, 7, 4, // Far plane
		0, 4, 1, 5, 2, 6, 3, 7  // Connect near to far plane
	};
	for (int i = 0; i < 24; ++i) {
		edges->push_back(indices[i]);
	}
}

osg::ref_ptr<osg::MatrixTransform> Camera::createFrustumGeometry(double heightThreshold) const {
	double fovY = osg::DegreesToRadians(photoInfo.fovY); // Vertical field of view in radians
	double aspectRatio = photoInfo.aspectRatio;
	double nearPlane = 0.1;  // Near clipping plane

	// Determine the far clipping plane based on the height threshold
	double farPlane;
	// print  photoInfo.pose.center[1] heightThreshold
	//std::cout << "photoInfo.pose.center[2]:" << photoInfo.pose.center[2] << std::endl;
	//std::cout << "heightThreshold:" << heightThreshold << std::endl;
	if (photoInfo.pose.center[2] > (heightThreshold + 30)) {
		//farPlane = 200.0;  // Use far plane of 100 if above the threshold
		farPlane = 8.0;  // Use far plane of 100 if above the threshold
	}
	else {
		//farPlane = 130.0;   // Use far plane of 50 if at or below the threshold
		farPlane = 8.0;
	}

	double tanHalfFovY = tan(fovY / 2.0);
	double tanHalfFovX = tanHalfFovY * aspectRatio;

	osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
	vertices->push_back(osg::Vec3(-tanHalfFovX * nearPlane, -tanHalfFovY * nearPlane, nearPlane));
	vertices->push_back(osg::Vec3(tanHalfFovX * nearPlane, -tanHalfFovY * nearPlane, nearPlane));
	vertices->push_back(osg::Vec3(tanHalfFovX * nearPlane, tanHalfFovY * nearPlane, nearPlane));
	vertices->push_back(osg::Vec3(-tanHalfFovX * nearPlane, tanHalfFovY * nearPlane, nearPlane));
	vertices->push_back(osg::Vec3(-tanHalfFovX * farPlane, -tanHalfFovY * farPlane, farPlane));
	vertices->push_back(osg::Vec3(tanHalfFovX * farPlane, -tanHalfFovY * farPlane, farPlane));
	vertices->push_back(osg::Vec3(tanHalfFovX * farPlane, tanHalfFovY * farPlane, farPlane));
	vertices->push_back(osg::Vec3(-tanHalfFovX * farPlane, tanHalfFovY * farPlane, farPlane));

	osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
	geometry->setVertexArray(vertices);

	osg::ref_ptr<osg::DrawElementsUInt> edges = new osg::DrawElementsUInt(osg::PrimitiveSet::LINES, 24);
	addFrustumEdges(edges);
	geometry->addPrimitiveSet(edges);

	osg::ref_ptr<osg::Geode> geode = new osg::Geode();
	geode->addDrawable(geometry);

	osg::Matrixd RotationMatrixInverse(
		photoInfo.pose.rotationMatrix[0][0], photoInfo.pose.rotationMatrix[1][0], photoInfo.pose.rotationMatrix[2][0], 0,
		photoInfo.pose.rotationMatrix[0][1], photoInfo.pose.rotationMatrix[1][1], photoInfo.pose.rotationMatrix[2][1], 0,
		photoInfo.pose.rotationMatrix[0][2], photoInfo.pose.rotationMatrix[1][2], photoInfo.pose.rotationMatrix[2][2], 0,
		0, 0, 0, 1
	);

	osg::Matrixd combinedMatrix = enuToOsgRotation * RotationMatrixInverse;

	osg::Matrixd transformMatrix = osg::Matrix::inverse(combinedMatrix) * osg::Matrixd::translate(
		osg::Vec3(photoInfo.pose.center[0], -photoInfo.pose.center[2], photoInfo.pose.center[1])
	);

	osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform();
	transform->setMatrix(transformMatrix);
	transform->addChild(geode);

	return transform;
}

// 从视锥体的MatrixTransform中提取边界盒
osg::BoundingBox Camera::calculateFrustumBoundingBox(const osg::ref_ptr<osg::MatrixTransform>& frustumTransform) const {
	if (!frustumTransform) return osg::BoundingBox();

	osg::Geode* geode = dynamic_cast<osg::Geode*>(frustumTransform->getChild(0));
	if (!geode) return osg::BoundingBox();

	osg::BoundingBox bbox;
	osg::Matrix worldMatrix = frustumTransform->getMatrix();  // 获取变换矩阵

	for (unsigned int i = 0; i < geode->getNumDrawables(); ++i) {
		osg::Drawable* drawable = geode->getDrawable(i);
		osg::Geometry* geometry = drawable->asGeometry();
		osg::Vec3Array* vertices = dynamic_cast<osg::Vec3Array*>(geometry->getVertexArray());
		for (auto& vertex : *vertices) {
			bbox.expandBy(vertex * worldMatrix);  // 应用变换矩阵
		}
	}

	return bbox;
}

// 创建一个用于显示边界框的Geode，边界框为黄色
osg::ref_ptr<osg::Geode> Camera::createBoundingBoxDrawable(const osg::BoundingBox& bbox) {
	osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
	osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array(8);

	vertices->at(0) = osg::Vec3(bbox.xMin(), bbox.yMin(), bbox.zMin());
	vertices->at(1) = osg::Vec3(bbox.xMax(), bbox.yMin(), bbox.zMin());
	vertices->at(2) = osg::Vec3(bbox.xMax(), bbox.yMax(), bbox.zMin());
	vertices->at(3) = osg::Vec3(bbox.xMin(), bbox.yMax(), bbox.zMin());
	vertices->at(4) = osg::Vec3(bbox.xMin(), bbox.yMin(), bbox.zMax());
	vertices->at(5) = osg::Vec3(bbox.xMax(), bbox.yMin(), bbox.zMax());
	vertices->at(6) = osg::Vec3(bbox.xMax(), bbox.yMax(), bbox.zMax());
	vertices->at(7) = osg::Vec3(bbox.xMin(), bbox.yMax(), bbox.zMax());

	geometry->setVertexArray(vertices);

	osg::ref_ptr<osg::DrawElementsUInt> edges = new osg::DrawElementsUInt(osg::PrimitiveSet::LINES, 0);
	for (int i = 0; i < 4; ++i) {
		edges->push_back(i); edges->push_back((i + 1) % 4);
		edges->push_back(i + 4); edges->push_back((i + 1) % 4 + 4);
		edges->push_back(i); edges->push_back(i + 4);
	}

	geometry->addPrimitiveSet(edges);
	osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;
	colors->push_back(osg::Vec4(1.0, 1.0, 0.0, 1.0)); // Yellow color
	geometry->setColorArray(colors, osg::Array::BIND_OVERALL);

	osg::ref_ptr<osg::Geode> geode = new osg::Geode();
	geode->addDrawable(geometry);

	return geode;
}

// 检测与MatrixTransform相关的视锥体边界框与一组瓦片边界框之间的交集
// 函数：计算与视锥体边界盒相交的瓦片边界盒
std::vector<NamedBoundingBox> Camera::calculateIntersectingTiles(const osg::BoundingBox& frustumBBox, const std::vector<NamedBoundingBox>& tileBoundingBoxes) {
	std::vector<NamedBoundingBox> intersectingTiles;

	for (const NamedBoundingBox& tile : tileBoundingBoxes) {
		if (frustumBBox.intersects(tile.bbox)) {
			intersectingTiles.push_back(tile);
			std::cout << "Intersected Tile: " << tile.name << std::endl;  // 输出相交的瓦片名称
		}
	}

	return intersectingTiles;
}