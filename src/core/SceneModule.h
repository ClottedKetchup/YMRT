#pragma once

#include <ppl.h>

#include <mutex>
#include <atomic>
#include <vector>
#include <random>
#include <unordered_set>
#include <unordered_map>

#include "src/Helper.h"
#include "src/SceneDefines.h"
#include "src/Camera.h"
#include "src/Transform.h"
#include "src/GeometryDefines.h"
#include "src/GeometryUtilities.h"
#include "src/BottomBVH.h"
#include "src/TopBVH.h"
#include "src/PrimtiveInstance.h"
#include "src/Material.h"
#include "src/Texture.h"
#include "src/Light.h"
#include "src/Volume.h"

#include "src/LowDiscrepancy.h"
#include "src/TextureManager.h"
#include "src/FileIO.h"

namespace YumeRT{
	class SceneModule {
	public:
		enum SCENE_CHANGE_FLAG {
			SCENE_NONE_CHANGE = 0,
			SCENE_CAMERA_CHANGE = 1,

			SCENE_INSTANCE_CREATE = 1 << 1,
			SCENE_INSTANCE_DELETE = 1 << 2,
			SCENE_INSTANCE_GEOMETRY_CHANGE = 1 << 3,
			SCENE_INSTANCE_TRANSFORM_CHANGE = 1 << 4,
			SCENE_INSTANCE_MATERIAL_CHANGE = 1 << 5,
			SCENE_INSTANCE_VOLUME_CHANGE = 1 << 6,

			SCENE_TRANSFORM_CREATE = 1 << 7,
			SCENE_TRANSFORM_DELETE = 1 << 8,
			SCENE_TRANSFORM_CHANGE = 1 << 9,

			SCENE_GEOMETRY_CREATE = 1 << 10,
			SCENE_GEOMETRY_DELETE = 1 << 11,
			SCENE_GEOMETRY_CHANGE = 1 << 12,

			SCENE_MATERIAL_CREATE = 1 << 13,
			SCENE_MATERIAL_DELETE = 1 << 14,
			SCENE_MATERIAL_CHANGE = 1 << 15,

			SCENE_TEXTURE_CREATE = 1 << 16,
			SCENE_TEXTURE_DELETE = 1 << 17,
			SCENE_TEXTURE_CHANGE = 1 << 18,

			SCENE_DISTANT_LIGHT_CREATE = 1 << 19,
			SCENE_DISTANT_LIGHT_DELETE = 1 << 20,
			SCENE_DISTANT_LIGHT_CHANGE = 1 << 21,

			SCENE_SHAPE_LIGHT_CREATE = 1 << 22,
			SCENE_SHAPE_LIGHT_DELETE = 1 << 23,
			SCENE_SHAPE_LIGHT_CHANGE = 1 << 24,

			SCENE_VOLUME_CREATE = 1 << 25,
			SCENE_VOLUME_DELETE = 1 << 26,
			SCENE_VOLUME_CHANGE = 1 << 27,

			SCENE_RENDER_SETTING_CHANGE = 1 << 28,

			SCENE_IMAGE_TILE_CREATE = 1 << 29,
			SCENE_IMAGE_TILE_DELETE = 1 << 30,

			SCENE_INIT = 0xFFFFFFFFu
		};
		
		SceneModule();
		~SceneModule();
		SceneModule(const SceneModule&) = delete;
		SceneModule(const SceneModule&&) = delete;
		SceneModule& operator=(const SceneModule&) = delete;

		// note: for all the update function, their flag should handle by the gui interface.
		friend class GuiModule;

		inline Camera& GetSceneCamera() {
			return camera[EDITOR_CAMERA_INDEX];
		}
		inline bool SceneChanged() const {
			return scene_change_flag != SCENE_NONE_CHANGE;
		}
		inline void AddSceneFlag(SCENE_CHANGE_FLAG flag) {
			scene_change_flag |= (uint32_t)flag;
		}
		inline void ResetSceneFlag(SCENE_CHANGE_FLAG flag) {
			scene_change_flag = (uint32_t)flag;
		}
		inline bool CheckSceneFlag(SCENE_CHANGE_FLAG flag) const {
			return (scene_change_flag & ((uint32_t)flag));
		} 

		void LoadModelFromFile(const std::string& file_name, const glm::vec3& common_translate = glm::vec3(0.0f), const glm::vec3& common_scale = glm::vec3(1.0f), const glm::vec3& common_rotate = glm::vec3(0.0f), const int common_inner_volume_index = -1, const bool common_treat_as_boundary = false);

