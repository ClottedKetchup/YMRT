#include <iostream>
#include <vector>
#include <random>
#include <string>

#include "MainSystem.h"
#include "MathCommon.h"
#include "FileIO.h"

void TestScene_CornellBox(const std::string &exec_path, std::shared_ptr<YumeRT::SceneManager> scene_manager, int screen_width, int screen_height)
{
	/*YumeRT::GeometryData test_geo;
	test_geo.InitCube();
	YumeRT::Triangle *triangles = test_geo.triangle_mesh.GetTrianglesHost();
	uint32_t *position_indices = test_geo.triangle_mesh.GetPositionIndicesHost();
	glm::vec3 *positions = test_geo.triangle_mesh.GetPositionsHost();
	uint32_t *normal_indices = test_geo.triangle_mesh.GetNormalIndicesHost();
	glm::vec3 *normals = test_geo.triangle_mesh.GetNormalsHost();
	uint32_t *texcoord_indices = test_geo.triangle_mesh.GetTexcoordIndicesHost();
	glm::vec2 *texcoords = test_geo.triangle_mesh.GetTexcoordsHost();
	YumeRT::BottomNode *bottom_nodes = test_geo.triangle_mesh.GetNodesHost();
	test_geo.Destory();*/

	const std::string exec_prefix = exec_path.substr(0, exec_path.rfind('\\') + 1);

	uint32_t cube_idx = scene_manager->AddGeometry("Cube", YumeRT::GeometryData().InitCube()), 
		sphere_idx = scene_manager->AddGeometry("Sphere", YumeRT::GeometryData().InitSphere(1.0f));
	const uint32_t fog_tex_black = scene_manager->AddTexture("fog_tex_black", YumeRT::Texture().InitConstantTextureRGB(glm::vec3(0.04f)));
	const uint32_t fog_tex_white = scene_manager->AddTexture("fog_tex_white", YumeRT::Texture().InitConstantTextureRGB(glm::vec3(0.96f)));
	const uint32_t fog_tex = scene_manager->AddTexture("fog_tex", YumeRT::Texture().InitNoiseTextureTurbulence(fog_tex_black, fog_tex_white));
	int world_volume_idx = -1;

	glm::vec3 camera_from = glm::vec3(0.0f, 0.0f, 10.0f);
	glm::vec3 camera_look = glm::vec3(0.0f, 0.0f, 4.0f);
	float camera_fov = glm::radians(45.0f);
	float camera_aspect = float(screen_width) / float(screen_height);

	scene_manager->GetCameraRef().SetFov(camera_fov);
	scene_manager->GetCameraRef().SetAspectRatio(camera_aspect);
	scene_manager->GetCameraRef().SetPosition(camera_from);
	scene_manager->GetCameraRef().SetDir(camera_from - camera_look);
	scene_manager->GetCameraRef().SetIOR(1.0f);
	scene_manager->GetCameraRef().SetVolumeIndex(world_volume_idx);

	const uint32_t default_mtl = scene_manager->AddSurfaceMaterial("default material", glm::vec3(0.25f));
	const uint32_t bunny_inner_vol_idx_0 = scene_manager->AddVolume("bunny_inner_vol_0", scene_manager->AddTransform(), glm::vec3(0.085f), glm::vec3(0.001f));
	const uint32_t bunny_inner_vol_idx_1 = scene_manager->AddVolume("bunny_inner_vol_1", scene_manager->AddTransform(), glm::vec3(0.085f), glm::vec3(0.001f));

	//// fog bound
	//scene_manager->AddPrimInstance(cube_idx,
	//	scene_manager->AddTransform(glm::vec3(0.0f, 9.0f, -5.0f), glm::vec3(150.0f)),
	//	default_mtl,
	//	1.0f,
	//	-1, world_volume_idx, true);

	// left
	scene_manager->AddPrimInstance(cube_idx,
		scene_manager->AddTransform(glm::vec3(-5.025f, 0.0f, -5.0f), glm::vec3(0.05f, 10.0f, 10.0f)),
		scene_manager->AddSurfaceMaterial("left wall", glm::vec3(0.65f, 0.05f, 0.05f)),
		1.0f,
		world_volume_idx, -1);

	// right
	scene_manager->AddPrimInstance(cube_idx,
		scene_manager->AddTransform(glm::vec3(5.025f, 0.0f, -5.0f), glm::vec3(0.05f, 10.0f, 10.0f)),
		scene_manager->AddSurfaceMaterial("right wall", glm::vec3(0.15f, 0.55f, 0.15f)),
		1.0f,
		world_volume_idx, -1);

	// test basic texture op
	const uint32_t tex_black_idx = scene_manager->AddTexture("tex_black", YumeRT::Texture().InitConstantTextureRGB(glm::vec3(0.01f)));
	const uint32_t tex_white_idx = scene_manager->AddTexture("tex_white", YumeRT::Texture().InitConstantTextureRGB(glm::vec3(0.99f)));
	const uint32_t tex_checkerboard = scene_manager->AddTexture("CheckerBoard_0", YumeRT::Texture().InitCheckerBoardTexture(tex_black_idx, tex_white_idx, 64.0f));
	const uint32_t bump_tex_black_idx = scene_manager->AddTexture("bump_tex_black", YumeRT::Texture().InitConstantTextureFloat(0.1f));
	const uint32_t bump_tex_white_idx = scene_manager->AddTexture("bump_tex_white", YumeRT::Texture().InitConstantTextureFloat(1.0f));
	const uint32_t bump_tex_checkerboard = scene_manager->AddTexture("bump_tex_CheckerBoard", YumeRT::Texture().InitCheckerBoardTexture(bump_tex_black_idx, bump_tex_white_idx, 64.0f));

	// bottom
	scene_manager->AddPrimInstance(cube_idx,
		scene_manager->AddTransform(glm::vec3(0.0f, -5.025f, -5.0f), glm::vec3(10.0f, 0.05f, 10.0f)),
		scene_manager->AddSurfaceMaterial("bottom wall", glm::vec3(0.75f, 0.75f, 0.75f), glm::vec3(1.0f), 0.15, 0.15, 1.45, 0.0f, 1.0f, 0.0f, 0, tex_checkerboard),
		1.0f,
		world_volume_idx, -1);

	// top
	scene_manager->AddPrimInstance(cube_idx,
		scene_manager->AddTransform(glm::vec3(0.0f, 5.025f, -5.0f), glm::vec3(10.0f, 0.05f, 10.0f)),
		scene_manager->AddSurfaceMaterial("top wall", glm::vec3(0.75f, 0.75f, 0.75f)),
		1.0f,
		world_volume_idx, -1);

	// back
	scene_manager->AddPrimInstance(cube_idx,
		scene_manager->AddTransform(glm::vec3(0.0f, 0.0f, -10.025f), glm::vec3(10.0f, 10.0f, 0.05f)),
		scene_manager->AddSurfaceMaterial("back wall", glm::vec3(0.75f, 0.75f, 0.75f)),
		1.0f,
		world_volume_idx, -1);

	scene_manager->AddPrimInstance(cube_idx,
		scene_manager->AddTransform(glm::vec3(-3.0f, -3.0f, -5.0f), glm::vec3(2.15f, 4.0f, 2.15f)),
		scene_manager->AddSurfaceMaterial("cube left", glm::vec3(0.75f, 0.75f, 0.75f)),
		1.0f,
		world_volume_idx, -1);

	scene_manager->AddPrimInstance(cube_idx,
		scene_manager->AddTransform(glm::vec3(0.0f, -3.0f, -5.0f), glm::vec3(2.15f, 4.0f, 2.15f)),
		scene_manager->AddSurfaceMaterial("cube mid", glm::vec3(0.75f, 0.75f, 0.75f)),
		1.0f,
		world_volume_idx, -1);

	scene_manager->AddPrimInstance(cube_idx,
		scene_manager->AddTransform(glm::vec3(3.0f, -3.0f, -5.0f), glm::vec3(2.15f, 4.0f, 2.15f)),
		scene_manager->AddSurfaceMaterial("cube right", glm::vec3(0.75f, 0.75f, 0.75f)),
		1.0f,
		world_volume_idx, -1);

	//// light
	//scene_manager->AddPrimInstance(cube_idx,
	//	scene_manager->AddTransform(glm::vec3(-3.0f, 4.15f, -5.0f), glm::vec3(0.3, 0.3f, 0.3)),
	//	scene_manager->AddLightMaterial("ceil light", glm::vec3(1.0f), 150.0f),
	//	1.0f,
	//	world_volume_idx, -1);

	//// sphere light
	//scene_manager->AddPrimInstance(sphere_idx,
	//	scene_manager->AddTransform(glm::vec3(3.0f, 3.35f, -5.0f), glm::vec3(0.15f, 0.15f, 0.15f)),
	//	scene_manager->AddLightMaterial("sphere light", glm::vec3(1.0f), 200.0f),
	//	1.0f,
	//	world_volume_idx, -1);

	const uint32_t glass_tex_black_idx = scene_manager->AddTexture("glass_tex_black", YumeRT::Texture().InitConstantTextureFloat(0.65f));
	const uint32_t glass_tex_white_idx = scene_manager->AddTexture("glass_tex_white", YumeRT::Texture().InitConstantTextureFloat(0.05f));
	const uint32_t glass_tex_checkerboard = scene_manager->AddTexture("glass_CheckerBoard_0", YumeRT::Texture().InitCheckerBoardTexture(glass_tex_black_idx, glass_tex_white_idx, 16.0f));

	scene_manager->AddPrimInstance(sphere_idx,
		scene_manager->AddTransform(glm::vec3(0.0f, 0.6f, 3.0f), glm::vec3(1.2f)),
		scene_manager->AddSurfaceMaterial("extra sphere", glm::vec3(0.75f, 0.75f, 0.75f), glm::vec3(1.0f), 0.1, 0.1, 1.33, 0.0f, 1.0f, 1.0f, 1),
		1.0f,
		world_volume_idx, -1);

	scene_manager->AddPrimInstance(sphere_idx,
		scene_manager->AddTransform(glm::vec3(1.0f, 0.6f, 3.0f), glm::vec3(1.2f)),
		scene_manager->AddSurfaceMaterial("extra sphere 2", glm::vec3(0.75f, 0.75f, 0.75f), glm::vec3(1.0f, 0.15f, 0.15f), 0.1, 0.1, 1.33, 0.0f, 1.0f, 1.0f, 1),
		1.0f,
		world_volume_idx, -1);

	// ground
	const std::string diffuse_image_tex_path = exec_prefix + std::string("test_assests\\brick_2k.png");
	const uint32_t diffuse_image_tex_idx = scene_manager->AddTexture("test texture brick", YumeRT::Texture().InitImageTexture(
		YumeRT::LoadImageTexture(diffuse_image_tex_path, scene_manager->GetImageTextureManager())
	));
	scene_manager->AddPrimInstance(cube_idx,
		scene_manager->AddTransform(glm::vec3(0.0f, -5.55f, 0.0f), glm::vec3(135.0, 1.0f, 135.0f)),
		scene_manager->AddSurfaceMaterial("ground", glm::vec3(0.55f, 0.55f, 0.55f), glm::vec3(0.0f), 0.2, 0.2, 1.3, 0.0f, 0.0f, 0.0f, 0, 
			diffuse_image_tex_idx, EMPTY_UINT32, diffuse_image_tex_idx),
		1.0f,
		world_volume_idx, -1);

	const uint32_t tex_perlin_noise = scene_manager->AddTexture("Noise_0", YumeRT::Texture().InitNoiseTextureMarble(tex_black_idx, tex_white_idx, 2.0f));

	scene_manager->AddPrimInstance(cube_idx,
		scene_manager->AddTransform(glm::vec3(5.8f, -2.55f, 5.8f), glm::vec3(4.0f, 4.0f, 4.0f)),
		scene_manager->AddSurfaceMaterial("extra cube", glm::vec3(0.55f, 0.55f, 0.55f), glm::vec3(0.0f), 0.2, 0.2, 1.3, 0.0f, 0.0f, 0.0f, 0, tex_perlin_noise),
		1.0f,
		world_volume_idx, -1);

	const uint32_t cube_volume = scene_manager->AddVolume("cube volume", scene_manager->AddTransform(), glm::vec3(0.085f), glm::vec3(0.001f), 0.0f, fog_tex);
	scene_manager->AddPrimInstance(cube_idx,
		scene_manager->AddTransform(glm::vec3(0.0f, 9.0f, 0.0f), glm::vec3(15.0f, 2.0f, 15.0f)),
		scene_manager->AddSurfaceMaterial("extra cube"),
		1.0f,
		world_volume_idx, cube_volume, true);

	scene_manager->AddDistantLight("Distant Light", scene_manager->AddTransform(glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(0.0f, 0.0f, 0.0f)), glm::vec3(1.0f), 2.0f, 8.0f);
}

int main(int argc, char *argv[])
{
	try
	{
		const int width = 1024, height = 800;
		std::shared_ptr<YumeRT::MainSystem> ptr = YumeRT::MainSystem::GetInstance(std::string(argv[0]), width, height);
		TestScene_CornellBox(std::string(argv[0]), ptr->GetSceneManager(), width, height);
		ptr->Run();
	}
	catch (const std::exception& err)
	{
		std::cout << err.what() << std::endl;
	}

	return 0;
}