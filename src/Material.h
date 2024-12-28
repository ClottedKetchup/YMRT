#pragma once
 
#include <driver_types.h>

#include "MathCommon.h"

namespace YumeRT
{
	enum  MATERIAL_TYPE
	{
		DEFAULT_MTL = 0,
		LIGHT_MTL = 1
	};

	struct DefaultMtl 
	{
		glm::vec3 diffuse_albedo = glm::vec3(0.5f);
		uint32_t diffuse_albedo_tex = EMPTY_UINT32;
		glm::vec3 specular_albedo = glm::vec3(1.0f);
		uint32_t specular_albedo_tex = EMPTY_UINT32;

		float alpha_x = 0.2f;
		uint32_t alpha_x_tex = EMPTY_UINT32;
		float	alpha_y = 0.2f;
		uint32_t	alpha_y_tex = EMPTY_UINT32;

		float specular_weight = 1.0f;
		uint32_t specular_weight_tex = EMPTY_UINT32;
		float metalness = 0.0f;
		uint32_t metalness_tex = EMPTY_UINT32;

		float transmission_weight = 0.0f;
		uint32_t transmission_weight_tex = EMPTY_UINT32;
		float	ior_n = 1.33f;
		uint32_t ior_priority = 0;

		glm::vec3 coat_albedo = glm::vec3(1.0f);
		uint32_t coat_albedo_tex = EMPTY_UINT32;

		float coat_weight = 0.0f;
		uint32_t coat_weight_tex = EMPTY_UINT32;
		float coat_thickness = 1.0f;
		uint32_t coat_thickness_tex = EMPTY_UINT32;
		
		float coat_ior = 1.6f;
		uint32_t normal_mapping_tex = EMPTY_UINT32;
		uint32_t bump_mapping_tex = EMPTY_UINT32;
		uint32_t padding;

		__device__ __host__ inline DefaultMtl(const glm::vec3 &diffuse_albedo,
			const glm::vec3 &specular_albedo,
			float roughness_x,
			float roughness_y,
			float ior_n,
			float metalness,
			float specular_weight,
			float transmission_weight,
			uint32_t ior_priority,

			const glm::vec3& coat_albedo,
			float coat_weight,
			float coat_thickness,
			float coat_ior,

			uint32_t diffuse_albedo_tex,
			uint32_t alpha_x_tex,
			uint32_t specular_albedo_tex,
			uint32_t alpha_y_tex,
			uint32_t specular_weight_tex,
			uint32_t metalness_tex,
			uint32_t transmission_weight_tex,
			uint32_t normal_mapping_tex,
			uint32_t bump_mapping_tex,
			
			uint32_t coat_albedo_tex,
			uint32_t coat_weight_tex,
			uint32_t coat_thickness_tex) :

			diffuse_albedo(diffuse_albedo), 
			specular_albedo(specular_albedo), 
			alpha_x(roughness_x), 
			alpha_y(roughness_y), 
			ior_n(ior_n), 
			ior_priority(ior_priority),
			metalness(metalness), 
			specular_weight(specular_weight), 
			transmission_weight(transmission_weight), 

			coat_albedo(coat_albedo),
			coat_weight(coat_weight),
			coat_thickness(coat_thickness),
			coat_ior(coat_ior),

			diffuse_albedo_tex(diffuse_albedo_tex), 
			alpha_x_tex(alpha_x_tex), 
			specular_albedo_tex(specular_albedo_tex), 
			alpha_y_tex(alpha_y_tex), 
			specular_weight_tex(specular_weight_tex), 
			metalness_tex(metalness_tex), 
			transmission_weight_tex(transmission_weight_tex),
			normal_mapping_tex(normal_mapping_tex), 
			bump_mapping_tex(bump_mapping_tex),

			coat_albedo_tex(coat_albedo_tex),
			coat_weight_tex(coat_weight_tex),
			coat_thickness_tex(coat_thickness_tex)
		{
		
		}

		__device__ __host__ inline DefaultMtl& operator=(const DefaultMtl& other)
		{
			diffuse_albedo = other.diffuse_albedo;
			alpha_x = other.alpha_x;
			specular_albedo = other.specular_albedo;
			alpha_y = other.alpha_y;

			specular_weight = other.specular_weight;
			ior_n = other.ior_n;
			metalness = other.metalness;
			transmission_weight = other.transmission_weight;

			diffuse_albedo_tex = other.diffuse_albedo_tex;
			alpha_x_tex = other.alpha_x_tex;
			specular_albedo_tex = other.specular_albedo_tex;
			alpha_y_tex = other.alpha_y_tex;

			specular_weight_tex = other.specular_weight_tex;
			metalness_tex = other.metalness_tex;
			transmission_weight_tex = other.transmission_weight_tex;
			normal_mapping_tex = other.normal_mapping_tex;

			coat_albedo = other.coat_albedo;
			coat_albedo_tex = other.coat_albedo_tex;

			coat_weight = other.coat_weight;
			coat_weight_tex = other.coat_weight_tex;
			coat_thickness = other.coat_thickness;
			coat_thickness_tex = other.coat_thickness_tex;

			coat_ior = other.coat_ior;

			bump_mapping_tex = other.bump_mapping_tex;
			ior_priority = other.ior_priority;
			
			return *this;
		}
	};

