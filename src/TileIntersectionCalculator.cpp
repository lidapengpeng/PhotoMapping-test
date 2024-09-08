#include "TileIntersectionCalculator.h"
#include <iostream>
#include <osg/NodeVisitor>
#include <osg/Geometry>
#include <limits>
#include <fstream>
#include <vector>
#include <iomanip>
#include <Camera.h>

static std::ostream& operator<<(std::ostream& os, const osg::Vec3d& vec) {
	os << "(" << vec.x() << ", " << vec.y() << ", " << vec.z() << ")";
	return os;
}

// 辅助函数：根据名称查找节点
static osg::Node* findNodeByName(const osg::ref_ptr<osg::Group>& group, const std::string& name) {
	for (unsigned int i = 0; i < group->getNumChildren(); ++i) {
		osg::Node* child = group->getChild(i);
		if (child->getName() == name) {
			return child;
		}
	}
	return nullptr;
}

// 递归遍历节点树，找到所有的 osg::Geode 节点
static void findGeodes(osg::Node* node, std::vector<osg::Geode*>& geodes) {
	if (!node) return;

	osg::Geode* geode = dynamic_cast<osg::Geode*>(node);
	if (geode) {
		geodes.push_back(geode);
	}

	osg::Group* group = node->asGroup();
	if (group) {
		for (unsigned int i = 0; i < group->getNumChildren(); ++i) {
			findGeodes(group->getChild(i), geodes);
		}
	}
}

std::vector<TileIntersectionResult> performRayTileIntersections(
	const osg::ref_ptr<osg::Group>& scene,
	const std::vector<std::pair<osg::Vec3d, osg::Vec3d>>& pixelRays,
	const std::vector<NamedBoundingBox>& intersectingTiles) {
	// 存储每个 tile 被射线击中的数量
	std::map<std::string, int> tileHitCounts;

	// 初始化所有 tile 的击中计数
	for (const auto& tile : intersectingTiles) {
		tileHitCounts[tile.name] = 0;
	}

	for (const auto& ray : pixelRays) {
		osg::Vec3d start(ray.first.x(), ray.first.y(), ray.first.z());
		osg::Vec3d end(ray.second.x(), ray.second.y(), ray.second.z());

		osg::ref_ptr<osgUtil::LineSegmentIntersector> intersector =
			new osgUtil::LineSegmentIntersector(start, end);
		osg::ref_ptr<osgUtil::IntersectionVisitor> iv = new osgUtil::IntersectionVisitor(intersector.get());

		double closestDistance = std::numeric_limits<double>::max();
		std::string closestTile;
		osg::Vec3d closestIntersection;

		for (const auto& tile : intersectingTiles) {
			osg::Node* tileNode = findNodeByName(scene, tile.name);
			if (tileNode) {
				std::vector<osg::Geode*> geodes;
				findGeodes(tileNode, geodes);
				for (osg::Geode* geode : geodes) {
					//std::cout << "Checking geode: " << geode->getName() << std::endl;
					for (unsigned int i = 0; i < geode->getNumDrawables(); ++i) {
						osg::Drawable* drawable = geode->getDrawable(i);
						//std::cout << "Drawable type: " << drawable->className() << std::endl;
						osg::Geometry* geometry = dynamic_cast<osg::Geometry*>(drawable);
						//if (geometry) {
						//    std::cout << "Geometry has " << geometry->getVertexArray()->getNumElements() << " vertices" << std::endl;
						//}
					}
					geode->accept(*iv);
					if (intersector->containsIntersections()) {
						osgUtil::LineSegmentIntersector::Intersections intersections = intersector->getIntersections();
						for (auto& intersection : intersections) {
							double distance = (intersection.getWorldIntersectPoint() - start).length();
							if (distance < closestDistance) {
								closestDistance = distance;
								closestTile = tile.name;
								closestIntersection = intersection.getWorldIntersectPoint();
							}
						}
					}
				}
			}
			else {
				std::cout << "Tile not found: " << tile.name << std::endl;
			}
		}

		if (!closestTile.empty()) {
			//std::cout << "Ray intersects with tile: " << closestTile << " at distance: " << closestDistance << std::endl;
			tileHitCounts[closestTile]++;
		}
	}

	// 计算每个 tile 的射线占比
	std::vector<TileIntersectionResult> results;
	int totalRays = pixelRays.size();
	for (const auto& entry : tileHitCounts) {
		double percentage = (static_cast<double>(entry.second) / totalRays) * 100.0;
		results.push_back({ entry.first, percentage });
	}

	std::cout << "Tile intersection results:" << std::endl;
	for (const auto& result : results) {
		std::cout << "Tile: " << result.tileName << " - " << result.percentage << "%" << std::endl;
	}

	return results;
}

void outputIntersectionResultsToCSV(const std::string& filename, const std::vector<PhotoData>& allPhotoData, const std::vector<std::string>& tileNames) {
	std::ofstream outFile(filename);
	if (!outFile.is_open()) {
		std::cerr << "无法打开文件：" << filename << std::endl;
		return;
	}

	outFile << "Photo Index, Image Path";
	for (const auto& tileName : tileNames) {
		outFile << "," << tileName;
	}
	outFile << std::endl;

	for (const auto& photoData : allPhotoData) {
		outFile << photoData.index << "," << photoData.imagePath;
		std::map<std::string, double> tilePercentages;
		for (const auto& result : photoData.intersectionResults) {
			tilePercentages[result.tileName] = result.percentage;
		}
		for (const auto& tileName : tileNames) {
			double percentage = tilePercentages[tileName];  // Default to 0 if not found
			outFile << "," << std::fixed << std::setprecision(5) << percentage;
		}
		outFile << std::endl;
	}

	outFile.close();
}