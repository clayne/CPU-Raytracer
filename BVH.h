#pragma once
#include <algorithm>

#include "BVHBuilders.h"

template<typename PrimitiveType>
struct BVH {
	PrimitiveType * primitives;
	int             primitive_count;

	int * indices;
	
	int       node_count;
	BVHNode * nodes;

	int leaf_count;

	inline void init(int count) {
		assert(count > 0);

		primitive_count = count; 
		primitives = new PrimitiveType[primitive_count];

		indices = nullptr;

		// Construct Node pool
		nodes = reinterpret_cast<BVHNode *>(ALLIGNED_MALLOC(2 * primitive_count * sizeof(BVHNode), 64));
		assert((unsigned long long)nodes % 64 == 0);
	}

	inline void build_bvh() {
		int * indices_x = new int[primitive_count];
		int * indices_y = new int[primitive_count];
		int * indices_z = new int[primitive_count];

		for (int i = 0; i < primitive_count; i++) {
			indices_x[i] = i;
			indices_y[i] = i;
			indices_z[i] = i;
		}

		std::sort(indices_x, indices_x + primitive_count, [&](int a, int b) { return primitives[a].get_position().x < primitives[b].get_position().x; });
		std::sort(indices_y, indices_y + primitive_count, [&](int a, int b) { return primitives[a].get_position().y < primitives[b].get_position().y; });
		std::sort(indices_z, indices_z + primitive_count, [&](int a, int b) { return primitives[a].get_position().z < primitives[b].get_position().z; });
		
		int * indices_xyz[3] = { indices_x, indices_y, indices_z };

		float * sah  = new float[primitive_count];
		int   * temp = new int[primitive_count];

		node_count = 2;
		BVHBuilders::build_bvh(nodes[0], primitives, indices_xyz, nodes, node_count, 0, primitive_count, sah, temp);

		assert(node_count <= 2 * primitive_count);

		leaf_count = primitive_count;

		// Use indices_x to index the Primitives array, and delete the other two
		indices = indices_x;
		delete [] indices_y;
		delete [] indices_z;

		delete [] temp;
		delete [] sah;
	}

	inline void build_sbvh() {
		const int overallocation = 2; // SBVH requires more space

		int * indices_x = new int[overallocation * primitive_count];
		int * indices_y = new int[overallocation * primitive_count];
		int * indices_z = new int[overallocation * primitive_count];

		for (int i = 0; i < primitive_count; i++) {
			indices_x[i] = i;
			indices_y[i] = i;
			indices_z[i] = i;
		}
		
		std::sort(indices_x, indices_x + primitive_count, [&](int a, int b) { return primitives[a].get_position().x < primitives[b].get_position().x; });
		std::sort(indices_y, indices_y + primitive_count, [&](int a, int b) { return primitives[a].get_position().y < primitives[b].get_position().y; });
		std::sort(indices_z, indices_z + primitive_count, [&](int a, int b) { return primitives[a].get_position().z < primitives[b].get_position().z; });

		int * indices_xyz[3] = { indices_x, indices_y, indices_z };
		
		float * sah     = new float[primitive_count];
		int   * temp[2] = { new int[primitive_count], new int[primitive_count] };

		AABB root_aabb = BVHPartitions::calculate_bounds(primitives, indices_xyz[0], 0, primitive_count);

		node_count = 2;
		leaf_count = BVHBuilders::build_sbvh(nodes[0], primitives, indices_xyz, nodes, node_count, 0, primitive_count, sah, temp, 1.0f / root_aabb.surface_area(), root_aabb);

		printf("SBVH Leaf count: %i\n", leaf_count);

		assert(node_count <= 2 * primitive_count);

		// Use indices_x to index the Primitives array, and delete the other two
		indices = indices_x;
		delete [] indices_y;
		delete [] indices_z;

		delete [] temp[0];
		delete [] temp[1];
		delete [] sah;
	}

