#pragma once
#include "BVHBuilders.h"


struct BottomLevelBVH {
	struct TriangleHot {
		Vector3 position_0;
		Vector3 position_edge_1;
		Vector3 position_edge_2;
	} * triangles_hot;

	struct TriangleCold {
		Vector2 tex_coord_0;
		Vector2 tex_coord_edge_1;
		Vector2 tex_coord_edge_2;

		Vector3 normal_0;
		Vector3 normal_edge_1;
		Vector3 normal_edge_2;

		int material_id; // Material id as obtained from the obj file, should not be used directly to index the global Material buffer
	} * triangles_cold;
	
	int triangle_count;

	int material_offset; // Offset in the Material buffer for all Triangles in this BVH

	int * indices;
	int   index_count;
	
	BVHNode * nodes;
	int       node_count;
	
	void init(int count);

	static const BottomLevelBVH * load(const char * filename);

	void trace(const Ray & ray, RayHit & ray_hit, const Matrix4 & world) const;
	SIMD_float intersect(const Ray & ray, SIMD_float max_distance) const;

private:
	void build_bvh (const Triangle * triangles);
	void build_sbvh(const Triangle * triangles);

	void save_to_disk  (const char * bvh_filename) const;
	void load_from_disk(const char * bvh_filename);
	
	void flatten();

	FORCEINLINE void       triangle_trace    (int index, const Ray & ray, RayHit & ray_hit, const Matrix4 & world) const;
	FORCEINLINE SIMD_float triangle_intersect(int index, const Ray & ray, SIMD_float max_distance) const;
};
