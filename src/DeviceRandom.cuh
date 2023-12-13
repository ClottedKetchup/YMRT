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

	struct PixelRandom
	{
	public:
		__device__ __host__ PixelRandom() : sample_idx(0) {}
		__device__ __host__ PixelRandom(int frame): sample_idx(0), frame_seed(frame) {}
		__device__ inline float Random(uint32_t px, uint32_t py)
		{
			int idx = atomicAdd(&sample_idx, 1);
			return float(XxHash32(glm::uvec4(px, py, idx, frame_seed))) / float(uint32_t(INVALID_UINT_32));
		}
		__device__ inline void MultiRandom(uint32_t px, uint32_t py, float *rands, int count)
		{
			int idx = atomicAdd(&sample_idx, count);
			for (int i = 0; i < count; ++i)
			{
				rands[i] = float(XxHash32(glm::uvec4(px, py, idx + i, frame_seed))) / float(uint32_t(INVALID_UINT_32));
			}
		}
	private:
		int sample_idx;
		int frame_seed;
	};
};