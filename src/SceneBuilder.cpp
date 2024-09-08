#include "SceneBuilder.h"
#include <osgDB/ReadFile>
#include <osg/Geode>
#include <iostream>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <osg/LineWidth>
#include <osg/Geometry>
#include <thread>
#include <mutex>
#include <limits>
#include <vector>
#include <osgUtil/IntersectionVisitor>
#include <osgUtil/LineSegmentIntersector>
#include <osg/MatrixTransform>

BBoxPrinter::BBoxPrinter() : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {}

void BBoxPrinter::apply(osg::Geode& geode) {
	osg::BoundingBox bbox = geode.getBoundingBox();
	totalBoundingBox.expandBy(bbox);
	traverse(geode);
}

osg::BoundingBox BBoxPrinter::getTotalBoundingBox() const {
	return totalBoundingBox;
}

SceneBuilder::SceneBuilder() {}

static std::string removeTrailingSlash(const std::string& path) {
	if (!path.empty() && path.back() == '/') {
		return path.substr(0, path.size() - 1);
	}
	return path;
}

static std::string replaceBackslashes(const std::string& path) {
	std::string correctedPath = path;
	std::replace(correctedPath.begin(), correctedPath.end(), '\\', '/');
	return correctedPath;
}

osg::ref_ptr<osg::Group> SceneBuilder::buildScene(const std::string& meshFolderPath) {
	osg::ref_ptr<osg::Group> root = new osg::Group();
	DIR* dir;
	struct dirent* ent;
	std::mutex mutex;
	std::vector<std::thread> threads;

	std::string correctedMeshFolderPath = replaceBackslashes(removeTrailingSlash(meshFolderPath));
	if ((dir = opendir(correctedMeshFolderPath.c_str())) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			std::string entryName(ent->d_name);
			if (entryName == "." || entryName == "..") continue;
			std::string tileFolderPath = correctedMeshFolderPath + "/" + entryName;
			struct stat path_stat;
			stat(tileFolderPath.c_str(), &path_stat);
			if (!S_ISDIR(path_stat.st_mode)) {
				continue; // Not a directory, skip this entry
			}

			threads.emplace_back([&, tileFolderPath, entryName] {
				DIR* tileDir = opendir(tileFolderPath.c_str());
				if (tileDir) {
					struct dirent* tileEnt;
					while ((tileEnt = readdir(tileDir)) != NULL) {
						std::string tileEntryName(tileEnt->d_name);
						if (tileEntryName == "." || tileEntryName == "..") continue;
						if (tileEntryName.substr(tileEntryName.find_last_of(".") + 1) == "obj") {
							std::string objFilePath = tileFolderPath + "/" + tileEntryName;
							osg::ref_ptr<osg::Node> tileNode = osgDB::readNodeFile(objFilePath);
							if (tileNode) {
								BBoxPrinter bboxPrinter;
								tileNode->accept(bboxPrinter);
								osg::BoundingBox bbox = bboxPrinter.getTotalBoundingBox();

								std::lock_guard<std::mutex> lock(mutex);
								tileBoundingBoxes.push_back({ entryName, bbox });

								tileNode->setName(entryName); // Set the name of the tile node
								std::cout << "Adding tile node: " << tileNode->getName() << std::endl; // Debug print
								root->addChild(tileNode);
							}
						}
					}
					closedir(tileDir);
				}
				});
		}
		closedir(dir);
	}

	for (auto& thread : threads) {
		thread.join();
	}

	return root;
}

const std::vector<NamedBoundingBox>& SceneBuilder::getTileBoundingBoxes() const {
	return tileBoundingBoxes;
}

void SceneBuilder::printTileBoundingBoxes() const {
	for (const auto& item : tileBoundingBoxes) {
		const std::string& tileName = item.name;
		const osg::BoundingBox& bbox = item.bbox;
		std::cout << "Tile: " << tileName << " BBox: ["
			<< bbox.xMin() << ", " << bbox.yMin() << ", " << bbox.zMin() << "] - ["
			<< bbox.xMax() << ", " << bbox.yMax() << ", " << bbox.zMax() << "]"
			<< std::endl;
	}
}

