#pragma once

#include <driver_types.h>

#include "MathCommon.h"

namespace YumeRT
{
#define	TMAX 1E36f
#define	TMIN  1E-6f
	struct Ray 
	{
		glm::vec3 origin;
		glm::vec3 direction;
		mutable float t;
		uint32_t volume_idx;

		__device__ __host__ Ray() = default;

		__device__ __host__ Ray(const glm::vec3 &origin, const glm::vec3 &direction)
			:origin(origin), direction(glm::normalize(direction)), t(TMAX) {}

		__device__ __host__ inline glm::vec3 PositionAtT(float time)
		{
			return origin + direction * time;
		}
	};

	__device__ __host__ inline Ray TransformRay(const Ray &ray, const glm::mat4 &transform)
	{
		Ray transformed_ray;
		transformed_ray.origin = glm::vec3(transform * glm::vec4(ray.origin, 1.0f));;
		transformed_ray.direction = glm::vec3(transform * glm::vec4(ray.direction, 0.0f));
		transformed_ray.t = ray.t;
		transformed_ray.volume_idx = ray.volume_idx;
		return transformed_ray;
	}
};