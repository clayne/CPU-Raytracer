#pragma once
#include "SIMD_Vector3.h"

struct RayHit {
	SIMD_float hit;
	SIMD_float distance;

	SIMD_Vector3 point;  // Coordinates of the hit in World Space
	SIMD_Vector3 normal; // Normal      of the hit in World Space

	SIMD_int   material_id;
	SIMD_float u, v;

#if RAY_DIFFERENTIALS_ENABLED
	// Derivatives of texture space coordinates s, t
	// with respect to screen space coordinates x, y
	SIMD_float ds_dx, ds_dy;
	SIMD_float dt_dx, dt_dy;

	SIMD_Vector3 dO_dx, dO_dy;
	SIMD_Vector3 dN_dx, dN_dy;
#endif

#if BVH_VISUALIZE_HEATMAP
	int bvh_steps;
#endif

	inline RayHit() {
		hit      = SIMD_float(0.0f);
		distance = SIMD_float(INFINITY);

#if BVH_VISUALIZE_HEATMAP
		bvh_steps = 0;
#endif
	}
};
