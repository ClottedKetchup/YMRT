#pragma once

#include <driver_types.h>

#include "MathCommon.h"

namespace YumeRT
{
#define	TMAX 1E36f
#define	TMIN  1E-7f
	struct Ray 
	{
		//TODO: ior
		glm::vec3 origin;
		glm::vec3 direction;
		mutable float t;
		int volume_idx;
		
		float ray_ior;

		__device__ __host__ inline Ray() : t(TMAX), volume_idx(-1), origin(0.0f), direction(0.0f), ray_ior(1.0f){};

		__device__ __host__ inline Ray(const glm::vec3 &origin, const glm::vec3 &direction, float ray_ior = 1.0f, int volume_idx = -1)
			:origin(origin), direction(glm::normalize(direction)), t(TMAX), ray_ior(ray_ior), volume_idx(volume_idx) {}

		__device__ __host__ inline glm::vec3 PositionAtT(float time)
		{
			return origin + direction * time;
		}
	};

	__device__ __host__ inline Ray TransformRay(const Ray &ray, const glm::mat4 &transform)
	{
		Ray transformed_ray;
		transformed_ray.origin = glm::vec3(transform * glm::vec4(ray.origin, 1.0f));
		transformed_ray.direction = glm::vec3(transform * glm::vec4(ray.direction, 0.0f));
		transformed_ray.t = ray.t;
		transformed_ray.volume_idx = ray.volume_idx;
		transformed_ray.ray_ior = ray.ray_ior;
		return transformed_ray;
	}

	struct RayDifferential 
	{
		glm::vec3 origin_x;
		glm::vec3 direction_x;
		glm::vec3 origin_y;
		glm::vec3 direction_y;

		float scale_x;
		float scale_y;

		__device__ __host__ inline RayDifferential(): scale_x(1.0f), scale_y(1.0f) {}
		__device__ __host__ inline void SetInvalid() 
		{
			auto set_bits = [](float *fp)->void {(*(uint32_t*)(fp)) = EMPTY_UINT32; };
			set_bits(&scale_x);
			set_bits(&scale_y);
		}
		__device__ __host__ bool inline IsValid() const
		{
			auto get_bits = [](const float *fp)->uint32_t {return (*(uint32_t*)(fp)); };
			return get_bits(&scale_x) == EMPTY_UINT32 &&
				get_bits(&scale_y) == EMPTY_UINT32;
		}
	};
};