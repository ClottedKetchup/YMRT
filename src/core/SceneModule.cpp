#include "src/core/SceneModule.h"

namespace YumeRT {
	SceneModule::SceneModule() :scene_resource{}, camera(), scene_change_flag(SCENE_INIT)
	{

	}

	SceneModule::~SceneModule()
	{
		auto &scene = scene_resource.scene;

		FREE_GPU_RESOURCE(scene.camera);
		scene.camera_count = 0;
		scene.camera = nullptr;

		ResetSceneFlag(SCENE_INIT);
	}
};