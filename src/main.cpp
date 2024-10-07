#include <iostream>

#include "src/core/DuskRenderer.h"
	
void TestScene_CornellBox(const std::string& exec_path, std::shared_ptr<YumeRT::SceneModule> scene_module)
{
	const std::string exec_prefix = exec_path.substr(0, exec_path.rfind('\\') + 1);

	auto& scene_manager = *scene_module;
	const uint32_t cube_index = scene_manager.CreateCube("Cube");
	const uint32_t 	sphere_index = scene_manager.CreateSphere("Sphere", 1.0f);
	const uint32_t fog_tex_black = scene_manager.CreateConstantTexture("fog_tex_black", glm::vec3(0.04f));
	const uint32_t fog_tex_white = scene_manager.CreateConstantTexture("fog_tex_white", glm::vec3(0.96f));
	const uint32_t fog_tex = scene_manager.CreateNoiseTextureTurbulence("fog_tex", fog_tex_black, fog_tex_white, 0.5f, 2.0f, 0.5f, 4, 2.5f, 0.0f);

	auto& camera = scene_manager.GetSceneCamera();

	glm::vec3 camera_from = glm::vec3(0.0f, 0.0f, 10.0f);
	glm::vec3 camera_look = glm::vec3(0.0f, 0.0f, 4.0f);
	constexpr float camera_fov = glm::radians(45.0f);

	camera.SetFov(camera_fov);
	camera.SetPosition(camera_from);
	camera.SetDir(camera_from - camera_look);

	const uint32_t default_material = scene_manager.CreateDefaultMaterial("default material", glm::vec3(0.25f), glm::vec3(1.0f), 0.2f, 0.2f, 1.0f, 0.0f, 0.0f, 0.0f, 0);
	const uint32_t world_volume_index = scene_manager.CreateVolume("world_vol", scene_manager.CreateTransform("world_volume_index_transform"), glm::vec3(0.235f), glm::vec3(1.0f));
	const uint32_t bunny_inner_volume_index_0 = scene_manager.CreateVolume("bunny_inner_vol_0", scene_manager.CreateTransform("bunny_inner_volume_index_0_transform"), glm::vec3(0.085f), glm::vec3(1.0f));
	const uint32_t bunny_inner_volume_index_1 = scene_manager.CreateVolume("bunny_inner_vol_1", scene_manager.CreateTransform("bunny_inner_volume_index_1_transform"), glm::vec3(0.085f), glm::vec3(1.0f));

	scene_manager.LoadModelFromFile(exec_prefix + std::string("model\\bunny\\bunny.obj"), scene_manager.CreateTransform("bunny_transform"));

	// test basic texture op
	const uint32_t tex_black_index = scene_manager.CreateConstantTexture("tex_black", glm::vec3(0.15f));
	const uint32_t tex_white_index = scene_manager.CreateConstantTexture("tex_white", glm::vec3(0.42f));
	const uint32_t tex_checkerboard = scene_manager.CreateCheckerBoardTexture("CheckerBoard_0", tex_black_index, tex_white_index, 64.0f);
	const uint32_t bump_tex_black_index = scene_manager.CreateConstantTexture("bump_tex_black", 0.1f);
	const uint32_t bump_tex_white_index = scene_manager.CreateConstantTexture("bump_tex_white", 1.0f);
	const uint32_t bump_tex_checkerboard = scene_manager.CreateCheckerBoardTexture("bump_tex_CheckerBoard", bump_tex_black_index, bump_tex_white_index, 64.0f);

	// bottom
	scene_manager.CreatePrimitiveInstance(cube_index,
		scene_manager.CreateTransform("bottom cube transform", glm::vec3(0.0f, -5.025f, -5.0f), glm::vec3(10.0f, 0.05f, 10.0f)),
		scene_manager.CreateDefaultMaterial("bottom wall material", glm::vec3(0.75f, 0.75f, 0.75f), glm::vec3(1.0f), 0.15, 0.15, 1.45, 0.0f, 1.0f, 0.0f, 0, tex_checkerboard),
		-1);

	const auto group_transform_index = scene_manager.CreateTransform("cube group transform", glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec3(0.0f, 45.0f, 0.0f));

	scene_manager.CreatePrimitiveInstance(cube_index,
		scene_manager.CreateTransform("left cube transform", glm::vec3(-3.0f, -3.0f, -5.0f), glm::vec3(2.15f, 4.0f, 2.15f), glm::vec3(0.0f), group_transform_index),
		scene_manager.CreateDefaultMaterial("left cube material", glm::vec3(0.75f, 0.75f, 0.75f)),
		-1);

	scene_manager.CreatePrimitiveInstance(cube_index,
		scene_manager.CreateTransform("mid cube transform", glm::vec3(0.0f, -3.0f, -5.0f), glm::vec3(2.15f, 4.0f, 2.15f), glm::vec3(0.0f), group_transform_index),
		scene_manager.CreateDefaultMaterial("mid cube material", glm::vec3(0.75f, 0.75f, 0.75f)),
		-1);

	scene_manager.CreatePrimitiveInstance(cube_index,
		scene_manager.CreateTransform("right cube transform", glm::vec3(3.0f, -3.0f, -5.0f), glm::vec3(2.15f, 4.0f, 2.15f), glm::vec3(0.0f), group_transform_index),
		scene_manager.CreateDefaultMaterial("right cube material", glm::vec3(0.75f, 0.75f, 0.75f)),
		-1);

	// sphere light
	scene_manager.CreatePrimitiveInstance(sphere_index,
		scene_manager.CreateTransform("sphere light transform", glm::vec3(3.0f, 3.35f, -5.0f), glm::vec3(0.75f, 0.75f, 0.75f)),
		scene_manager.CreateLightMaterial("sphere light material", glm::vec3(1.0f), 85.0f),
		-1);

	const uint32_t glass_tex_black_index = scene_manager.CreateConstantTexture("glass_tex_black", 0.65f);
	const uint32_t glass_tex_white_index = scene_manager.CreateConstantTexture("glass_tex_white", 0.05f);
	const uint32_t glass_tex_checkerboard = scene_manager.CreateCheckerBoardTexture("glass_CheckerBoard_0", glass_tex_black_index, glass_tex_white_index, 16.0f);

		// ground
	const std::string ground_diffuse_tex_path = exec_prefix + std::string("test_assests\\weathered_wood\\weathered_brown_planks_diff_2k.png");
	const uint32_t ground_diffuse_tex_idx = scene_manager.CreateImageTexture("ground diffuse", ground_diffuse_tex_path);
	const std::string ground_bump_tex_path = exec_prefix + std::string("test_assests\\weathered_wood\\weathered_brown_planks_disp_2k.png");
	const uint32_t ground_bump_tex_idx = scene_manager.CreateImageTexture("ground bump", ground_bump_tex_path);
	const std::string ground_roughness_tex_path = exec_prefix + std::string("test_assests\\weathered_wood\\weathered_brown_planks_rough_2k.png");
	const uint32_t ground_roughness_tex_idx = scene_manager.CreateImageTexture("ground roughness", ground_roughness_tex_path);

	scene_manager.CreatePrimitiveInstance(cube_index,
		scene_manager.CreateTransform("ground transform", glm::vec3(0.0f, -5.55f, 0.0f), glm::vec3(135.0, 1.0f, 135.0f)),
		scene_manager.CreateDefaultMaterial("ground material", glm::vec3(0.55f, 0.55f, 0.55f), glm::vec3(0.0f), 0.2, 0.2, 1.3, 0.0f, 0.0f, 0.0f, 0,
			ground_diffuse_tex_idx, ground_roughness_tex_idx, ground_diffuse_tex_idx, ground_roughness_tex_idx, EMPTY_UINT32, EMPTY_UINT32, EMPTY_UINT32, EMPTY_UINT32, ground_bump_tex_idx),
		-1);

	const uint32_t cube_volume = scene_manager.CreateVolume("cube volume", scene_manager.CreateTransform("cube volume transform"), glm::vec3(0.085f), glm::vec3(1.0f), 0.0f, fog_tex);

	scene_manager.CreatePrimitiveInstance(cube_index,
		scene_manager.CreateTransform("cube volume boundary transform", glm::vec3(0.0f, 9.0f, 0.0f), glm::vec3(15.0f, 2.0f, 15.0f)),
		scene_manager.CreateDefaultMaterial("extra cube 2 material"),
		-1, true);

	scene_manager.CreateDistantLight("default distant light", glm::vec3(1.0f), scene_manager.CreateTransform("transform_distant_light", glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(180.0f, 0.0f, 30.0f)), 1.0f);
}

