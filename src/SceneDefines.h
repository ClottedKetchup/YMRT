#pragma once

#include <shared_mutex>

#include "MathCommon.h"

namespace YumeRT
{
	struct Ray;

	struct Camera;

	struct GeometryData;

	struct PrimitiveInstance;

	struct TopNode;

	struct Material;

	struct DistantLight;

	struct ShapeLight;

	struct Volume;

	struct Texture;
	
	// TODO: turn this into void pointer!
	struct Scene
	{
		uint32_t camera_count = 0;
		Camera *camera = nullptr;

		uint32_t geometry_count = 0;
		GeometryData *geometries = nullptr;

		uint32_t primitive_instance_count = 0;
		PrimitiveInstance *primitive_instances = nullptr;

		uint32_t top_node_count = 0;
		TopNode *top_nodes = nullptr;

		uint32_t transform_count = 0;
		glm::mat4 *transforms = nullptr;
		glm::mat4 *i_transforms = nullptr;

		uint32_t material_count = 0;
		Material *materials = nullptr;

		uint32_t distant_light_count = 0;
		DistantLight *distant_lights = nullptr;

		uint32_t shape_light_count = 0;
		ShapeLight *shape_lights = nullptr;
		float *shape_light_sample_table = nullptr;

		uint32_t volume_count = 0;
		Volume *volumes = nullptr;

		uint32_t texture_count = 0;
		Texture *textures = nullptr;

		struct {
			uint32_t *halton_permute_table = nullptr;
			uint32_t *sobol_matrices = nullptr;
		}sampler_data;
	};

	struct RayCounter {
		uint32_t shading_ray_counter;
		uint32_t hit_counter;
		uint32_t shadow_ray_counter;
	};

	struct RayCounterData
	{
		RayCounter *ray_counter_device;
		RayCounter *ray_counter_host;
	};

	struct ShadingRayData 
	{
		Ray *ray_data_ray;
		glm::vec3 *ray_data_L;
		glm::vec3 *ray_data_throughput;
		int *pixel_position_x;
		int *pixel_position_y;
	};

	struct ShadowRayData 
	{
	
	};

	struct SceneResource{
		Scene scene;
		std::shared_mutex scene_mutex;
		uint64_t scene_change_time;

		SceneResource() :scene{}, scene_change_time(0ull){}
		SceneResource(const SceneResource&) = delete;
		SceneResource& operator=(const SceneResource&) = delete;
	};

	enum 
	{
		PCG_SAMPLER = 0,
		HALTON_SAMPLER = 1,
		SOBOL_SAMPLER = 2
	};
	struct RenderSetting
	{
		int ssp;
		int ray_depth;
		float gamma;
		float exposure;
		int max_frame_count;
		int sampler_type;
		int padding;
		bool enable_distant_light;
		bool enable_volume_scattering;
		bool enable_env_light;
		bool enable_russian_roulette;

		__device__ __host__ inline RenderSetting(): ssp(1), ray_depth(2), gamma(2.2f), exposure(1.0f), sampler_type(0), padding(0),
			max_frame_count(-1), enable_distant_light(false), enable_env_light(true), enable_russian_roulette(false), enable_volume_scattering(false)  {}
		__device__ __host__ inline RenderSetting& operator=(const RenderSetting&) = default;
	};
};