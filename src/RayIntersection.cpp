#include "RayIntersection.h"
#include <osgUtil/IntersectVisitor>
#include <osg/LineSegment>
#include <osg/MatrixTransform>
#include <thread>
#include <vector>
#include <mutex>
#include <map>
#include <osg/Vec3d>

typedef std::map<osg::Vec3d, IntersectionDetail> IntersectionResults;

RayIntersection::RayIntersection(osg::ref_ptr<osg::Group> sceneRoot, const std::map<std::string, osg::BoundingBox>& tileBoundingBoxes)
	: sceneRoot(sceneRoot), tileBoundingBoxes(tileBoundingBoxes) {}

osg::ref_ptr<osg::Node> getNodeByName(osg::Group* group, const std::string& name) {
	if (!group) return nullptr;
	for (unsigned int i = 0; i < group->getNumChildren(); ++i) {
		osg::Node* child = group->getChild(i);
		if (child->getName() == name) {
			return child;
		}
	}
	return nullptr;
}

void processSegment(int start, int end, const std::vector<osg::Vec3d>& rays, const osg::Vec3& cameraCenter,
	const std::map<std::string, osg::BoundingBox>& tileBoundingBoxes, osg::Group* sceneRoot,
	IntersectionResults& results, std::mutex& mutex) {
	for (int i = start; i < end; ++i) {
		const auto& rayDirection = rays[i];
		IntersectionDetail detail;
		osg::ref_ptr<osg::LineSegment> lineSegment = new osg::LineSegment(cameraCenter, cameraCenter + rayDirection * 1000.0);

		for (const auto& tile : tileBoundingBoxes) {
			const std::string& tileName = tile.first;
			const osg::BoundingBox& bbox = tile.second;

			if (lineSegment->intersect(bbox)) {
				detail.tileNames.push_back(tileName);
			}
		}

		if (!detail.tileNames.empty()) {
			std::lock_guard<std::mutex> lock(mutex);
			results[rayDirection] = detail;
		}
	}
}

IntersectionResults RayIntersection::calculateIntersections(const osg::Vec3& cameraCenter, const std::vector<osg::Vec3d>& rays) {
	IntersectionResults rayTileIntersections;
	std::mutex mapMutex;
	unsigned int numThreads = std::thread::hardware_concurrency();
	unsigned int blockSize = rays.size() / numThreads;
	std::vector<std::thread> threads;

	for (unsigned int i = 0; i < numThreads; ++i) {
		int start = i * blockSize;
		int end = (i == numThreads - 1) ? rays.size() : start + blockSize;
		threads.emplace_back(processSegment, start, end, std::ref(rays), std::ref(cameraCenter),
			std::ref(tileBoundingBoxes), sceneRoot.get(), std::ref(rayTileIntersections), std::ref(mapMutex));
	}

	for (auto& thread : threads) {
		thread.join();
	}

	return rayTileIntersections;
}