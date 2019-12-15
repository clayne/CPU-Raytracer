#pragma once
#include "SIMD.h"

#include "Vector3.h"

// Various math util functions
namespace Math {
	// Clamps the value between a smallest and largest allowed value
	template<typename T>
	inline T clamp(T value, T min, T max) {
		if (value < min) return min;
		if (value > max) return max;

		return value;
	}

	// Interpolate between a,b,c given barycentric coordinates u,v
	template<typename T, typename Real>
	inline T barycentric(const T & a, const T & b, const T & c, Real u, Real v) {
        return a + (u * (b - a)) + (v * (c - a));
    }

	enum class PlaneTriangleIntersection {
		INTERSECTS = 0,
		LEFT,
		RIGHT
	};

	inline PlaneTriangleIntersection plane_triangle_intersection(const Vector3 & plane_normal, float plane_distance, const Vector3 & p0, const Vector3 & p1, const Vector3 & p2, Vector3 & i0, Vector3 & i1) {
		// Calculate signed distance to the Plane for each endpoint of the Triangle
		float dist_p0 = Vector3::dot(plane_normal, p0) + plane_distance;
		float dist_p1 = Vector3::dot(plane_normal, p1) + plane_distance;
		float dist_p2 = Vector3::dot(plane_normal, p2) + plane_distance;

		// If all three points lie on the same side of the plane there is no intersection
		if (dist_p0 <= 0.0f && dist_p1 <= 0.0f && dist_p2 <= 0.0f) return PlaneTriangleIntersection::LEFT;
		if (dist_p0 >= 0.0f && dist_p1 >= 0.0f && dist_p2 >= 0.0f) return PlaneTriangleIntersection::RIGHT;

		Vector3 edge10 = p1 - p0;
		Vector3 edge20 = p2 - p0;
		Vector3 edge21 = p2 - p1;

		float t0 = -(Vector3::dot(plane_normal, p0) + plane_distance) / Vector3::dot(plane_normal, edge10);
		float t1 = -(Vector3::dot(plane_normal, p0) + plane_distance) / Vector3::dot(plane_normal, edge20);
		float t2 = -(Vector3::dot(plane_normal, p1) + plane_distance) / Vector3::dot(plane_normal, edge21);

		if (t0 <= 0.0f || t0 >= 1.0f) {
			i0 = p0 + t1 * edge20;
			i1 = p1 + t2 * edge21;
		} else if(t1 <= 0.0f || t1 >= 1.0f) {
			i0 = p0 + t0 * edge10;
			i1 = p1 + t2 * edge21;
		} else { 
			assert(t2 <= 0.0f || t2 >= 1.0f);

			i0 = p0 + t0 * edge10;
			i1 = p0 + t1 * edge20;
		}

		return PlaneTriangleIntersection::INTERSECTS;
	}
	
	// Reflects the vector in the normal
	// The sign of the normal is irrelevant, but it should be normalized
	inline SIMD_Vector3 reflect(const SIMD_Vector3 & vector, const SIMD_Vector3 & normal) {
		return vector - (SIMD_float(2.0f) * SIMD_Vector3::dot(vector, normal)) * normal;
	}

	// Refracts the vector in the normal, according to Snell's Law
	// The normal should be oriented such that it makes the smallest angle possible with the vector
	inline SIMD_Vector3 refract(const SIMD_Vector3 & vector, const SIMD_Vector3 & normal, SIMD_float eta, SIMD_float cos_theta, SIMD_float k) {
		return SIMD_Vector3::normalize(eta * vector + ((eta * cos_theta) - SIMD_float::sqrt(k)) * normal);
	}
	
	// Checks if n is a power of two
	inline constexpr bool is_power_of_two(int n) {
		if (n == 0) return false;

		return (n & (n - 1)) == 0;
	}
	
	// Computes positive modulo of given value
	inline unsigned mod(int value, int modulus) {
		int result = value % modulus;
		if (result < 0) {
			result += modulus;
		}

		return result;
	}

	// Calculates N-th power by repeated squaring. This only works when N is a power of 2
	template<int N> inline float pow2(float value);

	template<>      inline float pow2<0>(float value) { return 1.0f; }
	template<>      inline float pow2<1>(float value) { return value; }
	template<int N> inline float pow2   (float value) { static_assert(is_power_of_two(N)); float sqrt = pow2<N / 2>(value); return sqrt * sqrt; }

	template<int N> inline double pow2(double value);

	template<>      inline double pow2<0>(double value) { return 1.0; }
	template<>      inline double pow2<1>(double value) { return value; }
	template<int N> inline double pow2   (double value) { static_assert(is_power_of_two(N)); double sqrt = pow2<N / 2>(value); return sqrt * sqrt; }

	template<int N> inline SIMD_float pow2(SIMD_float value);

	template<>      inline SIMD_float pow2<0>(SIMD_float value) { return SIMD_float(1.0f); }
	template<>      inline SIMD_float pow2<1>(SIMD_float value) { return value; }
	template<int N> inline SIMD_float pow2   (SIMD_float value) { static_assert(is_power_of_two(N)); SIMD_float sqrt = pow2<N / 2>(value); return sqrt * sqrt; }
}
