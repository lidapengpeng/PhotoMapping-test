#ifndef RAY_TILE_INTERSECTIONS_H
#define RAY_TILE_INTERSECTIONS_H

#include <osg/Group>
#include <osgUtil/IntersectVisitor>
#include <osgUtil/LineSegmentIntersector>
#include <map>
#include <vector>
#include <string>
#include "SceneBuilder.h"
#include "PhotoInfoParser.h"

// 定义输出结果的结构体
struct TileIntersectionResult {
	std::string tileName;
	double percentage;
};

struct PhotoData {
	int index;
	std::string imagePath;
	std::vector<TileIntersectionResult> intersectionResults;
};

// 函数声明：计算射线与 Tile 的碰撞检测并返回每个 Tile 的射线占比
std::vector<TileIntersectionResult> performRayTileIntersections(
	const osg::ref_ptr<osg::Group>& scene,
	const std::vector<std::pair<osg::Vec3d, osg::Vec3d>>& pixelRays,
	const std::vector<NamedBoundingBox>& intersectingTiles);

void outputIntersectionResultsToCSV(
	const std::string& filename,
	const std::vector<PhotoData>& allPhotoData,
	const std::vector<std::string>& tileNames);

#endif // RAY_TILE_INTERSECTIONS_H
