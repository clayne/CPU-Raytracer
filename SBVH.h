#pragma once
#include <algorithm>

#include "BVHPartitions.h"

#include "ScopedTimer.h"

#define SBVH_CONSTRUCT_MEDIAN   0 // Split using median Primitive along the longest axis
#define SBVH_CONSTRUCT_FULL_SAH 1 // Evaluate SAH for every Primtive to determine if we should split there

#define SBVH_CONSTRUCTION_STRATEGY SBVH_CONSTRUCT_FULL_SAH

#define SBVH_TRAVERSE_BRUTE_FORCE  0 // Doesn't use the tree structure of the BVH, checks every Primitive for every Ray
#define SBVH_TRAVERSE_TREE_NAIVE   1 // Traverses the BVH in a naive way, always checking the left Node before the right Node
#define SBVH_TRAVERSE_TREE_ORDERED 2 // Traverses the BVH based on the split axis and the direction of the Ray

#define SBVH_TRAVERSAL_STRATEGY SBVH_TRAVERSE_TREE_ORDERED

#define SBVH_AXIS_X_BITS 0x40000000 // 01 00 zeroes...
#define SBVH_AXIS_Y_BITS 0x80000000 // 10 00 zeroes...
#define SBVH_AXIS_Z_BITS 0xc0000000 // 11 00 zeroes...
#define SBVH_AXIS_MASK   0xc0000000 // 11 00 zeroes...

struct SBVHNode {
	AABB aabb;
	union {  // A Node can either be a leaf or have children. A leaf Node means count = 0
		int left;  // Left contains index of left child if the current Node is not a leaf Node
		int first; // First constains index of first primtive if the current Node is a leaf Node
	};
	int count; // Stores split axis in its 2 highest bits, count in its lowest 30 bits

