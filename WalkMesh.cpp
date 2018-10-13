#include "WalkMesh.hpp"

WalkMesh(std::vector< glm::vec3 > const &vertices_, std::vector< glm::uvec3 > const &triangles_)
	: vertices(vertices_), triangles(triangles_) {
	//construct next_vertex map
  for (auto &tri : triangles) {
    next_vertex.insert(glm::uvec2(tri.x, tri.y), tri.z);
    next_vertex.insert(glm::uvec2(tri.y, tri.z), tri.x);
    next_vertex.insert(glm::uvec2(tri.z, tri.x), tri.y);
  }
}

WalkPoint WalkMesh::start(glm::vec3 const &world_point) const {
	WalkPoint closest;
  //iterate through triangles
  for (auto &tri : triangles) {
    //TODO: for each triangle, find closest point on triangle to world_point
	  //TODO: if point is closest, closest.triangle gets the current triangle, closest.weights gets the barycentric coordinates
    /*if (false) {
      closest.triangle = tri;
      closest.weights = ;
    }*/
  }
	return closest;
}

void WalkMesh::walk(WalkPoint &wp, glm::vec3 const &step) const {
	//TODO: project step to barycentric coordinates to get weights_step
	glm::vec3 weights_step;

	//TODO: when does wp.weights + t * weights_step cross a triangle edge?
	float t = 1.0f;

	if (t >= 1.0f) { //if a triangle edge is not crossed
		//TODO: wp.weights gets moved by weights_step, nothing else needs to be done.

	} else { //if a triangle edge is crossed
		//TODO: wp.weights gets moved to triangle edge, and step gets reduced
		//if there is another triangle over the edge:
			//TODO: wp.triangle gets updated to adjacent triangle
			//TODO: step gets rotated over the edge
		//else if there is no other triangle over the edge:
			//TODO: wp.triangle stays the same.
			//TODO: step gets updated to slide along the edge
	}
}
