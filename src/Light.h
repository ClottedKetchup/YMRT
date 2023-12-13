#pragma once

#include <driver_types.h>

#include "MathCommon.h"

#include "SampleUtilities.h"

namespace YumeRT
{
	struct DistantLight
	{
		glm::vec3 light_u;
		glm::vec3 light_v;
		glm::vec3 light_w;
		glm::vec3 light_color;
		float intensity;
		float cos_theta_max;
		float theta_max;
		float padding;

		// dir always toward the light, wi!
		__device__ __host__ DistantLight() {}
		__device__ __host__ inline float PDF(const glm::vec3 &wi)
		{
			return 0.0f;
		}
		__device__ __host__ inline glm::vec3 EvalLi(const glm::vec3 &wi, float *pdf)
		{
			float cos_theta = glm::dot(-wi, light_w);
			if (cos_theta < cos_theta_max) 
			{
				*pdf = 0.0f;
				return glm::vec3(0.0f);
			}
			float cone_solid_angle = 2.0f * ONE_PI * (1.0f - cos_theta_max);
			*pdf = 1.0f / cone_solid_angle;
			return light_color * intensity * (*pdf);
		}

		// return light's energy
		__device__ __host__ inline glm::vec3 SampleLi(const glm::vec3& position, 
																				 const glm::vec3 &normal, 
																				 float u0, 
																				 float u1,
																				 glm::vec3 *wi, 
																				 glm::vec3 *light_sample_pos, 
																				 float *pdf)
		{
			float cone_solid_angle = 2.0f * ONE_PI * (1.0f - cos_theta_max);
			
			*pdf = 1.0f / cone_solid_angle;
			
			glm::vec3 cone_sample = SampleCone(u0, u1, cos_theta_max);
			
			// should be normalized
			*wi = -(cone_sample.x * light_u + cone_sample.y * light_v + cone_sample.z * light_w);

			return light_color * intensity;
		}

	};
};