#pragma once
#include "AABB.h"
#include "Math.h"

#include "Debug.h"

namespace BVHConstructors {
	// Calculates the smallest enclosing AABB over the union of all AABB's of the primitives in the range defined by [first, last>
	template<typename PrimitiveType>
	inline AABB calculate_bounds(const PrimitiveType * primitives, const int * indices, int first, int last) {
		AABB aabb = AABB::create_empty();

		// Iterate over relevant Primitives
		for (int i = first; i < last; i++) {
			aabb.expand(primitives[indices[i]].aabb);
		}

		// Make sure the AABB is non-zero along every dimension
		for (int d = 0; d < 3; d++) {
			if (aabb.max[d] - aabb.min[d] < 0.001f) {
				aabb.max[d] += 0.005f;
			}
		}

		return aabb;
	} 

	// Used for debugging
	template<typename PrimitiveType>
	inline bool is_sorted(const PrimitiveType * primitives, int * indices[3], int first, int last) {
		for (int dimension = 0; dimension < 3; dimension++) {
			for (int i = first + 1; i < last; i++) {
				float prev = primitives[indices[dimension][i-1]].get_position()[dimension];
				float curr = primitives[indices[dimension][i  ]].get_position()[dimension];

				if (prev > curr) return false;
			}
		}

		return true;
	}

	// Reorders indices arrays such that indices on the left side of the splitting dimension end up on the left partition in the other dimensions as well
	template<typename PrimitiveType>
	inline void split_indices(const PrimitiveType * primitives, int * indices[3], int first_index, int index_count, int * temp, int split_dimension, int split_index, float split) {
		for (int dimension = 0; dimension < 3; dimension++) {
			if (dimension != split_dimension) {
				int left  = first_index;
				int right = split_index;

				for (int i = first_index; i < first_index + index_count; i++) {
					bool goes_left = primitives[indices[dimension][i]].get_position()[split_dimension] < split;

					if (primitives[indices[dimension][i]].get_position()[split_dimension] == split) {
						// In case the current primitive has the same coordianate as the one we split on along the split dimension,
						// We don't know whether the primitive should go left or right.
						// In this case check all primitive indices on the left side of the split that 
						// have the same split coordinate for equality with the current primitive index i

						int j = split_index - 1;
						// While we can go left and the left primitive has the same coordinate along the split dimension as the split itself
						while (j >= 0 && primitives[indices[split_dimension][j]].get_position()[split_dimension] == split) {
							if (indices[split_dimension][j] == indices[dimension][i]) {
								goes_left = true;

								break;
							}

							j--;
						}
					}

					if (goes_left) {					
						temp[left++] = indices[dimension][i];
					} else {
						temp[right++] = indices[dimension][i];
					}
				}

				// If these conditions are not met the memcpy below is invalid
				assert(left  == split_index);
				assert(right == first_index + index_count);

				memcpy(indices[dimension] + first_index, temp + first_index, index_count * sizeof(int));

				assert(is_sorted(primitives, indices, first_index,        left ));
				assert(is_sorted(primitives, indices, first_index + left, right));
			}
		}
	}

	// Partitions object using the median Primitive along the longest axis
	template<typename PrimitiveType>
	inline int partition_median(const PrimitiveType * primitives, int * indices[3], int first_index, int index_count, int * temp, int & split_dimension) {
		float max_axis_length    = -INFINITY;
		int   max_axis_dimension = -1;

		// Find longest dimension
		for (int dimension = 0; dimension < 3; dimension++) {
			assert(is_sorted(primitives, indices, first_index, first_index + index_count));

			float min = primitives[indices[dimension][first_index                  ]].get_position()[dimension];
			float max = primitives[indices[dimension][first_index + index_count - 1]].get_position()[dimension];

			float axis_length = max - min;
			if (axis_length > max_axis_length) {
				max_axis_length = axis_length;
				max_axis_dimension = dimension;
			}
		}

		split_dimension = max_axis_dimension;

		int median_index = first_index + (index_count >> 1);
		return median_index;
	}

