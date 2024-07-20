#pragma once

#include <atomic>

#include "src/Helper.h"
#include "src/SceneDefines.h"
#include "src/Camera.h"

namespace YumeRT{
	class SceneModule {
	public:
		enum SCENE_CHANGE_FLAG {
			SCENE_NONE_CHANGE = 0,
			SCENE_CAMERA_CHANGE = 1,

			SCENE_INIT = 0xFFFFFFFF
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

	private:
		uint32_t scene_change_flag;

		// note: device data block.
		SceneResource scene_resource;

		// note: original host data.
		Camera camera[CAMERA_COUNT];
	};
}