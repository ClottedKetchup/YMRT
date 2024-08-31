#include <iostream>

#include "src/core/DuskRenderer.h"
	
int main(int argc, char *argv[])
{
	try {
		auto dusk_renderer = YumeRT::DuskRenderer::GetDuskRenderer();

		// Do some default initialization...
		auto& scene_manager = *(dusk_renderer->GetSceneModule().get());

		const auto sphere_geometry_index =  scene_manager.CreateSphere("default sphere", 1.0f);

		const auto sphere_transform_index_0 = scene_manager.CreateTransform("transform_0", glm::vec3(2, 0, -2));
		const auto sphere_transform_index_1 = scene_manager.CreateTransform("transform_1", glm::vec3(-2, 0, -2));
		const auto sphere_transform_index_2 = scene_manager.CreateTransform("transform_2", glm::vec3(0, 0, -2));

		const auto material_texture_0 = scene_manager.CreateCheckerBoardTexture("CheckerBoard_0",
			scene_manager.CreateConstantTexture("Constant_1", glm::vec3(1.0f, 0.05f, 0.05f)),
			scene_manager.CreateConstantTexture("Constant_0", glm::vec3(0.05f)),
			15.0f);

		const auto sphere_material_index_0 = scene_manager.CreateDefaultMaterial("material_0", 
			glm::vec3(0.75, 0.15, 0.15), 
			glm::vec3(1.0f), 
			0.2f, 
			0.2f, 
			1.3f, 
			0.0f, 
			1.0f, 
			0.0f, 
			0, 
			material_texture_0);
		
		
		const auto sphere_material_index_1 = scene_manager.CreateDefaultMaterial("material_1", glm::vec3(0.75, 0.75, 0.75));
		const auto sphere_material_index_2 = scene_manager.CreateDefaultMaterial("material_2", glm::vec3(0.15, 0.75, 0.15));

		scene_manager.CreatePrimitiveInstance(sphere_geometry_index, sphere_transform_index_0, sphere_material_index_0);
		scene_manager.CreatePrimitiveInstance(sphere_geometry_index, sphere_transform_index_1, sphere_material_index_1);
		scene_manager.CreatePrimitiveInstance(sphere_geometry_index, sphere_transform_index_2, sphere_material_index_2);

		scene_manager.CreateDistantLight("default distant light", glm::vec3(1.0f), scene_manager.CreateTransform("transform_distant_light", glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(-180.0f, 0.0f, 30.0f)), 5.0f);

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