	// Evaluates SAH for every object for every dimension to determine splitting candidate
	template<typename PrimitiveType>
	inline int partition_full_sah(const PrimitiveType * primitives, int * indices[3], int first_index, int index_count, float * sah, int * temp, int & split_dimension, float & split_cost) {
		float min_split_cost = INFINITY;
		int   min_split_index     = -1;
		int   min_split_dimension = -1;

		// Check splits along all 3 dimensions
		for (int dimension = 0; dimension < 3; dimension++) {
			assert(is_sorted(primitives, indices, first_index, index_count));

			// First traverse left to right along the current dimension to evaluate first half of the SAH
			AABB aabb_left = AABB::create_empty();

			for (int i = 0; i < index_count - 1; i++) {
				aabb_left.expand(primitives[indices[dimension][first_index + i]].aabb);
				
				sah[i] = aabb_left.surface_area() * float(i + 1);
			}

			// Then traverse right to left along the current dimension to evaluate second half of the SAH
			AABB aabb_right = AABB::create_empty();

			for (int i = index_count - 1; i > 0; i--) {
				aabb_right.expand(primitives[indices[dimension][first_index + i]].aabb);

				sah[i - 1] += aabb_right.surface_area() * float(index_count - i);
			}

			// Find the minimum of the SAH
			for (int i = 0; i < index_count - 1; i++) {
				float cost = sah[i];
				if (cost < min_split_cost) {
					min_split_cost = cost;
					min_split_index = first_index + i + 1;
					min_split_dimension = dimension;
				}
			}
		}
		
		split_dimension = min_split_dimension;
		split_cost      = min_split_cost;

		return min_split_index;
	}

