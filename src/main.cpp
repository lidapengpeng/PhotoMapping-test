#include <iostream>
#include <osg/MatrixTransform>
#include <osgViewer/Viewer>
#include <osg/Geometry>
#include <osg/LineWidth>
#include <osg/Material>
#include <osg/ShapeDrawable>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>
#include "SceneBuilder.h"
#include "PhotoInfoParser.h"
#include "Camera.h"
#include "RayIntersection.h"
#include "TileIntersectionCalculator.h"
#include <unordered_set>
#include <fstream>

std::unordered_set<int> loadPhotoIndices(const std::string& filePath)
{
	std::unordered_set<int> indices;
	std::ifstream file(filePath);
	std::string line;
	if (file.is_open())
	{
		while (getline(file, line))
		{
			indices.insert(std::stoi(line));
		}
		file.close();
	}
	else
	{
		std::cerr << "Unable to open file: " << filePath << std::endl;
	}
	return indices;
}

// 输出交集结果到CSV文件
static std::vector<std::string> extractTileNames(const std::vector<NamedBoundingBox>& tileBoundingBoxes)
{
	std::vector<std::string> tileNames;
	for (const NamedBoundingBox& box : tileBoundingBoxes)
	{
		tileNames.push_back(box.name);
	}
	return tileNames;
}

// 设置全局互斥锁
std::mutex allIntersectionResultsMutex;

// 处理单张照片，生成包含射线和相机中心球体的场景
static osg::ref_ptr<osg::Group> processPhoto(const PhotoInfo& photoInfo, int photoIndex, osg::ref_ptr<osg::Group> scene,
                                             double heightThreshold,
                                             const std::vector<NamedBoundingBox>& tileBoundingBoxes,
                                             std::vector<PhotoData>& allPhotoData)
{
	osg::ref_ptr<osg::Group> localScene = new osg::Group();

	Camera camera(photoInfo);

	// 判断照片高度~如果高于阈值，则不处理,-camera.getCameraCenter().y()代表相机中心的高度值
	if (-camera.getCameraCenter().y() > (heightThreshold + 30))
	{
		return localScene;
	}
	// 创建视椎体
	osg::ref_ptr<osg::MatrixTransform> frustumTransform = camera.createFrustumGeometry(heightThreshold);
	localScene->addChild(frustumTransform);
	// 可视化视锥体边界框
	osg::BoundingBox frustumBBox = camera.calculateFrustumBoundingBox(frustumTransform);
	osg::ref_ptr<osg::Geode> frustumBBoxGeode = camera.createBoundingBoxDrawable(frustumBBox);
	localScene->addChild(frustumBBoxGeode);
	// 计算与视锥体相交的边界框
	std::vector<NamedBoundingBox> intersectingTiles = camera.calculateIntersectingTiles(frustumBBox, tileBoundingBoxes);
	// 计算射线
	std::vector<std::pair<osg::Vec3d, osg::Vec3d>> pixelRays = camera.calculatePartialPixelRays(128, 5.0);
	std::cout << "Calculated " << pixelRays.size() << " rays for photo: " << photoInfo.imagePath << std::endl;
	// 计算射线与边界框
	//std::vector<TileIntersectionResult> intersectionResults = performRayTileIntersections(scene, pixelRays, intersectingTiles);
	// 全局互斥锁
	//PhotoData data;
	//data.index = photoIndex;
	//data.imagePath = photoInfo.imagePath;
	//data.intersectionResults = intersectionResults;
	//{
	//    std::lock_guard<std::mutex> lock(allIntersectionResultsMutex);
	//    allPhotoData.push_back(data);
	//}
	// 绘制射线
	for (const auto& ray : pixelRays)
	{
		osg::ref_ptr<osg::Geometry> rayGeometry = new osg::Geometry();
		osg::ref_ptr<osg::Vec3Array> rayVertices = new osg::Vec3Array();

		// ray.first 是起点，ray.second 是终点
		rayVertices->push_back(ray.first);
		rayVertices->push_back(ray.second);
		rayGeometry->setVertexArray(rayVertices);

		osg::ref_ptr<osg::DrawArrays> drawArrays = new osg::DrawArrays(GL_LINES, 0, 2);
		rayGeometry->addPrimitiveSet(drawArrays);

		osg::ref_ptr<osg::Geode> rayGeode = new osg::Geode();
		rayGeode->addDrawable(rayGeometry);
		localScene->addChild(rayGeode);
	}
	return localScene;
}

int main()
{
	try
	{
		auto startTime = std::chrono::high_resolution_clock::now();
		// 解析照片信息
		std::string xmlFile = "data/images/weizi.xml";
		PhotoInfoParser parser(xmlFile);
		std::vector<PhotoInfo> photoInfos = parser.parsePhotoInfo();
		if (photoInfos.empty())
		{
			std::cerr << "No photo information found." << std::endl;
			return 1;
		}
		std::cout << "Parsed " << photoInfos.size() << " photos." << std::endl;

		// 构建场景和边界框
		SceneBuilder builder;
		osg::ref_ptr<osg::Group> scene = builder.buildScene("data/mesh");
		// 创建边界框几何

		osg::ref_ptr<osg::Group> bboxGeometry = builder.createBoundingBoxGeometry();
		scene->addChild(bboxGeometry);
		builder.printTileBoundingBoxes();
		std::cout << "Scene built." << std::endl;

		std::vector<NamedBoundingBox> tileBoundingBoxes = builder.getTileBoundingBoxes();
		// 计算高度阈值
		double heightThreshold = builder.calculateHeightThreshold();
		// 存储所有照片的结果
		std::vector<PhotoData> allPhotoData;

		std::unordered_set<int> photoIndices = loadPhotoIndices(
			"C://Users//Admin//Desktop//PhotoMapping//PhotoMapping//images//Tile_0016_0020//photo_indices.txt");

		// 处理所有照片
		std::vector<std::thread> threads;
		std::mutex sceneMutex; // Mutex for scene synchronization photoInfos.size()
		for (int photoIndex = 655; photoIndex < 656; ++photoIndex) {
		    threads.emplace_back([&photoInfos, photoIndex, &scene, &sceneMutex, heightThreshold, &tileBoundingBoxes, &allPhotoData]() {
		        auto localScene = processPhoto(photoInfos[photoIndex], photoIndex, scene, heightThreshold, tileBoundingBoxes, allPhotoData);
		        std::lock_guard<std::mutex> lock(sceneMutex);
		        scene->addChild(localScene);
		        });
		}
		// 等待所有线程完成
		for (auto& thread : threads)
		{
			if (thread.joinable())
			{
				thread.join();
			}
		}

		// 获取所有瓦片名称
		//std::vector<std::string> tileNames = extractTileNames(tileBoundingBoxes);
		// 输出交集结果到CSV文件
		//outputIntersectionResultsToCSV("output.csv", allPhotoData, tileNames);

		// 设置背景色并运行观察器
		osgViewer::Viewer viewer;
		viewer.setSceneData(scene);
		viewer.getCamera()->setClearColor(osg::Vec4(1.0, 1.0, 1.0, 1.0));
		viewer.setUpViewInWindow(100, 100, 800, 600);
		// 设置总时间
		auto endTime = std::chrono::high_resolution_clock::now();
		auto totalTime = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
		std::cout << "Total time: " << totalTime << " seconds." << std::endl;
		return viewer.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
}
