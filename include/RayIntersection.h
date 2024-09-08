#ifndef RAYINTERSECTION_H
#define RAYINTERSECTION_H

#include <osg/Group>
#include <osg/Node>
#include <osg/Vec3d>
#include <vector>
#include <string>
#include <map>

struct IntersectionDetail {
	std::vector<std::string> tileNames; // Tile names that a ray intersects
};

typedef std::map<osg::Vec3d, IntersectionDetail> IntersectionResults;

class RayIntersection {
public:
	RayIntersection(osg::ref_ptr<osg::Group> sceneRoot, const std::map<std::string, osg::BoundingBox>& tileBoundingBoxes);
	IntersectionResults calculateIntersections(const osg::Vec3& cameraCenter, const std::vector<osg::Vec3d>& rays);
private:
	osg::ref_ptr<osg::Group> sceneRoot;
	std::map<std::string, osg::BoundingBox> tileBoundingBoxes;
	osg::ref_ptr<osg::Node> getNodeByName(osg::Group* group, const std::string& name);
};

#endif // RAYINTERSECTION_H