	inline int partition_spatial(const Triangle * triangles, int * indices[3], int first_index, int index_count, float * sah, int * temp, int & split_dimension, float & split_cost, float & plane_distance) {
		float min_bin_cost = INFINITY;
		int   min_bin_index     = -1;
		int   min_bin_dimension = -1;
		float min_bin_plane_distance;

		const int BIN_COUNT = 100;

		// @SPEED: get this from caller
		AABB bounds = calculate_bounds(triangles, indices[0], first_index, first_index + index_count);

		Vector3 plane_normals[3] = {
			Vector3(1.0f, 0.0f, 0.0f),
			Vector3(0.0f, 1.0f, 0.0f),
			Vector3(0.0f, 0.0f, 1.0f),
		};

		struct Bin {
			AABB aabb;
			int entry;
			int exit;
		} bins[3][BIN_COUNT]; // @TODO: should this be on the stack?

		for (int dimension = 0; dimension < 3; dimension++) {
			float bounds_min  = bounds.min[dimension];
			float bounds_max  = bounds.max[dimension];
			float bounds_step = (bounds_max - bounds_min) / BIN_COUNT;

			Vector3 plane_normal = plane_normals[dimension];

			float plane_left_distance  = -bounds_min;
			float plane_right_distance = plane_left_distance - bounds_step;

			for (int b = 0; b < BIN_COUNT; b++) {
				Bin & bin = bins[dimension][b];
				bin.aabb = AABB::create_empty();
				bin.entry = 0;
				bin.exit  = 0;

				for (int i = first_index; i < first_index + index_count; i++) {
					const Triangle & triangle = triangles[indices[dimension][i]];

					Vector3 intersections[4];
					Math::PlaneTriangleIntersection left  = Math::plane_triangle_intersection(plane_normal, plane_left_distance,  triangle.position0, triangle.position1, triangle.position2, intersections[0], intersections[1]);
					Math::PlaneTriangleIntersection right = Math::plane_triangle_intersection(plane_normal, plane_right_distance, triangle.position0, triangle.position1, triangle.position2, intersections[2], intersections[3]);

					if (left == Math::PlaneTriangleIntersection::INTERSECTS && right == Math::PlaneTriangleIntersection::INTERSECTS) {
						bin.aabb.expand(AABB::from_points(intersections, 4));
					} else if (left == Math::PlaneTriangleIntersection::INTERSECTS) {
						assert(right == Math::PlaneTriangleIntersection::LEFT);

						bin.aabb.expand(AABB::from_points(intersections, 2));

						float dist_p0 = Vector3::dot(plane_normal, triangle.position0) + plane_left_distance;
						float dist_p1 = Vector3::dot(plane_normal, triangle.position1) + plane_left_distance;
						float dist_p2 = Vector3::dot(plane_normal, triangle.position2) + plane_left_distance;

						if (dist_p0 >= 0.0f) bin.aabb.expand(triangle.position0);
						if (dist_p1 >= 0.0f) bin.aabb.expand(triangle.position1);
						if (dist_p2 >= 0.0f) bin.aabb.expand(triangle.position2);

						bin.exit++;
					} else if (right == Math::PlaneTriangleIntersection::INTERSECTS) {
						assert(left == Math::PlaneTriangleIntersection::RIGHT);

						bin.aabb.expand(AABB::from_points(intersections + 2, 2));

						float dist_p0 = Vector3::dot(plane_normal, triangle.position0) + plane_right_distance;
						float dist_p1 = Vector3::dot(plane_normal, triangle.position1) + plane_right_distance;
						float dist_p2 = Vector3::dot(plane_normal, triangle.position2) + plane_right_distance;

						if (dist_p0 <= 0.0f) bin.aabb.expand(triangle.position0);
						if (dist_p1 <= 0.0f) bin.aabb.expand(triangle.position1);
						if (dist_p2 <= 0.0f) bin.aabb.expand(triangle.position2);

						bin.entry++;
					} else if (left == Math::PlaneTriangleIntersection::RIGHT && right == Math::PlaneTriangleIntersection::LEFT){
						bin.aabb.expand(triangle.aabb);

						bin.entry++;
						bin.exit++;
					}
				}

				assert(bin.aabb.min[dimension] >= bounds_min +  b    * bounds_step - 0.001f);
				assert(bin.aabb.max[dimension] <= bounds_min + (b+1) * bounds_step + 0.001f);

				// Advance planes for next Bin
				plane_left_distance  -= bounds_step;
				plane_right_distance -= bounds_step;
			}

			float bin_sah[BIN_COUNT];

			AABB left_aabb  = AABB::create_empty();
			int  left_count = 0;
			
			for (int b = 0; b < BIN_COUNT - 1; b++) {
				left_aabb.expand(bins[dimension][b].aabb);
				left_count += bins[dimension][b].entry;

				bin_sah[b] = left_aabb.surface_area() * float(left_count);

				// if (dimension == 2 && b == 68) left_aabb.debug(0);
			}

			AABB right_aabb  = AABB::create_empty();
			int  right_count = 0;

			for (int b = BIN_COUNT - 1; b > 0; b--) {
				right_aabb.expand(bins[dimension][b].aabb);
				right_count += bins[dimension][b].exit;

				bin_sah[b - 1] += right_aabb.surface_area() * float(right_count);
				
				// if (dimension == 2 && b == 69) right_aabb.debug(1);
			}

			// Find the minimum of the SAH
			for (int b = 0; b < BIN_COUNT - 1; b++) {
				float cost = bin_sah[b];
				if (cost < min_bin_cost) {
					min_bin_cost = cost;
					min_bin_index = first_index + b + 1;
					min_bin_dimension = dimension;

					min_bin_plane_distance = -(bounds_min + bounds_step * float(b+1));
				}
			}
		}
		
		split_dimension = min_bin_dimension;
		split_cost      = min_bin_cost;
		plane_distance  = min_bin_plane_distance;

		return -1;
	}
}