		uint32_t CreateTransform(const std::string& name, const glm::vec3& translate = glm::vec3(0.0f), const glm::vec3& scale = glm::vec3(1.0f), const glm::vec3& rotate = glm::vec3(0.0f), const int parent_transform_index = EMPTY_UINT32, const glm::vec3& pivot = glm::vec3(0.0f));
		void DeleteTransform(const uint32_t index);
		void UpdateTransform(const uint32_t index);

		uint32_t CreateMesh(const std::string& name, 
			std::vector<Triangle>& mesh_triangles,
			std::vector<uint32_t>& mesh_position_idxs,
			std::vector<float>& mesh_positions,
			std::vector<uint32_t>& mesh_normal_idxs,
			std::vector<float>& mesh_normals,
			std::vector<uint32_t>& mesh_texcoord_idxs,
			std::vector<float>& mesh_texcoords);
		uint32_t CreateCube(const std::string& name);
		uint32_t CreateSphere(const std::string& name, const float radius);
		void DeleteGeometry(const uint32_t index);
		void UpdateGeometry(const uint32_t index);

		uint32_t CreatePrimitiveInstance(const uint32_t geometry_index, const uint32_t transform_index, const uint32_t material_index, const int inner_volume_index = -1, const bool treat_as_boundary = false);
		void DeletePrimitiveInstance(const uint32_t index);
		void UpdatePrimitiveInstance(const uint32_t index);

		uint32_t CreateDefaultMaterial(const std::string& name,
			const glm::vec3& diffuse_albedo = glm::vec3(0.5f), const glm::vec3& specular_albedo = glm::vec3(1.0f),
			const float roughness_x = 0.2f, const float roughness_y = 0.2f,
			const float ior_n = 1.3f,
			const float metalness = 0.0f,
			const float specular_weight = 1.0f, const float transmission_weight = 0.0f,
			const uint32_t ior_priority = 0,

			const uint32_t diffuse_albedo_tex = EMPTY_UINT32,
			const uint32_t alpha_x_tex = EMPTY_UINT32,
			const uint32_t specular_albedo_tex = EMPTY_UINT32,
			const uint32_t alpha_y_tex = EMPTY_UINT32,
			const uint32_t specular_weight_tex = EMPTY_UINT32,
			const uint32_t metalness_tex = EMPTY_UINT32,
			const uint32_t transmission_weight_tex = EMPTY_UINT32,
			const uint32_t normal_mapping_tex = EMPTY_UINT32,
			const uint32_t bump_mapping_tex = EMPTY_UINT32);
		uint32_t CreateLightMaterial(const std::string& name, const glm::vec3& light_color = glm::vec3(1.0f), const float intensity = 1.0f);
		void DeleteMaterial(const uint32_t index);
		void UpdateMaterial(const uint32_t index);

		uint32_t CreateImageTexture(const std::string& name, const std::string& texture_file_name);
		uint32_t CreateConstantTexture(const std::string& name, const float value);
		uint32_t CreateConstantTexture(const std::string& name, const glm::vec3& color);
		uint32_t CreateCheckerBoardTexture(const std::string& name, 
			const uint32_t texture_black_idx = EMPTY_UINT32, const uint32_t texture_white_idx = EMPTY_UINT32,
			const float freqency = 1.0f);
		uint32_t CreateNoiseTexture(const std::string& name, 
			const uint32_t texture_black_idx = EMPTY_UINT32, const uint32_t texture_white_idx = EMPTY_UINT32,
			const float frequency = 1.0f,
			const bool normalized = true);
		uint32_t CreateNoiseTextureFBM(const std::string& name,
			const uint32_t texture_black_idx = EMPTY_UINT32, const uint32_t texture_white_idx = EMPTY_UINT32,
			const float frequency = 16.0f,
			const float lacunarity = 2.0f, const float gain = 0.5f,
			const int layer_count = 4,
			const float amplitude = 1.0f, const float offset = 0.0f);
		uint32_t CreateNoiseTextureTurbulence(const std::string& name,
			const uint32_t texture_black_idx = EMPTY_UINT32, const uint32_t texture_white_idx = EMPTY_UINT32,
			const float frequency = 16.0f, 
			const float lacunarity = 2.0f,  const float gain = 0.5f, 
			const int layer_count = 4, 
			const float amplitude = 1.0f, const float offset = 0.0f);
		uint32_t CreateNoiseTextureMarble(const std::string& name,
			const uint32_t texture_black_idx = EMPTY_UINT32, const uint32_t texture_white_idx = EMPTY_UINT32,
			const float frequency = 16.0f,
			const float lacunarity = 2.0f, const float gain = 0.5f,
			const int layer_count = 4,
			const float variation = 10.0f,
			const bool x_turb = true, const bool y_turb = false, const bool z_turb = false);
		uint32_t CreateNoiseTextureWood(const std::string& name,
			const uint32_t texture_black_idx = EMPTY_UINT32, const uint32_t texture_white_idx = EMPTY_UINT32,
			const float frequency = 2.0f, 
			const float variation = 10.0f);
		uint32_t CreateNoiseTexturePolkaDot(const std::string& name,
			const uint32_t texture_black_idx = EMPTY_UINT32, const uint32_t texture_white_idx = EMPTY_UINT32,
			const float frequency = 8.0f,
			const float radius = 0.35f);
		uint32_t CreateNoiseTextureWave(const std::string& name,
			const uint32_t texture_black_idx = EMPTY_UINT32, const uint32_t texture_white_idx = EMPTY_UINT32,
			const float freq_0 = 0.1f, const float lacunarity_0 = 2.0f,
			const float gain_0 = 0.5f, const int layer_count_0 = 2,
			const float freq_1 = 2.0f, const float lacunarity_1 = 2.0f,
			const float gain_1 = 0.5f, const int layer_count_1 = 4);
		void DeleteTexture(const uint32_t index);
		void UpdateTexture(const uint32_t index);

