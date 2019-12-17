#pragma once
#include "Vector2.h"
#include "Vector3.h"

#include "Ray.h"
#include "RayHit.h"

#include "AABB.h"

struct Triangle {
	Vector3 position0;
	Vector3 position1;
	Vector3 position2;

	Vector2 tex_coord0;
	Vector2 tex_coord1;
	Vector2 tex_coord2;

	Vector3 normal0;
	Vector3 normal1;
	Vector3 normal2;

	const Material * material;

	inline void update() { }

	void       trace    (const Ray & ray, RayHit & ray_hit, int bvh_step) const;
	SIMD_float intersect(const Ray & ray, SIMD_float max_distance) const;

	
	inline void debug(FILE * file, int index) const {
		fprintf(file, "o Triangle_%i\n", index);

		fprintf(file, "v %f %f %f\n", position0.x, position0.y, position0.z);
		fprintf(file, "v %f %f %f\n", position1.x, position1.y, position1.z);
		fprintf(file, "v %f %f %f\n", position2.x, position2.y, position2.z);

		fprintf(file, "f %i %i %i\n", 3*index + 1, 3*index + 2, 3*index + 3);
	}


	// BVH Related
	AABB aabb;
	
	inline Vector3 get_position() const {
		return (position0 + position1 + position2) / 3.0f;
	}
};
