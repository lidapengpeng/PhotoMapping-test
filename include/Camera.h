#ifndef CAMERA_H
#define CAMERA_H

#include <osg/Matrixd>
#include <osg/Vec3d>
#include <vector>
#include "PhotoInfoParser.h"
#include <osg/Vec3>
#include <osg/Group>
#include <osg/Polytope>
#include <osg/PrimitiveSet>
#include <osg/ref_ptr>
#include "SceneBuilder.h"

//// 构造平面方程，通过三点
//class Plane {
//public:
//    osg::Vec3 normal;  // 平面的法线
//    float d;           // 平面方程中的D值
//
//    // 构造平面方程，通过三点
//    Plane(const osg::Vec3& p1, const osg::Vec3& p2, const osg::Vec3& p3) {
//        osg::Vec3 u = p2 - p1;  // 从p1到p2的向量
//        osg::Vec3 v = p3 - p1;  // 从p1到p3的向量
//        normal = u ^ v;  // 叉积得到法线
//        normal.normalize(); // 归一化法线
//        d = -(normal * p1);  // 计算平面方程的D值
//    }
//};

class Camera {
public:
	explicit Camera(const PhotoInfo& photoInfo);

	std::vector<osg::Vec3d> calculateCornerRays() const;
	std::vector<std::pair<osg::Vec3d, osg::Vec3d>> calculatePartialPixelRays(int step, double length) const;

	const PhotoInfo& getPhotoInfo() const {
		return photoInfo;
	}

	osg::Vec3 getCameraCenter() const {
		return osg::Vec3(photoInfo.pose.center[0], -photoInfo.pose.center[2], photoInfo.pose.center[1]);
	}
	osg::ref_ptr<osg::MatrixTransform> createFrustumGeometry(double heightThreshold) const;

	osg::BoundingBox calculateFrustumBoundingBox(const osg::ref_ptr<osg::MatrixTransform>& frustumTransform) const;

	//std::vector<NamedBoundingBox> getIntersectingTileBoxes(const osg::ref_ptr<osg::MatrixTransform>& frustumTransform, const std::vector<NamedBoundingBox>& tileBoundingBoxes);
	osg::ref_ptr<osg::Geode> createBoundingBoxDrawable(const osg::BoundingBox& bbox);
	std::vector<NamedBoundingBox> calculateIntersectingTiles(const osg::BoundingBox& frustumBBox, const std::vector<NamedBoundingBox>& tileBoundingBoxes);
	//std::vector<osg::Plane> extractFrustumPlanes(const osg::MatrixTransform* transform) const;
	/*FrustumPlanes createFrustumGeometry() const;*/
	void addFrustumEdges(osg::ref_ptr<osg::DrawElementsUInt> edges) const;
private:
	PhotoInfo photoInfo;  // Stores all relevant photo metadata
	double fx, fy, cx, cy;  // Camera intrinsic parameters
	osg::Matrixd intrinsicMatrixInverse;  // Inverse of the camera's intrinsic matrix
	osg::Matrixd rotationMatrix;  // Transpose of the camera's rotation matrix for converting image coordinates to world coordinates
	osg::Matrixd enuToOsgRotation;  // Rotation matrix to convert from ENU to OSG coordinates
	osg::Vec3d cameraCenter;  // World coordinates of the camera center
	osg::Vec3d pixelToNormalizedImageCoordinates(double x, double y) const;
	osg::Vec3d normalizedImageCoordinatesToRay(const osg::Vec3d& normalizedCoords) const;
};

#endif // CAMERA_H
