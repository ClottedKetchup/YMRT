#pragma once
 
#include <driver_types.h>

#include "MathCommon.h"

namespace YumeRT
{
	enum  MATERIAL_TYPE
	{
		SURFACE_MTL = 0,
		LIGHT_MTL = 1
	};

	struct Material
	{
		uint32_t material_type;
		union 
		{
			struct 
			{
				glm::vec3 diffuse_albedo = glm::vec3(0.5f);
				float alpha_x = 0.2f;
				glm::vec3 specular_albedo = glm::vec3(1.0f);
				float	alpha_y = 0.2f;
				float	ior_n = 1.3f;
				float metalness = 0.0f;
				float transmission_weight = 0.0f;
			}surface_material;
			struct
			{
				glm::vec3 light_color = glm::vec3(0.5f);
				float intensity = 0.2f;
			}light_material;
		};
		
		__device__ __host__ Material() {}
	};
};