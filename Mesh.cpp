#include "Mesh.h"

#include <immintrin.h>

#include "Math.h"

void Mesh::update() {
	transform.calc_world_matrix();

	for (int i = 0; i < mesh_data->triangle_count; i++) {
		// For every three Vertices, the first Vector is an actual vertex position, the two Vectors after that are the Triangle edges
		triangle_bvh.primitives[i].position0 = Matrix4::transform_position(transform.world_matrix, mesh_data->triangles[i].position0);
		triangle_bvh.primitives[i].position1 = Matrix4::transform_position(transform.world_matrix, mesh_data->triangles[i].position1);
		triangle_bvh.primitives[i].position2 = Matrix4::transform_position(transform.world_matrix, mesh_data->triangles[i].position2);

		triangle_bvh.primitives[i].normal0 = Matrix4::transform_direction(transform.world_matrix, mesh_data->triangles[i].normal0);
		triangle_bvh.primitives[i].normal1 = Matrix4::transform_direction(transform.world_matrix, mesh_data->triangles[i].normal1);
		triangle_bvh.primitives[i].normal2 = Matrix4::transform_direction(transform.world_matrix, mesh_data->triangles[i].normal2);
	}
}

void Mesh::trace(const Ray & ray, RayHit & ray_hit) const {
	triangle_bvh.trace(ray, ray_hit);
}

SIMD_float Mesh::intersect(const Ray & ray, SIMD_float max_distance) const {	
	return triangle_bvh.intersect(ray, max_distance);
}
