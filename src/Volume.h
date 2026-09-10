#pragma once

#include <driver_types.h>

#include "MathCommon.h"

namespace YMRT
{
	struct Volume 
	{
		glm::vec3 sigma_t;
		glm::vec3 albedo;
		int density_texture_idx;
		uint32_t transform_idx;
		float g;

		__device__ __host__ inline Volume() : density_texture_idx(-1), g(0.0f), transform_idx(EMPTY_UINT32), sigma_t(0.0f), albedo(0.0f) {}
	};


	__device__ __host__ inline float EvalPhase(const float g, const glm::vec3 &wo, const glm::vec3 &wi, float *pdf) 
	{
		// HenyeyGreenstein anisotropy scattering
		float cos_theta = glm::dot(wo, wi);
		float t = 1.f + g * g + 2.f * g * cos_theta;
		*pdf = 0.25f * INV_PI * (1.f - g * g) * SafeRcp((t * glm::sqrt(glm::max(t, 0.0f))));
		return (*pdf);
	}
	__device__ __host__ inline bool SamplePhase(const float g, const float u0, const float u1, const glm::vec3 &wo, float *weight, glm::vec3 *wi, float *pdf)
	{
		float cos_theta;
		if (glm::abs(g) < 1e-4f) {
			cos_theta = 1.f - 2.f * u0;
		}
		else {
			float t = (1.f - g * g) * SafeRcp(1.f - g + 2.f * g * u0);
			cos_theta = (1.f + g * g - t * t) / (2.f * g);
		}

		float sin_theta = glm::sqrt(glm::max(0.0f, 1.0f - cos_theta * cos_theta));
		float phi = 2.f * ONE_PI * u1;
		glm::vec3 w = wo, u, v;
		OrthogonalBasis(w, u, v);
		*wi = u * sin_theta * glm::cos(phi) + v * sin_theta * glm::sin(phi) + w * cos_theta;
		float phase_value = EvalPhase(g, wo, *wi, pdf);
		*weight = 1.0f;
		return true;
	}
	
};