	inline int subdivide(const Triangle * triangles, int * indices[3], SBVHNode nodes[], int & node_index, int first_index, int index_count, float * sah, int * temp[2], float inv_root_surface_area, AABB node_aabb) {
		aabb = node_aabb;

		if (index_count < 3) {
			// Leaf Node, terminate recursion
			first = first_index;
			count = index_count;
			
			return count;
		}
		
		left = node_index;
		node_index += 2;

		// Object Split information
		float full_sah_split_cost;
		int   full_sah_split_dimension = -1;
		AABB  full_sah_aabb_left;
		AABB  full_sah_aabb_right;
		int   full_sah_split_index = BVHPartitions::partition_object(triangles, indices, first_index, index_count, sah, full_sah_split_dimension, full_sah_split_cost, node_aabb, full_sah_aabb_left, full_sah_aabb_right);

		// Spatial Split information
		float spatial_split_cost = INFINITY;
		int   spatial_split_dimension = -1;
		float spatial_split_plane_distance;
		int   spatial_split_bin = -1;
		AABB  spatial_split_aabb_left;
		AABB  spatial_split_aabb_right;
		int   spatial_split_count_left;
		int   spatial_split_count_right;

		// Calculate the overlap between the child bounding boxes resulting from the Object Split
		float lamba = AABB::overlap_surface_area(full_sah_aabb_left, full_sah_aabb_right);

		// Alpha == 1 means regular BVH, Alpha == 0 means full SBVH
		const float alpha = 10e-5; 

		// Divide by the surface area of the bounding box of the root Node
		float ratio = lamba * inv_root_surface_area;
		
		assert(ratio >= 0.0f && ratio <= 1.0f);

		// If ratio between overlap area and root area is large enough, consider a Spatial Split
		if (ratio > alpha) { 
			spatial_split_bin = BVHPartitions::partition_spatial(triangles, indices, first_index, index_count, sah, spatial_split_dimension, spatial_split_cost, spatial_split_plane_distance, spatial_split_aabb_left, spatial_split_aabb_right, spatial_split_count_left, spatial_split_count_right, node_aabb);
		}

		// Check SAH termination condition
		float parent_cost = aabb.surface_area() * float(index_count); 
		if (parent_cost <= full_sah_split_cost && parent_cost <= spatial_split_cost) {
			first = first_index;
			count = index_count;
			
			return count;
		} 

		// From this point on it is decided that this Node will NOT be a leaf Node
		count = (full_sah_split_dimension + 1) << 30;
		
		int * children_left [3] { indices[0] + first_index, indices[1] + first_index, indices[2] + first_index };
		int * children_right[3] { new int[index_count],     new int[index_count],     new int[index_count]     };

		int children_left_count [3] = { 0, 0, 0 };
		int children_right_count[3] = { 0, 0, 0 };

		int n_left, n_right;

		AABB child_aabb_left;
		AABB child_aabb_right;

		if (full_sah_split_cost <= spatial_split_cost) {
			// Perform Object Split

			// Obtain split plane
			float split = triangles[indices[full_sah_split_dimension][full_sah_split_index]].get_position()[full_sah_split_dimension];

			for (int dimension = 0; dimension < 3; dimension++) {
				for (int i = first_index; i < first_index + index_count; i++) {
					int index = indices[dimension][i];

					bool goes_left = triangles[index].get_position()[full_sah_split_dimension] < split;

					if (triangles[index].get_position()[full_sah_split_dimension] == split) {
						// In case the current primitive has the same coordianate as the one we split on along the split dimension,
						// We don't know whether the primitive should go left or right.
						// In this case check all primitive indices on the left side of the split that 
						// have the same split coordinate for equality with the current primitive index i

						int j = full_sah_split_index - 1;
						// While we can go left and the left primitive has the same coordinate along the split dimension as the split itself
						while (j >= first_index && triangles[indices[full_sah_split_dimension][j]].get_position()[full_sah_split_dimension] == split) {
							if (indices[full_sah_split_dimension][j] == index) {
								goes_left = true;

								break;
							}

							j--;
						}
					}

					if (goes_left) {
						children_left[dimension][children_left_count[dimension]++] = index;
					} else {
						children_right[dimension][children_right_count[dimension]++] = index;
					}
				}
			}
			
			// We should have made the same decision (going left/right) in every dimension
			assert(children_left_count [0] == children_left_count [1] && children_left_count [1] == children_left_count [2]);
			assert(children_right_count[0] == children_right_count[1] && children_right_count[1] == children_right_count[2]);
			
			n_left  = children_left_count [0];
			n_right = children_right_count[0];

			// Using object split, no duplicates can occur. 
			// Thus, left + right should equal the total number of triangles
			assert(first_index + n_left == full_sah_split_index);
			assert(n_left + n_right == index_count);
			
			child_aabb_left  = full_sah_aabb_left;
			child_aabb_right = full_sah_aabb_right;
		} else {
			// Perform Spatial Split

			float bounds_min = node_aabb.min[spatial_split_dimension] - 0.001f;
			float bounds_max = node_aabb.max[spatial_split_dimension] + 0.001f;
			
			float inv_bounds_delta = 1.0f / (bounds_max - bounds_min);

			// The two temp arrays will be used as lookup tables
			int * indices_going_left  = temp[0];
			int * indices_going_right = temp[1];

			int temp_left = 0, temp_right = 0;
			
			// Keep track of amount of rejected references on both sides for debugging purposes
			int rejected_left  = 0;
			int rejected_right = 0;

			float n_1 = float(spatial_split_count_left);
			float n_2 = float(spatial_split_count_right);

			for (int i = first_index; i < first_index + index_count; i++) {
				int index = indices[spatial_split_dimension][i];
				const Triangle & triangle = triangles[index];
				
				int bin_min = int(BVHPartitions::SBVH_BIN_COUNT * ((triangle.aabb.min[spatial_split_dimension] - bounds_min) * inv_bounds_delta));
				int bin_max = int(BVHPartitions::SBVH_BIN_COUNT * ((triangle.aabb.max[spatial_split_dimension] - bounds_min) * inv_bounds_delta));

				bin_min = Math::clamp(bin_min, 0, BVHPartitions::SBVH_BIN_COUNT - 1);
				bin_max = Math::clamp(bin_max, 0, BVHPartitions::SBVH_BIN_COUNT - 1);

				bool goes_left  = false;
				bool goes_right = false;

				if (bin_max < spatial_split_bin) {
					goes_left = true;
				} else if (bin_min >= spatial_split_bin) {
					goes_right = true;
				} else { 
					// Straddler
					
					// A split can result in triangles that lie on one side of the plane but that don't overlap the AABB
					bool valid_left  = AABB::overlap(triangle.aabb, spatial_split_aabb_left ).is_valid();
					bool valid_right = AABB::overlap(triangle.aabb, spatial_split_aabb_right).is_valid();

					if (valid_left && valid_right) {
						goes_left  = true;
						goes_right = true;

						// Consider unsplitting
						AABB delta_left  = spatial_split_aabb_left;
						AABB delta_right = spatial_split_aabb_right;

						delta_left.expand (triangle.aabb);
						delta_right.expand(triangle.aabb);

						float spatial_split_aabb_left_surface_area  = spatial_split_aabb_left.surface_area();
						float spatial_split_aabb_right_surface_area = spatial_split_aabb_right.surface_area();

						float c_split = spatial_split_aabb_left_surface_area   *  n_1       + spatial_split_aabb_right_surface_area   *  n_2;
						float c_1     =              delta_left.surface_area() *  n_1       + spatial_split_aabb_right_surface_area   * (n_2-1.0f);
						float c_2     = spatial_split_aabb_left_surface_area   * (n_1-1.0f) +              delta_right.surface_area() *  n_2;

						// If C_1 resp. C_2 is cheapest, let the triangle go left resp. right
						// Otherwise, do nothing and let the triangle go both left and right
						if (c_1 < c_split) {
							if (c_2 < c_1) {
								// C_2 is cheapest, remove from left
								goes_left = false;
								rejected_left++;

								n_1 -= 1.0f;

								spatial_split_aabb_right.expand(triangle.aabb);
							} else {
								// C_1 is cheapest, remove from right
								goes_right = false;
								rejected_right++;
								
								n_2 -= 1.0f;

								spatial_split_aabb_left.expand(triangle.aabb);
							}
						} else if (c_2 < c_split) {
							// C_2 is cheapest, remove from left
							goes_left = false;
							rejected_left++;
							
							n_1 -= 1.0f;

							spatial_split_aabb_right.expand(triangle.aabb);
						}
					} else if (valid_left) {
						goes_left = true;

						rejected_right++;
					} else if (valid_right) {
						goes_right = true;

						rejected_left++;
					} else {
						goes_left  = false;
						goes_right = false;

						rejected_left++;
						rejected_right++;
					}
				}

				// Triangle must go left, right, or both
				assert(goes_left || goes_right);

				indices_going_left [index] = goes_left;
				indices_going_right[index] = goes_right;
			}

			// In all three dimensions, use the lookup table to decide which way each Triangle should go
			for (int dimension = 0; dimension < 3; dimension++) {	
				for (int i = first_index; i < first_index + index_count; i++) {
					int index = indices[dimension][i];

					bool goes_left  = indices_going_left [index];
					bool goes_right = indices_going_right[index];

					if (goes_left)  children_left [dimension][children_left_count [dimension]++] = index;
					if (goes_right) children_right[dimension][children_right_count[dimension]++] = index;
				}
			}

			// We should have made the same decision (going left/right) in every dimension
			assert(children_left_count [0] == children_left_count [1] && children_left_count [1] == children_left_count [2]);
			assert(children_right_count[0] == children_right_count[1] && children_right_count[1] == children_right_count[2]);
			
			n_left  = children_left_count [0];
			n_right = children_right_count[0];
			
			// The actual number of references going left/right should match the numbers calculated during spatial splitting
			assert(n_left  == spatial_split_count_left  - rejected_left);
			assert(n_right == spatial_split_count_right - rejected_right);

			// A valid partition contains at least one and strictly less than all
			assert(n_left  > 0 && n_left  < index_count);
			assert(n_right > 0 && n_right < index_count);
			
			// Make sure no triangles dissapeared
			assert(n_left + n_right >= index_count);
			
			child_aabb_left  = spatial_split_aabb_left;
			child_aabb_right = spatial_split_aabb_right;
		}
		
		// Do a depth first traversal, so that we know the amount of indices that were recursively created by the left child
		int offset_left = nodes[left].subdivide(triangles, indices, nodes, node_index, first_index, n_left, sah, temp, inv_root_surface_area, child_aabb_left);

		// Using the depth first offset, we can now copy over the right references
		memcpy(indices[0] + first_index + offset_left, children_right[0], n_right * sizeof(int));
		memcpy(indices[1] + first_index + offset_left, children_right[1], n_right * sizeof(int));
		memcpy(indices[2] + first_index + offset_left, children_right[2], n_right * sizeof(int));
			
		// Now recurse on the right side
		int offset_right = nodes[left + 1].subdivide(triangles, indices, nodes, node_index, first_index + offset_left, n_right, sah, temp, inv_root_surface_area, child_aabb_right);
		
		delete [] children_right[0];
		delete [] children_right[1];
		delete [] children_right[2];
		
		// Report the total number of leaves contained in the subtree
		return offset_left + offset_right;
	}
	