	inline void save_to_disk(const char * bvh_filename) const {
		FILE * file;
		fopen_s(&file, bvh_filename, "wb");

		if (file == nullptr) abort();

		fwrite(reinterpret_cast<const char *>(&primitive_count), sizeof(int), 1, file);
		fwrite(reinterpret_cast<const char *>(primitives), sizeof(PrimitiveType), primitive_count, file);

		fwrite(reinterpret_cast<const char *>(&node_count), sizeof(int), 1, file);
		fwrite(reinterpret_cast<const char *>(nodes), sizeof(BVHNode), node_count, file);

		fwrite(reinterpret_cast<const char *>(&leaf_count), sizeof(int), 1, file);
		
		fwrite(reinterpret_cast<const char *>(indices), sizeof(int), leaf_count, file);

		fclose(file);
	}

	inline void load_from_disk(const char * bvh_filename) {
		FILE * file;
		fopen_s(&file, bvh_filename, "rb"); 
		
		if (file == nullptr) abort();

		fread(reinterpret_cast<char *>(&primitive_count), sizeof(int), 1, file);

		primitives = new PrimitiveType[primitive_count];
		fread(reinterpret_cast<char *>(primitives), sizeof(PrimitiveType), primitive_count, file);
		
		fread(reinterpret_cast<char *>(&node_count), sizeof(int), 1, file);

		nodes = new BVHNode[node_count];
		fread(reinterpret_cast<char *>(nodes), sizeof(BVHNode), node_count, file);

		fread(reinterpret_cast<char *>(&leaf_count), sizeof(int), 1, file);
			
		indices = new int[leaf_count];
		fread(reinterpret_cast<char *>(indices), sizeof(int), leaf_count, file);

		fclose(file);
	}

	inline void update() const {
		for (int i = 0; i < primitive_count; i++) {
			primitives[i].update();
		}
	}

	inline void trace(const Ray & ray, RayHit & ray_hit, const Matrix4 & world) const {
		int stack[128];
		int stack_size = 1;

		// Push root on stack
		stack[0] = 0;

		int step = 0;

		while (stack_size > 0) {
			// Pop Node of the stack
			const BVHNode & node = nodes[stack[--stack_size]];

			SIMD_float mask = node.aabb.intersect(ray, ray_hit.distance);
			if (SIMD_float::all_false(mask)) continue;

			if (node.is_leaf()) {
				for (int i = node.first; i < node.first + node.count; i++) {
					primitives[indices[i]].trace(ray, ray_hit, world, step);
				}
			} else {
				if (node.should_visit_left_first(ray)) {
					stack[stack_size++] = node.left + 1;
					stack[stack_size++] = node.left;
				} else {
					stack[stack_size++] = node.left;
					stack[stack_size++] = node.left + 1;
				}
			}

			step++;
		}
	}

	inline SIMD_float intersect(const Ray & ray, SIMD_float max_distance) const {
		int stack[128];
		int stack_size = 1;

		// Push root on stack
		stack[0] = 0;

		int step = 0;

		SIMD_float hit(0.0f);

		while (stack_size > 0) {
			// Pop Node of the stack
			const BVHNode & node = nodes[stack[--stack_size]];

			SIMD_float mask = node.aabb.intersect(ray, max_distance);
			if (SIMD_float::all_false(mask)) continue;

			if (node.is_leaf()) {
				for (int i = node.first; i < node.first + node.count; i++) {
					hit = hit | primitives[indices[i]].intersect(ray, max_distance);

					if (SIMD_float::all_true(hit)) return hit;
				}
			} else {
				if (node.should_visit_left_first(ray)) {
					stack[stack_size++] = node.left + 1;
					stack[stack_size++] = node.left;
				} else {
					stack[stack_size++] = node.left;
					stack[stack_size++] = node.left + 1;
				}
			}

			step++;
		}

		return hit;
	}
};
