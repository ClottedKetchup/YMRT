#pragma once

#include <shared_mutex>

#include "MathCommon.h"

namespace YMRT
{
#define MAX_RAY_DEPTH 32

#define TILE_X_RES 16u
#define TILE_Y_RES 16u
#define TILE_PIXEL_COUNT 256u
#define TILE_PIXEL_RIGHT_SHIFT_BIT 8
#define MAX_RAY_COUNT 1024 * 1024

	struct RenderSetting
	{
		int ssp;
		int ray_depth;
		float gamma;
		float exposure;
		int max_frame_count;
		int sampler_type;
		int enable_shape_light;
		bool enable_distant_light;
		bool enable_volume_scattering;
		bool enable_env_light;
		bool enable_russian_roulette;

		__device__ __host__ inline RenderSetting() : ssp(1), ray_depth(3), gamma(2.2f), exposure(1.0f), sampler_type(0), 
			max_frame_count(-1), enable_distant_light(false), enable_env_light(true), enable_russian_roulette(false), enable_volume_scattering(false), enable_shape_light(false) {}
		__device__ __host__ inline RenderSetting(const RenderSetting&) = default;
		__device__ __host__ inline RenderSetting& operator=(const RenderSetting&) = default;
	};

	struct Ray;

	struct RayTransfer;

	struct HitRecord;

	struct RandomSampler;

	struct Camera;

	struct GeometryData;

	struct PrimitiveInstance;

	struct TopNode;

	struct Material;

	struct DistantLight;

	struct ShapeLight;

	struct Volume;

	struct Texture;
	
	struct ImageTileCache;

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

		RenderSetting *render_setting = nullptr;

		ImageTileCache *image_tile_cache = nullptr;

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
		RayCounter * ray_counter_device;
		RayCounter * ray_counter_host;
	};

	struct ShadingRayData 
	{
		Ray * shading_ray_data_ray;
		glm::vec3 * shading_ray_data_received_light;
		glm::vec3 * shading_ray_data_throughput;
		int * shading_ray_data_pixel_position_x;
		int * shading_ray_data_pixel_position_y;
		int * shading_ray_data_ray_depth;
		int * shading_ray_data_never_scatter;
		int * shading_ray_data_camera_ray;
		int * shading_ray_data_pixel_sample_index;
		RandomSampler * shading_ray_data_sampler;
		RayTransfer * shading_ray_data_ray_transfer;
	};

	struct HitData 
	{
		HitRecord * hit_data_hit_record;
		int * hit_data_nearby_hit_count;
		uint32_t * hit_data_nearby_hit_primitive_index;
		float * hit_data_nearby_t_hit;
		int * hit_data_nearby_hit_back;

		float * hit_data_volume_weights;

		Ray * hit_data_ray;
		glm::vec3 * hit_data_received_light;
		glm::vec3 * hit_data_throughput;
		int * hit_data_pixel_position_x;
		int * hit_data_pixel_position_y;
		int * hit_data_ray_depth;
		int * hit_data_never_scatter;
		int * hit_data_camera_ray;
		int * hit_data_pixel_sample_index;
		RandomSampler * hit_data_sampler;
		RayTransfer * hit_data_ray_transfer;
	};

	struct ShadowRayData 
	{
	
	};

	struct SceneResource{
		Scene scene;
		std::shared_mutex scene_mutex;
		uint64_t scene_change_time;

		SceneResource() :scene{}, scene_change_time(0){}
		SceneResource(const SceneResource&) = delete;
		SceneResource& operator=(const SceneResource&) = delete;
	};

	enum 
	{
		PCG_SAMPLER = 0,
		HALTON_SAMPLER = 1,
		SOBOL_SAMPLER = 2
	};
	
};