#ifndef SCENEBUILDER_H
#define SCENEBUILDER_H

#include <osg/Group>
#include <osg/NodeVisitor>
#include <osg/BoundingBox>
#include <string>
#include <map>

// 定义用于存储命名包围盒的结构体
struct NamedBoundingBox {
	std::string name;
	osg::BoundingBox bbox;
};

// 定义用于打印和计算包围盒的访问器类
class BBoxPrinter : public osg::NodeVisitor {
public:
	BBoxPrinter();
	virtual void apply(osg::Geode& geode);
	osg::BoundingBox getTotalBoundingBox() const;

private:
	osg::BoundingBox totalBoundingBox;
};

// 定义场景构建类
class SceneBuilder {
public:
	SceneBuilder();
	osg::ref_ptr<osg::Group> buildScene(const std::string& meshFolderPath);
	void printTileBoundingBoxes() const;
	osg::ref_ptr<osg::Group> createBoundingBoxGeometry();
	double calculateHeightThreshold() const;
	const std::vector<NamedBoundingBox>& getTileBoundingBoxes() const;
private:
	std::vector<NamedBoundingBox> tileBoundingBoxes;
};

#endif // SCENEBUILDER_H
