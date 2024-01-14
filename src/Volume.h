#pragma once

#include <driver_types.h>

#include "MathCommon.h"

namespace YumeRT
{
	struct Volume 
	{
		glm::vec3 sigma_s;
		glm::vec3 sigma_a;

		__device__ __host__ Volume() {}
		__device__ __host__ float EvalPhase(const glm::vec3 &wo, const glm::vec3 &wi, float *pdf) const
		{
			*pdf = 0.25f * INV_PI;
			return (*pdf);
		}
		__device__ __host__ bool SamplePhase(float u0, float u1, const glm::vec3 &wo, float *weight, glm::vec3 *wi, float *pdf) const
		{
			*pdf = 0.25f * INV_PI;
			*weight = 1.0f;
			*wi = SampleUnitSphere(u0, u1);
			return true;
		}
	};

};