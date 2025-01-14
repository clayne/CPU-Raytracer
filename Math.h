#pragma once
#include "Triangle.h"

#include "Util.h"
#include "SIMD.h"

#include "Vector3.h"

// Various math util functions
namespace Math {
	// Clamps the value between a smallest and largest allowed value
	template<typename T>
	inline FORCEINLINE T clamp(T value, T min, T max) {
		if (value < min) return min;
		if (value > max) return max;

		return value;
	}

	// Interpolate between a,b,c given barycentric coordinates u,v
	template<typename T, typename Real>
	inline FORCEINLINE T barycentric(const T & base, const T & edge1, const T & edge2, Real u, Real v) {
		return base + u * edge1 + v * edge2;
	}

	// Reflects the vector in the normal
	// The sign of the normal is irrelevant, but it should be normalized
	inline FORCEINLINE SIMD_Vector3 reflect(const SIMD_Vector3 & vector, const SIMD_Vector3 & normal) {
		return vector - (SIMD_float(2.0f) * SIMD_Vector3::dot(vector, normal)) * normal;
	}

	// Refracts the vector in the normal, according to Snell's Law
	// The normal should be oriented such that it makes the smallest angle possible with the vector
	inline FORCEINLINE SIMD_Vector3 refract(const SIMD_Vector3 & vector, const SIMD_Vector3 & normal, SIMD_float eta, SIMD_float cos_theta, SIMD_float k) {
		return eta * vector + ((eta * cos_theta) - SIMD_float::sqrt(k)) * normal;
	}
	
	// Checks if n is a power of two
	inline FORCEINLINE constexpr bool is_power_of_two(int n) {
		if (n == 0) return false;

		return (n & (n - 1)) == 0;
	}
	
	// Computes positive modulo of given value
	inline FORCEINLINE unsigned mod(int value, int modulus) {
		int result = value % modulus;
		if (result < 0) {
			result += modulus;
		}

		return result;
	}

	inline float linear_to_gamma(float x) {
		if (x <= 0.0f) {
			return 0.0f;
		} else if (x >= 1.0f) {
			return 1.0f;
		} else if (x < 0.0031308f) {
			return x * 12.92f;
		} else {
			return powf(x, 1.0f / 2.4f) * 1.055f - 0.055f;
		}
	}

	inline float gamma_to_linear(float x) {
		if (x <= 0.0f) {
			return 0.0f;
		} else if (x >= 1.0f) {
			return 1.0f;
		} else if (x < 0.04045f) {
			return x / 12.92f;
		} else {
			return powf((x + 0.055f) / 1.055f, 2.4f);
		}
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