		uint32_t CreateDistantLight(const std::string &name, const glm::vec3& light_color, const uint32_t transform_index, float intensity = 1.0f, float theta_max = 5.0f);
		void DeleteDistantLight(const uint32_t index);
		void UpdateDistantLight(const uint32_t index);

		void LoadShapeLight();

		uint32_t CreateVolume(const std::string& name, const uint32_t transform_index, const glm::vec3& sigma_t = glm::vec3(0.01f), const glm::vec3& albedo = glm::vec3(1.0f), const float g = 0.0f, const int density_texture_index = -1);
		void DeleteVolume(const uint32_t index);
		void UpdateVolume(const uint32_t index);

	private:
		struct PrimitiveIndexGenerator{
			uint32_t index_counter;
			std::unordered_set<uint32_t> inused_index;

			inline PrimitiveIndexGenerator() : index_counter(0), inused_index() {}
			uint32_t GetNextIndex() {
				while (inused_index.find(index_counter) != inused_index.end()) {
					++index_counter;
				}
				uint32_t result_index = index_counter;
				inused_index.insert(result_index);
				return result_index;
			}
			void ReleaseIndex(uint32_t index) {
				auto iter = inused_index.find(index);
				if (iter != inused_index.end()) {
					inused_index.erase(iter);
				}
			}
		};

		uint32_t scene_change_flag;

		// note: device data block.
		SceneResource scene_resource;

		// renderer parameters.
		RenderSetting render_setting;

		// note: original host data.
		Camera camera[CAMERA_COUNT];

		std::vector<TransformState> transform_states;
		std::vector<glm::mat4> transforms;
		std::vector<glm::mat4> i_transforms;
		std::vector<uint32_t> transform_reference_counters;
		std::vector<std::string> transform_names;

		std::vector<GeometryData> geometries;
		std::vector<uint32_t> geometry_reference_counters;
		std::vector<std::string> geometry_names;

		PrimitiveIndexGenerator primitive_index_generator;
		std::vector<PrimitiveInstance> primitive_instances;
		std::unordered_map<uint32_t, uint32_t> primitive_index_to_index_map;

		std::vector<TopNode> top_nodes;

		std::vector<Material> materials;
		std::vector<uint32_t> material_reference_counters;
		std::vector<std::string> material_names;

		std::vector<Texture> textures;
		std::vector<std::string> texture_names;

		std::vector<DistantLight> distant_lights;
		std::vector<std::string> distant_light_names;

		std::vector<ShapeLight> shape_lights;
		std::vector<float> shape_light_sample_table;

		std::vector<Volume> volumes;
		std::vector<std::string> volume_names;

		std::vector<uint32_t> permute_table;

		ImageTileCache image_tile_cache;
		std::vector<ImageFile> image_texture_files;
		std::vector<ImageTile> image_texture_tiles; // current implementation: tile rgb data will in both host and device when they are loaded. 

		inline Camera& GetCamera(int index) {
			return camera[index];
		}

		void InitHaltonPermuteTable();
		Scene GetHostSceneDataPointer();
	};
}