	// The current Node is a leaf if its primitive count is larger than zero
	inline bool is_leaf() const {
		return (count & (~SBVH_AXIS_MASK)) > 0;
	}

	inline bool should_visit_left_first(const Ray & ray) const {
#if SBVH_TRAVERSAL_STRATEGY == SBVH_TRAVERSE_TREE_NAIVE
		return true;
#elif SBVH_TRAVERSAL_STRATEGY == SBVH_TRAVERSE_TREE_ORDERED
		switch (count & SBVH_AXIS_MASK) {
			case SBVH_AXIS_X_BITS: return ray.direction.x[0] > 0.0f;
			case SBVH_AXIS_Y_BITS: return ray.direction.y[0] > 0.0f;
			case SBVH_AXIS_Z_BITS: return ray.direction.z[0] > 0.0f;

			default: abort();
		}
#endif
	}

	inline void trace(const Triangle * primitives, const int * indices, const SBVHNode nodes[], const Ray & ray, RayHit & ray_hit, int step) const {
		SIMD_float mask = aabb.intersect(ray, ray_hit.distance);
		if (SIMD_float::all_false(mask)) return;

		if (is_leaf()) {
			for (int i = first; i < first + count; i++) {
				primitives[indices[i]].trace(ray, ray_hit, step + count);
			}
		} else {
			if (should_visit_left_first(ray)) {
				// Visit left Node first, then visit right Node
				nodes[left    ].trace(primitives, indices, nodes, ray, ray_hit, step + 1);
				nodes[left + 1].trace(primitives, indices, nodes, ray, ray_hit, step + 1);
			} else {
				// Visit right Node first, then visit left Node
				nodes[left + 1].trace(primitives, indices, nodes, ray, ray_hit, step + 1);
				nodes[left    ].trace(primitives, indices, nodes, ray, ray_hit, step + 1);
			}
		}
	}

