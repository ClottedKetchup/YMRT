#pragma once

#include <cuda_runtime.h>
#include <vector_types.h>
#include <driver_types.h>

#include "MathCommon.h"

namespace YumeRT 
{
	__device__ __host__ inline glm::vec2 SampleUnitDisk(float u0, float u1)
	{
		float radius = glm::sqrt(u0);
		float phi = u1 * TWO_PI;
		return glm::vec2(radius * glm::cos(phi), radius * glm::sin(phi));
	}

	__device__ __host__ inline glm::vec3 SampleCosine(float u0, float u1)
	{
		glm::vec2 disk_point = SampleUnitDisk(u0, u1);
		float r2 = disk_point.x * disk_point.x + disk_point.y * disk_point.y;
		return glm::vec3(disk_point.x, disk_point.y, r2 < 1.0f ? glm::sqrt(1.0f - r2) : 0.0f);
	}

	__device__ __host__ inline glm::vec3 SampleCone(float u0, float u1, float cos_theta_max)
	{
		float cos_theta = (1.0f - u0) + u0 * cos_theta_max;
		float sin_theta = glm::sqrt(glm::max(0.0f, 1.0f - cos_theta * cos_theta));
		float phi = u1 * 2.0f * ONE_PI;
		return glm::vec3(glm::cos(phi) * sin_theta, glm::sin(phi) * sin_theta, cos_theta);
	}

	__device__ __host__ inline glm::vec3 SampleTriangle(float u0, float u1)
	{
		// get the barycentric coordinate, the pdf is always one
		float sqrt_u0 = glm::sqrt(glm::max(0.0f, u0));
		float u = 1.0f - sqrt_u0;
		float v = u1 * sqrt_u0;
		return glm::vec3(u, v, 1.0f - u - v);
	}
};
