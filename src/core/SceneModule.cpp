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

		FREE_GPU_RESOURCE(scene.top_nodes);
		scene.top_node_count = 0;
		scene.top_nodes = nullptr;

		FREE_GPU_RESOURCE(scene.primitive_instances);
		scene.primitive_instance_count = 0;
		scene.primitive_instances = nullptr;

		FREE_GPU_RESOURCE(scene.geometries);
		scene.geometry_count = 0;
		scene.geometries = nullptr;

		FREE_GPU_RESOURCE(scene.transforms);
		FREE_GPU_RESOURCE(scene.i_transforms);
		scene.transform_count = 0;
		scene.transforms = nullptr;
		scene.i_transforms = nullptr;

		for (auto& geometry : geometries) {
			geometry.Destory();
		}

		ResetSceneFlag(SCENE_INIT);
	}

	uint32_t SceneModule::CreateTransform(const glm::vec3& translate, const glm::vec3& scale, const glm::vec3& rotate, const glm::vec3& pivot)
	{
		TransformState transform_state;
		transform_state.translate_xyz = translate;
		transform_state.scale_xyz = scale;
		transform_state.rotate_xyz = rotate;

		const auto transform_matrix = transform_state.GetTransformMatrix();

		transforms.emplace_back(transform_matrix);
		i_transforms.emplace_back(glm::inverse(transform_matrix));
		transform_states.emplace_back(transform_state);
		transform_reference_counters.emplace_back(0);

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TRANSFORM_CREATE);

		const auto transform_index = (uint32_t)transform_states.size() - 1;
		return transform_index;
	}

	void SceneModule::DeleteTransform(const uint32_t index)
	{
	
	}

	void SceneModule::UpdateTransform(const uint32_t index)
	{
	
	}

	uint32_t SceneModule::CreateCube(const std::string& name)
	{
		geometries.emplace_back(GeometryData().InitCube());
		geometry_reference_counters.emplace_back(0);
		geometry_names.emplace_back(name);

		auto& cube = geometries.back();
		cube.Upload();

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_GEOMETRY_CREATE);

		const auto cube_index = (uint32_t)geometries.size() - 1;
		return cube_index;
	}

	uint32_t SceneModule::CreateSphere(const std::string& name, const float radius)
	{
		geometries.emplace_back(GeometryData().InitSphere(radius));
		geometry_reference_counters.emplace_back(0);
		geometry_names.emplace_back(name);

		auto& sphere = geometries.back();
		sphere.Upload();

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_GEOMETRY_CREATE);

		const auto sphere_index = (uint32_t)geometries.size() - 1;
		return sphere_index;
	}

	void SceneModule::DeleteGeometry(const uint32_t index)
	{
	
	}

	void SceneModule::UpdateGeometry(const uint32_t index)
	{
	
	}

	uint32_t SceneModule::CreatePrimitiveInstance(const uint32_t geometry_index, const uint32_t transform_index, const uint32_t material_index, const int inner_volume_index, const bool treat_as_boundary)
	{
		assert(geometry_index < geometries.size() && transform_index < transform_states.size());
		auto& geometry_reference_counter = geometry_reference_counters[geometry_index];
		auto& transform_reference_counter = transform_reference_counters[transform_index];

		primitive_instances.emplace_back(PrimitiveInstance(geometry_index, transform_index, material_index, inner_volume_index, treat_as_boundary));
		++geometry_reference_counter, ++transform_reference_counter;

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_INSTANCE_CREATE);

		const auto primitive_instance_index = (uint32_t)primitive_instances.size() - 1;
		return primitive_instance_index;
	}

	void SceneModule::DeletePrimitiveInstance(const uint32_t index)
	{

	}

	void SceneModule::UpdatePrimitiveInstance(const uint32_t index)
	{
	
	}
};