	inline SIMD_float intersect(const Triangle * primitives, const int * indices, const SBVHNode nodes[], const Ray & ray, SIMD_float max_distance) const {
		SIMD_float mask = aabb.intersect(ray, max_distance);
		if (SIMD_float::all_false(mask)) return mask;

		if (is_leaf()) {
			SIMD_float hit(0.0f);

			for (int i = first; i < first + count; i++) {
				hit = hit | primitives[indices[i]].intersect(ray, max_distance);

				if (SIMD_float::all_true(hit)) return hit;
			}

			return hit;
		} else {
			if (should_visit_left_first(ray)) {
				SIMD_float hit = nodes[left].intersect(primitives, indices, nodes, ray, max_distance);

				if (SIMD_float::all_true(hit)) return hit;

				return hit | nodes[left + 1].intersect(primitives, indices, nodes, ray, max_distance);
			} else {
				SIMD_float hit = nodes[left + 1].intersect(primitives, indices, nodes, ray, max_distance);

				if (SIMD_float::all_true(hit)) return hit;

				return hit | nodes[left].intersect(primitives, indices, nodes, ray, max_distance);
			}
		}
	}
	
	inline void debug(FILE * file, const SBVHNode nodes[], int & index) const {
		aabb.debug(file, index++);

		if (!is_leaf()) {
			nodes[left  ].debug(file, nodes, index);
			nodes[left+1].debug(file, nodes, index);
		}
	}
};