osg::ref_ptr<osg::Group> SceneBuilder::createBoundingBoxGeometry() {
	osg::ref_ptr<osg::Group> bboxGroup = new osg::Group();

	for (const auto& item : tileBoundingBoxes) {
		const osg::BoundingBox& bbox = item.bbox;

		osg::ref_ptr<osg::Geometry> bboxGeometry = new osg::Geometry();
		osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();

		// Bottom face
		vertices->push_back(osg::Vec3(bbox.xMin(), bbox.yMin(), bbox.zMin()));
		vertices->push_back(osg::Vec3(bbox.xMax(), bbox.yMin(), bbox.zMin()));
		vertices->push_back(osg::Vec3(bbox.xMax(), bbox.yMax(), bbox.zMin()));
		vertices->push_back(osg::Vec3(bbox.xMin(), bbox.yMax(), bbox.zMin()));
		vertices->push_back(osg::Vec3(bbox.xMin(), bbox.yMin(), bbox.zMin()));

		// Top face
		vertices->push_back(osg::Vec3(bbox.xMin(), bbox.yMin(), bbox.zMax()));
		vertices->push_back(osg::Vec3(bbox.xMax(), bbox.yMin(), bbox.zMax()));
		vertices->push_back(osg::Vec3(bbox.xMax(), bbox.yMax(), bbox.zMax()));
		vertices->push_back(osg::Vec3(bbox.xMin(), bbox.yMax(), bbox.zMax()));
		vertices->push_back(osg::Vec3(bbox.xMin(), bbox.yMin(), bbox.zMax()));

		// Connect top and bottom faces
		vertices->push_back(osg::Vec3(bbox.xMax(), bbox.yMin(), bbox.zMax()));
		vertices->push_back(osg::Vec3(bbox.xMax(), bbox.yMin(), bbox.zMin()));
		vertices->push_back(osg::Vec3(bbox.xMax(), bbox.yMax(), bbox.zMin()));
		vertices->push_back(osg::Vec3(bbox.xMax(), bbox.yMax(), bbox.zMax()));
		vertices->push_back(osg::Vec3(bbox.xMin(), bbox.yMax(), bbox.zMax()));
		vertices->push_back(osg::Vec3(bbox.xMin(), bbox.yMax(), bbox.zMin()));

		bboxGeometry->setVertexArray(vertices.get());
		osg::ref_ptr<osg::DrawArrays> drawArrays = new osg::DrawArrays(GL_LINE_STRIP, 0, vertices->size());
		bboxGeometry->addPrimitiveSet(drawArrays.get());

		osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
		colors->push_back(osg::Vec4(1.0, 0.0, 0.0, 1.0)); // Red
		bboxGeometry->setColorArray(colors.get(), osg::Array::BIND_OVERALL);

		osg::ref_ptr<osg::Geode> geode = new osg::Geode();
		geode->addDrawable(bboxGeometry.get());

		bboxGroup->addChild(geode.get());
	}

	return bboxGroup;
}

double SceneBuilder::calculateHeightThreshold() const {
	if (tileBoundingBoxes.empty()) return 0.0; // 如果没有边界框数据，则返回 0

	// 从第一个边界框的最小 z 值开始，初始化 minZ
	double minY = tileBoundingBoxes.front().bbox.yMin();
	// 遍历所有边界框，找到最小的 z 值
	for (const auto& namedBox : tileBoundingBoxes) {
		if (namedBox.bbox.yMin() < minY) {
			minY = namedBox.bbox.yMin(); // 更新最小值
		}
	}

	std::cout << "Calculated Global Minimum Z Value: " << -minY << std::endl;
	return -minY; // 返回计算得到的最小 z 值
}