#pragma once

#include <driver_types.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace YumeRT 
{
#define ONE_PI 3.14159265358f
#define TWO_PI 6.28318530716f
#define HALF_PI 1.57079632679f
#define INV_PI 0.318309886183f

#define WORLD_UP glm::vec3(0.0f, 1.0f, 0.0f)

#define YumeRT_FLOAT_MAX  1E36f
#define YumeRT_FLOAT_MIN  -1E36f

#define INVALID_UINT_32 0xFFFFFFFF
#define INVALID_UINT_16 0xFFFF

#define Matrix_T(t) (glm::translate(glm::mat4(1.0f), t))
#define Matrix_S(s) (glm::scale(glm::mat4(1.0f), s))
#define Matrix_R(axis, angle) (glm::rotate(glm::mat4(1.0f), angle, axis))

#define Round_Block_Count(Total_Thread_Count, Block_Thread_Count) (((Block_Thread_Count) + (Total_Thread_Count) - 1) / (Block_Thread_Count))

	template<typename T>
	__device__ __host__ inline void Swap(T &a, T &b)
	{
		T temp = a;
		a = b;
		b = temp;
	}

	template<typename T>
	__device__ __host__ inline T Sqr(const T &x)
	{
		return x * x;
	}

	__device__ __host__ inline int MaxDimension(const glm::vec3 &v)
	{
		int max_d = 0;
		if (v[1] > v[max_d]) { max_d = 1; }
		if (v[2] > v[max_d]) { max_d = 2; }
		return max_d;
	}

	__device__ __host__ inline glm::vec3 Permute(const glm::vec3 &p, int x, int y, int z)
	{
		return glm::vec3(p[x], p[y], p[z]);
	}

	__device__ __host__ inline uint32_t FloatToUint(float f)
	{
		return *((uint32_t*)(&f));
	}

	__device__ __host__ inline float UintToFloat(uint32_t i)
	{
		return *((float*)(&i));
	}

	__device__ __host__ inline int FloatToInt(float f)
	{
		return *((int*)(&f));
	}

	__device__ __host__ inline float IntToFloat(int i)
	{
		return *((float*)(&i));
	}

	__device__ __host__ inline float SafeRcp(float f)
	{
		return glm::abs(f) < 1E-30f ? (f > 0.0f ? 1E30f : -1E30f) : (1.0f / f);
	}

	__device__ __host__ inline glm::vec3 SafeRcp(const glm::vec3 &vec)
	{
		return glm::vec3(SafeRcp(vec.x), SafeRcp(vec.y), SafeRcp(vec.z));
	}

	 __device__ __host__ inline void OrthogonalBasis(glm::vec3 &N, glm::vec3 &T, glm::vec3 &B)
	{
		N = glm::normalize(N);
		if (glm::abs(N.z) > glm::abs(N.x))
		{
			float l = glm::sqrt(glm::max(1.f - N.x * N.x, 0.0f));
			float r = 1.f / l;
			B = glm::vec3(0.f, N.z * r, -N.y * r);
		}
		else
		{
			float l = glm::sqrt(glm::max(1.f - N.z * N.z, 0.0f));
			float r = 1.f / l;
			B = glm::vec3(N.y * r, -N.x * r, 0.f);
		}
		T = glm::cross(B, N);
	}

	 __device__ __host__ inline size_t AlignedMemorySize(size_t memory_required, size_t alignment)
	 {
		 return ((memory_required + (alignment - 1)) & (~(alignment - 1)));
	 }

	 __device__ __host__ inline float SRGBToLinear(float s)
	 {
		 return s > 0.04045 ? glm::pow((s + 0.055) / 1.055, 2.4f) : (s / 12.92f);
	 }

	 __device__ __host__ inline float LinearToSRGB(float l)
	 {
		 return l > 0.0031308f ? (1.055f * glm::pow(l, 1.0f / 2.4f) - 0.055f) : l * 12.92f;
	 }
};
