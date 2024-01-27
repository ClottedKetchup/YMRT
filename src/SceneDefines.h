#pragma once

#include "MathCommon.h"

namespace YumeRT
{
	struct Camera;

	struct GeometryData;

	struct PrimitiveInstance;

	struct Triangle;

	struct BottomNode;

	struct TopNode;

	struct Material;

	struct DistantLight;

	struct ShapeLight;

	struct Volume;
	
	// TODO: turn this into void pointer!
	struct Scene
	{
		Camera *camera = nullptr;

		uint32_t *vidxs = nullptr;
		glm::vec3 *positions = nullptr;

		uint32_t *nidxs = nullptr;
		glm::vec3 *normals = nullptr;

		uint32_t *uvidxs = nullptr;
		glm::vec2 *texcoords = nullptr;

		Triangle *triangles = nullptr;

		uint32_t geometry_count = 0;
		GeometryData *geometries = nullptr;

		uint32_t prim_instance_count = 0;
		PrimitiveInstance *prim_instances = nullptr;

		uint32_t bottom_node_count = 0;
		BottomNode *bottom_nodes = nullptr;

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

		struct {
			uint32_t *halton_permute_table = nullptr;
			uint64_t *sobol_matrices = nullptr;
		}sampler_data;
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
		bool enable_distant_light;
		bool enable_volume_scattering;
		bool enable_env_light;
		bool enable_russian_roulette;

		__device__ __host__ RenderSetting(): ssp(1), ray_depth(2), gamma(2.2f), exposure(1.0f), sampler_type(0),
			max_frame_count(-1), enable_distant_light(false), enable_env_light(true), enable_russian_roulette(false), enable_volume_scattering(true)  {}
	};
};