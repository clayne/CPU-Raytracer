#pragma once
#include "SIMD.h"
#include "Material.h"

struct RayHit {
	SIMD_float hit;
	SIMD_float distance;

	SIMD_Vector3 point;  // Coordinates of the hit in World Space
	SIMD_Vector3 normal; // Normal      of the hit in World Space

	const Material * material[SIMD_LANE_SIZE] = { nullptr };
	SIMD_float u, v;

	inline RayHit() {
		hit      = SIMD_float(0.0f);
		distance = SIMD_float(INFINITY);
	}
};
