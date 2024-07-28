#include <iostream>

#include "src/core/DuskRenderer.h"
	
int main(int argc, char *argv[])
{
	try {
		auto dusk_renderer = YumeRT::DuskRenderer::GetDuskRenderer();

		// Do some default initialization...
		auto& scene_manager = *(dusk_renderer->GetSceneModule().get());

		const auto sphere_geometry_index =  scene_manager.CreateSphere("default sphere", 1.0f);
		const auto sphere_transform_index_0 = scene_manager.CreateTransform(glm::vec3(2, 0, -2));
		const auto sphere_transform_index_1 = scene_manager.CreateTransform(glm::vec3(-2, 0, -2));
		const auto sphere_transform_index_2 = scene_manager.CreateTransform(glm::vec3(0, 0, -2));

		scene_manager.CreatePrimitiveInstance(sphere_geometry_index, sphere_transform_index_0, 0);
		scene_manager.CreatePrimitiveInstance(sphere_geometry_index, sphere_transform_index_1, 0);
		scene_manager.CreatePrimitiveInstance(sphere_geometry_index, sphere_transform_index_2, 0);

		auto& camera = scene_manager.GetCamera(EDITOR_CAMERA_INDEX);
		camera.SetFov(glm::radians(45.0f));
		camera.SetPosition(glm::vec3(0.0f, 0.0f, 10.0f));
		camera.SetDir(glm::vec3(0.0f, 0.0f, 10.0f) - glm::vec3(0.0f, 0.0f, 4.0f));

		dusk_renderer->Run();
	}
	catch (const std::exception& err) {
		std::cout << err.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}