	struct LightMtl 
	{
		glm::vec3 light_color = glm::vec3(0.5f);
		float intensity = 0.2f;

		__device__ __host__ inline LightMtl(const glm::vec3 &light_color, float intensity) : light_color(light_color), intensity(intensity) {}
		__device__ __host__ inline LightMtl& operator=(const LightMtl& other) 
		{
			light_color = other.light_color;
			intensity = other.intensity;
			
			return *this;
		}
	};

	struct Material
	{
		uint32_t material_type;
		union 
		{
			DefaultMtl default_mtl;
			LightMtl light_mtl;
		};
		
		__device__ __host__ inline Material() {}
		__device__ __host__ inline Material& operator=(const Material& other) 
		{
			material_type = other.material_type;
			if (material_type == DEFAULT_MTL) 
			{
				default_mtl = other.default_mtl;
			}
			else if (material_type == LIGHT_MTL) 
			{
				light_mtl = other.light_mtl;
			}
			else
			{
			
			}
			return *this;
		}
		__device__ __host__ inline uint32_t GetNormalMappingTex() const
		{
			if (material_type == DEFAULT_MTL) 
			{
				return default_mtl.normal_mapping_tex;
			}
			else 
			{
				return EMPTY_UINT32;
			}
		}
		__device__ __host__ inline uint32_t GetBumpMappingTex() const
		{
			if (material_type == DEFAULT_MTL)
			{
				return default_mtl.bump_mapping_tex;
			}
			else
			{
				return EMPTY_UINT32;
			}
		}
		__device__ __host__ inline float FetchIOR(uint32_t *priority) const 
		{
			if (material_type == DEFAULT_MTL)
			{
				*priority = default_mtl.ior_priority;
				return default_mtl.ior_n;
			}
			else if (material_type == LIGHT_MTL)
			{
				return 1.0f;
			}
			else
			{
				return 1.0f;
			}
		}
		__device__ __host__ inline Material& InitDefaultMtl(const glm::vec3 &diffuse_albedo = glm::vec3(0.5f),
																						  const glm::vec3 &specular_albedo = glm::vec3(1.0f),
																						  float roughness_x = 0.2f,
																						  float roughness_y = 0.2f,
																						  float ior_n = 1.3f,
																						  float metalness = 0.0f,
																						  float specular_weight = 1.0f,
																						  float transmission_weight = 0.0f,
																						  uint32_t ior_priority = 0,

																						  const glm::vec3& coat_albedo = glm::vec3(1.0f),
																						  float coat_weight = 0.0f,
																						  float coat_thickness = 1.0f,
																						  float coat_ior = 1.6f,

																						  uint32_t diffuse_albedo_tex = EMPTY_UINT32,
																						  uint32_t alpha_x_tex = EMPTY_UINT32,
																						  uint32_t specular_albedo_tex = EMPTY_UINT32,
																						  uint32_t alpha_y_tex = EMPTY_UINT32,
																						  uint32_t specular_weight_tex = EMPTY_UINT32,
																						  uint32_t metalness_tex = EMPTY_UINT32,
																						  uint32_t transmission_weight_tex = EMPTY_UINT32, 
																						  uint32_t normal_mapping_tex = EMPTY_UINT32,
																						  uint32_t bump_mapping_tex = EMPTY_UINT32,
			
																						  uint32_t coat_albedo_tex = EMPTY_UINT32,
																						  uint32_t coat_weight_tex = EMPTY_UINT32,
																						  uint32_t coat_thickness_tex = EMPTY_UINT32)
		{
			material_type = DEFAULT_MTL;
			default_mtl = DefaultMtl(diffuse_albedo, 
				specular_albedo, 
				roughness_x, 
				roughness_y, 
				ior_n, 
				metalness, 
				specular_weight, 
				transmission_weight, 
				ior_priority,

				coat_albedo,
				coat_weight,
				coat_thickness,
				coat_ior,

				diffuse_albedo_tex, 
				alpha_x_tex, 
				specular_albedo_tex, 
				alpha_y_tex, 
				specular_weight_tex, 
				metalness_tex, 
				transmission_weight_tex, 
				normal_mapping_tex, 
				bump_mapping_tex,
				
				coat_albedo_tex,
				coat_weight_tex,
				coat_thickness_tex);
			
			return *this;
		}
		__device__ __host__ inline Material& InitLightMtl(const glm::vec3 &light_color = glm::vec3(0.5f), float intensity = 1.0f)
		{
			material_type = LIGHT_MTL;
			light_mtl = LightMtl(light_color, intensity);
			return *this;
		}
	};
};