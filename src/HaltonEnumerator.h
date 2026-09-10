#pragma once

#include <cuda_runtime.h>
#include <vector_types.h>
#include <driver_types.h>

#include "MathCommon.h"

namespace YMRT
{
	struct HaltonEnumerator
	{
		__device__ __host__ inline HaltonEnumerator(int width, int height)
		{
			int scale2 = 1;
			exp2 = 0;
			while (scale2 < width)
			{
				scale2 *= 2;
				++exp2;
			}

			int scale3 = 1;
			exp3 = 0;
			while (scale3 < height)
			{
				scale3 *= 3;
				++exp3;
			}

			int w = scale2, h = scale3;
			MultiplicativeInverses mi = ExtendedEuclid(h, w);
			const int inv2 = mi.x < 0 ? (mi.x + w) : (mi.x % w);
			const int inv3 = mi.y < 0 ? (mi.y + h) : (mi.y % h);

			m_x = h * inv2;
			m_y = w * inv3;
			m_increment = w * h;
			m_x_scale = float(scale2);
			m_y_scale = float(scale3);
		}
		__device__ __host__ inline uint64_t GetIndex(int px, int py, int ith_pixel_sample) const
		{
			const uint64_t ix = Halton2Inverse(px, exp2);
			const uint64_t iy = Halton3Inverse(py, exp3);
			const uint64_t offset = (ix * (uint64_t)m_x + iy * (uint64_t)m_y) % m_increment;
			return offset + (uint64_t)ith_pixel_sample * (uint64_t)m_increment;
		}
		__device__ __host__ inline float ScaleX(float sample_x) const
		{
			return sample_x * m_x_scale;
		}
		__device__ __host__ inline float ScaleY(float sample_y) const
		{
			return sample_y * m_y_scale;
		}
		__device__ __host__ inline uint64_t MaxFrameCount()const
		{
			return uint64_t(0xFFFFFFFFFFFFFFFF) / uint64_t(m_increment);
		}

	private:
		float m_x_scale;
		float m_y_scale;
		int m_x;
		int m_y;
		int exp2;
		int exp3;
		int m_increment;

		struct MultiplicativeInverses
		{
			int x;
			int y;
		};

		__device__ __host__ inline MultiplicativeInverses ExtendedEuclid(const int a, const int b) const
		{
			if (b == 0)
			{
				MultiplicativeInverses mi;
				mi.x = 1;
				mi.y = 0;
				return mi;
			}

			MultiplicativeInverses m_lower = ExtendedEuclid(b, a % b);
			MultiplicativeInverses m_upper;
			m_upper.x = m_lower.y;
			m_upper.y = m_lower.x - (a / b) * m_lower.y;
			return m_upper;
		}
		__device__ __host__ inline uint64_t Halton2Inverse(uint64_t index, uint64_t digit_count) const
		{
			uint64_t i = 0, result = 0;
			while (i < digit_count)
			{
				result = result * 2 + (index % 2);
				index /= 2;
				++i;
			}
			return result;
		}
		__device__ __host__ inline uint64_t Halton3Inverse(uint64_t index, uint64_t digit_count) const
		{
			uint64_t i = 0, result = 0;
			while (i < digit_count)
			{
				result = result * 3 + (index % 3);
				index /= 3;
				++i;
			}
			return result;
		}
	};
};