void TestScene_Sponza(const std::string& exec_path, std::shared_ptr<YumeRT::SceneModule> scene_module) 
{
	const std::string exec_prefix = exec_path.substr(0, exec_path.rfind('\\') + 1);

	auto& scene_manager = *scene_module;
	const uint32_t cube_index = scene_manager.CreateCube("Cube");
	const uint32_t 	sphere_index = scene_manager.CreateSphere("Sphere", 1.0f);
	const uint32_t fog_tex_black = scene_manager.CreateConstantTexture("fog_tex_black", glm::vec3(0.04f));
	const uint32_t fog_tex_white = scene_manager.CreateConstantTexture("fog_tex_white", glm::vec3(0.96f));
	const uint32_t fog_tex = scene_manager.CreateNoiseTextureTurbulence("fog_tex", fog_tex_black, fog_tex_white);

	const uint32_t cube_volume = scene_manager.CreateVolume("cube_volume", scene_manager.CreateTransform("cube_volume_transform"), glm::vec3(0.085f), glm::vec3(1.0f), 0.0f, fog_tex);

	auto& camera = scene_manager.GetSceneCamera();

	glm::vec3 camera_from = glm::vec3(0.0f, 10.0f, 10.0f);
	glm::vec3 camera_look = glm::vec3(0.0f, 10.0f, 4.0f);
	constexpr float camera_fov = glm::radians(45.0f);

	camera.SetFov(camera_fov);
	camera.SetPosition(camera_from);
	camera.SetDir(camera_from - camera_look);

	const uint32_t default_material = scene_manager.CreateDefaultMaterial("default material", glm::vec3(0.25f), glm::vec3(1.0f), 0.2f, 0.2f, 1.0f, 0.0f, 0.0f, 0.0f, 0);
	const uint32_t world_volume_index = scene_manager.CreateVolume("world_vol", scene_manager.CreateTransform("world_volume_index_transform"), glm::vec3(0.235f), glm::vec3(1.0f));

	scene_manager.LoadModelFromFile(exec_prefix + std::string("model\\bunny\\bunny.obj"), scene_manager.CreateTransform("bunny_transform", glm::vec3(0, 20, 0), glm::vec3(15)));

	scene_manager.CreatePrimitiveInstance(cube_index,
		scene_manager.CreateTransform("cube volume boundary transform", glm::vec3(0.0f, 100.0f, 0.0f), glm::vec3(45.0f, 45.0f, 45.0f)),
		scene_manager.CreateDefaultMaterial("extra cube 2 material"),
		-1, false);

	scene_manager.LoadModelFromFile(exec_prefix + std::string("model\\sponza\\sponza.obj"), scene_manager.CreateTransform("sponza_transform"));

	
	scene_manager.CreateDistantLight("default distant light", glm::vec3(1.0f), scene_manager.CreateTransform("transform_distant_light", glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(180.0f, 0.0f, 30.0f)), 1.0f);
}

int main(int argc, char *argv[])
{
	try {
		auto dusk_renderer = YumeRT::DuskRenderer::GetDuskRenderer();

		// Do some default initialization...
		TestScene_CornellBox(std::string(argv[0]), dusk_renderer->GetSceneModule());

		dusk_renderer->Run();
	}
	catch (const std::exception& err) {
		std::cout << err.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}