#pragma once

#include <atomic>
#include <vector>
#include <random>
#include <unordered_set>
#include <unordered_map>

#include "src/Helper.h"
#include "src/SceneDefines.h"
#include "src/Camera.h"
#include "Transform.h"
#include "GeometryDefines.h"
#include "GeometryUtilities.h"
#include "BottomBVH.h"
#include "TopBVH.h"
#include "PrimtiveInstance.h"

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

			SCENE_INIT = 0xFFFFFFFFu
		};
		
		SceneModule();
		~SceneModule();
		SceneModule(const SceneModule&) = delete;
		SceneModule(const SceneModule&&) = delete;
		SceneModule& operator=(const SceneModule&) = delete;

		friend class GuiModule;

		inline Camera& GetCamera(int index) {
			return camera[index];
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

		uint32_t CreateTransform(const glm::vec3& translate = glm::vec3(0.0f), const glm::vec3& scale = glm::vec3(1.0f), const glm::vec3& rotate = glm::vec3(0.0f), const glm::vec3& pivot = glm::vec3(0.0f));
		
		void DeleteTransform(const uint32_t index);
		
		void UpdateTransform(const uint32_t index);

		uint32_t CreateCube(const std::string& name);

		uint32_t CreateSphere(const std::string& name, const float radius);

		void DeleteGeometry(const uint32_t index);

		void UpdateGeometry(const uint32_t index);

		uint32_t CreatePrimitiveInstance(const uint32_t geometry_index, const uint32_t transform_index, const uint32_t material_index, const int inner_volume_index = -1, const bool treat_as_boundary = false);
		
		void DeletePrimitiveInstance(const uint32_t index);
		
		void UpdatePrimitiveInstance(const uint32_t index);

	private:
		uint32_t scene_change_flag;

		// note: device data block.
		SceneResource scene_resource;

		// note: original host data.
		Camera camera[CAMERA_COUNT];

		std::vector<TransformState> transform_states;
		std::vector<uint32_t> transform_reference_counters;
		std::vector<glm::mat4> transforms;
		std::vector<glm::mat4> i_transforms;

		std::vector<GeometryData> geometries;
		std::vector<uint32_t> geometry_reference_counters;
		std::vector<std::string> geometry_names;

		std::vector<PrimitiveInstance> primitive_instances;

		std::vector<TopNode> top_nodes;
	};
}