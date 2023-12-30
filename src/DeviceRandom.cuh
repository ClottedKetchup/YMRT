#pragma once

#include <cuda_runtime.h>
#include <vector_types.h>
#include <driver_types.h>

#include "MathCommon.h"

namespace YumeRT
{
	// https://www.shadertoy.com/view/XlGcRh
	__device__ __host__ inline uint32_t XxHash32(uint32_t p)
	{
		const uint32_t PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
		const uint32_t PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
		uint32_t h32 = p + PRIME32_5;
		h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
		h32 = PRIME32_2 * (h32 ^ (h32 >> 15));
		h32 = PRIME32_3 * (h32 ^ (h32 >> 13));
		return h32 ^ (h32 >> 16);
	}

	__device__ __host__ inline uint32_t XxHash32(const glm::uvec2 &p)
	{
		const uint32_t PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
		const uint32_t PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
		uint32_t h32 = p.y + PRIME32_5 + p.x*PRIME32_3;
		h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
		h32 = PRIME32_2 * (h32 ^ (h32 >> 15));
		h32 = PRIME32_3 * (h32 ^ (h32 >> 13));
		return h32 ^ (h32 >> 16);
	}

	__device__ __host__ inline uint32_t XxHash32(const glm::uvec3 &p)
	{
		const uint32_t PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
		const uint32_t PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
		uint32_t h32 = p.z + PRIME32_5 + p.x*PRIME32_3;
		h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
		h32 += p.y * PRIME32_3;
		h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
		h32 = PRIME32_2 * (h32 ^ (h32 >> 15));
		h32 = PRIME32_3 * (h32 ^ (h32 >> 13));
		return h32 ^ (h32 >> 16);
	}

	__device__ __host__ inline uint32_t XxHash32(const glm::uvec4 &p)
	{
		const uint32_t PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
		const uint32_t PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
		uint32_t h32 = p.w + PRIME32_5 + p.x*PRIME32_3;
		h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
		h32 += p.y * PRIME32_3;
		h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
		h32 += p.z * PRIME32_3;
		h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
		h32 = PRIME32_2 * (h32 ^ (h32 >> 15));
		h32 = PRIME32_3 * (h32 ^ (h32 >> 13));
		return h32 ^ (h32 >> 16);
	}

	// https://www.pcg-random.org/
	__device__ __host__ inline uint32_t pcg(const uint32_t v)
	{
		uint32_t state = v * 747796405u + 2891336453u;
		uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
		return (word >> 22u) ^ word;
	}

	__device__ __host__ inline glm::uvec2 pcg2d(glm::uvec2 v)
	{
		v = v * 1664525u + 1013904223u;

		v.x += v.y * 1664525u;
		v.y += v.x * 1664525u;

		v = v ^ (v >> 16u);

		v.x += v.y * 1664525u;
		v.y += v.x * 1664525u;

		v = v ^ (v >> 16u);

		return v;
	}

	// http://www.jcgt.org/published/0009/03/02/
	__device__ __host__ inline glm::uvec3 pcg3d(glm::uvec3 v)
	{
		v = v * 1664525u + 1013904223u;

		v.x += v.y*v.z;
		v.y += v.z*v.x;
		v.z += v.x*v.y;

		v ^= v >> 16u;

		v.x += v.y*v.z;
		v.y += v.z*v.x;
		v.z += v.x*v.y;

		return v;
	}

	// http://www.jcgt.org/published/0009/03/02/
	__device__ __host__ inline glm::uvec4 pcg4d(glm::uvec4 v)
	{
		v = v * 1664525u + 1013904223u;

		v.x += v.y*v.w;
		v.y += v.z*v.x;
		v.z += v.x*v.y;
		v.w += v.y*v.z;

		v ^= v >> 16u;

		v.x += v.y*v.w;
		v.y += v.z*v.x;
		v.z += v.x*v.y;
		v.w += v.y*v.z;

		return v;
	}

	struct PixelRandom
	{
	public:
		__device__ __host__ PixelRandom() : sample_idx(0) {}
		__device__ __host__ PixelRandom(int frame): sample_idx(0), frame_seed(frame) {}
		__device__ inline float Random1D(uint32_t px, uint32_t py)
		{
			int idx = atomicAdd(&sample_idx, 1);
			// return float(XxHash32(glm::uvec4(px, py, idx, frame_seed))) / float(uint32_t(INVALID_UINT_32));
			return float(pcg4d(glm::uvec4(px, py, idx, frame_seed)).x) / float(uint32_t(INVALID_UINT_32));
		}
		__device__ inline glm::vec2 Random2D(uint32_t px, uint32_t py)
		{
			int idx = atomicAdd(&sample_idx, 1);
			glm::uvec4 pcg_random = pcg4d(glm::uvec4(px, py, idx, frame_seed));
			return glm::vec2(pcg_random.x, pcg_random.y) / float(uint32_t(INVALID_UINT_32));
		}
		__device__ inline glm::vec3 Random3D(uint32_t px, uint32_t py)
		{
			int idx = atomicAdd(&sample_idx, 1);
			glm::uvec4 pcg_random = pcg4d(glm::uvec4(px, py, idx, frame_seed));
			return glm::vec3(pcg_random.x, pcg_random.y, pcg_random.z) / float(uint32_t(INVALID_UINT_32));
		}
		__device__ inline glm::vec4 Random4D(uint32_t px, uint32_t py)
		{
			int idx = atomicAdd(&sample_idx, 1);
			glm::uvec4 pcg_random = pcg4d(glm::uvec4(px, py, idx, frame_seed));
			return glm::vec4(pcg_random) / float(uint32_t(INVALID_UINT_32));
		}
		
	private:
		int sample_idx;
		int frame_seed;
	};
};