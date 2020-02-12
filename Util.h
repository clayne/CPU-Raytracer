#pragma once
#include <immintrin.h>

#define INVALID -1

#define DATA_PATH(file_name) "../Raytracer/Data/" file_name

#define PI          3.14159265359f
#define ONE_OVER_PI 0.31830988618f

#define TWO_PI          6.28318530718f
#define ONE_OVER_TWO_PI 0.15915494309f

#define DEG_TO_RAD(angle) ((angle) * PI / 180.0f)
#define RAD_TO_DEG(angle) ((angle) / PI * 180.0f)

#define KILO_BYTE(value) (value) * 1024
#define MEGA_BYTE(value) (value) * 1024 * 1024
#define GIGA_BYTE(value) (value) * 1024 * 1024 * 1024

#define FORCEINLINE __forceinline

#define ALLIGNED_MALLOC(size, align) _aligned_malloc(size, align)
#define ALLIGNED_FREE(ptr)           _aligned_free(ptr)

namespace Util {
	const char * get_path(const char * file_path);

	template<typename T>
	void swap(T & a, T & b) {
		T temp = a;
		a = b;
		b = temp;
	}

	// Fast float to int conversion
	inline int float_to_int(float x) {
		return _mm_cvtss_si32(_mm_load_ss(&x));
	}
}