struct SBVH {
	Triangle * primitives;
	int        primitive_count;

	int * indices_x;
	int * indices_y;
	int * indices_z;

	SBVHNode * nodes;

	inline void init(int count) {
		assert(count > 0);

		primitive_count = count; 
		primitives = new Triangle[primitive_count];

		// Construct index array
		int overallocation = 10;

		int * all_indices  = new int[3 * overallocation * primitive_count];
		indices_x = all_indices;
		indices_y = all_indices + primitive_count * overallocation;
		indices_z = all_indices + primitive_count * overallocation * 2;

		for (int i = 0; i < primitive_count; i++) {
			indices_x[i] = i;
			indices_y[i] = i;
			indices_z[i] = i;
		}

		// Construct Node pool
		nodes = reinterpret_cast<SBVHNode *>(ALLIGNED_MALLOC(8 * primitive_count * sizeof(SBVHNode), 64));
		assert((unsigned long long)nodes % 64 == 0);
	}

	inline void build() {
		ScopedTimer timer("BVH Construction");

		float * sah = new float[primitive_count];
		
		std::sort(indices_x, indices_x + primitive_count, [&](int a, int b) { return primitives[a].get_position().x < primitives[b].get_position().x; });
		std::sort(indices_y, indices_y + primitive_count, [&](int a, int b) { return primitives[a].get_position().y < primitives[b].get_position().y; });
		std::sort(indices_z, indices_z + primitive_count, [&](int a, int b) { return primitives[a].get_position().z < primitives[b].get_position().z; });
		
		int * indices[3] = { indices_x, indices_y, indices_z };

		int * temp[2] = { new int[primitive_count], new int[primitive_count] };

		AABB root_aabb = BVHPartitions::calculate_bounds(primitives, indices[0], 0, primitive_count);

		int node_index = 2;
		int leaf_count = nodes[0].subdivide(primitives, indices, nodes, node_index, 0, primitive_count, sah, temp, 1.0f / root_aabb.surface_area(), root_aabb);

		printf("Leaf count: %i\n", leaf_count);

		assert(node_index <= 8 * primitive_count);

		delete [] temp[0];
		delete [] temp[1];
		delete [] sah;
	}
	
	inline void update() const {
		for (int i = 0; i < primitive_count; i++) {
			primitives[i].update();
		}
	}
	
	inline void trace(const Ray & ray, RayHit & ray_hit) const {
#if SBVH_TRAVERSAL_STRATEGY == SBVH_TRAVERSE_BRUTE_FORCE
		for (int i = 0; i < primitive_count; i++) {
			primitives[i].trace(ray, ray_hit, 0);
		}
#elif SBVH_TRAVERSAL_STRATEGY == SBVH_TRAVERSE_TREE_NAIVE || SBVH_TRAVERSAL_STRATEGY == SBVH_TRAVERSE_TREE_ORDERED
		nodes[0].trace(primitives, indices_x, nodes, ray, ray_hit, 0);
#endif
	}

	inline SIMD_float intersect(const Ray & ray, SIMD_float max_distance) const {
#if SBVH_TRAVERSAL_STRATEGY == SBVH_TRAVERSE_BRUTE_FORCE
		SIMD_float result(0.0f);

		for (int i = 0; i < primitive_count; i++) {
			result = result | primitives[i].intersect(ray, max_distance);

			if (SIMD_float::all_true(result)) break;
		}

		return result;
#elif SBVH_TRAVERSAL_STRATEGY == SBVH_TRAVERSE_TREE_NAIVE || SBVH_TRAVERSAL_STRATEGY == SBVH_TRAVERSE_TREE_ORDERED
		return nodes[0].intersect(primitives, indices_x, nodes, ray, max_distance);
#endif
	}
	
	inline void debug() const {
		FILE * file = nullptr;
		fopen_s(&file, DATA_PATH("debug.obj"), "w");

		if (file == nullptr) abort(); // Error opening file!

		int index = 0;
		nodes[0].debug(file, nodes, index);

		fclose(file);

		printf("Written debug info to debug.obj\n");
	}
};
