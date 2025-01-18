#include "src/core/SceneModule.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace YumeRT {
	SceneModule::SceneModule() :scene_resource{}, camera(),  render_setting{}, scene_change_flag(SCENE_INIT)
	{
		camera[EDITOR_CAMERA_INDEX].SetFov(glm::radians(45.0f));
		camera[EDITOR_CAMERA_INDEX].SetPosition(glm::vec3(0.0f, 0.0f, 10.0f));
		camera[EDITOR_CAMERA_INDEX].SetDir(glm::vec3(0.0f, 0.0f, 10.0f) - glm::vec3(0.0f, 0.0f, 4.0f));
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

		FREE_GPU_RESOURCE(scene.materials);
		scene.material_count = 0;
		scene.materials = nullptr;

		FREE_GPU_RESOURCE(scene.textures);
		scene.texture_count = 0;
		scene.textures = nullptr;

		FREE_GPU_RESOURCE(scene.distant_lights);
		scene.distant_light_count = 0;
		scene.distant_lights = nullptr;

		FREE_GPU_RESOURCE(scene.shape_lights);
		FREE_GPU_RESOURCE(scene.shape_light_sample_table);
		scene.shape_light_count = 0;
		scene.shape_lights = nullptr;
		scene.shape_light_sample_table = nullptr;

		FREE_GPU_RESOURCE(scene.volumes);
		scene.volume_count = 0;
		scene.volumes = nullptr;

		FREE_GPU_RESOURCE(scene.image_tile_cache);
		 // free tile device.
		FREE_GPU_RESOURCE(image_tile_cache.image_tiles_device);
		// free tile host.
		image_tile_cache.image_tiles_host = nullptr;
		for (auto& image_tile : image_texture_tiles) {
			FREE_GPU_RESOURCE(image_tile.device_data);
			if (image_tile.host_data != nullptr) {
				free(image_tile.host_data);
			}
		}

		FREE_GPU_RESOURCE(scene.sampler_data.halton_permute_table);
		scene.sampler_data.halton_permute_table = nullptr;
		FREE_GPU_RESOURCE(scene.sampler_data.sobol_matrices);
		scene.sampler_data.sobol_matrices = nullptr;

		FREE_GPU_RESOURCE(scene.render_setting);
		scene.render_setting = nullptr;

		for (auto& geometry : geometries) {
			geometry.Destory();
		}

		ResetSceneFlag(SCENE_INIT);
	}

	uint32_t SceneModule::CreateTransform(const std::string& name, const glm::vec3& translate, const glm::vec3& scale, const glm::vec3& rotate, const int parent_transform_index, const glm::vec3& pivot)
	{
		TransformState transform_state(translate, scale, rotate, parent_transform_index);
		const auto transform_matrix = transform_state.GetTransformMatrix(transform_states, transforms, i_transforms);
		
		transform_state.SetValid();

		transforms.emplace_back(transform_matrix);
		i_transforms.emplace_back(glm::inverse(transform_matrix));
		transform_states.emplace_back(transform_state);
		transform_names.emplace_back(name);

		transform_reference_transform_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		transform_reference_primitive_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		transform_reference_light_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		transform_reference_volume_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		const auto transform_index = (uint32_t)transform_states.size() - 1;
		if (parent_transform_index != EMPTY_UINT32) {
			transform_reference_transform_indices[parent_transform_index][transform_index]++;
		}

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TRANSFORM_CREATE);
		return transform_index;
	}
	void SceneModule::DeleteTransform(const uint32_t index)
	{
	
	}
	void SceneModule::UpdateTransform(const uint32_t index)
	{
		if (transform_states.empty() || !(index < transform_states.size())) {
			return;
		}

		auto& scene_device_data = scene_resource.scene;
		transforms[index] = transform_states[index].GetTransformMatrix(transform_states, transforms, i_transforms);
		i_transforms[index] = glm::inverse(transforms[index]);
		
		transform_states[index].SetValid();

		assert(scene_device_data.transforms != nullptr && scene_device_data.i_transforms != nullptr);
		TRANSFER_TO_GPU(scene_device_data.transforms + index, &transforms[index], sizeof(glm::mat4));
		TRANSFER_TO_GPU(scene_device_data.i_transforms + index, &i_transforms[index], sizeof(glm::mat4));
	}

	uint32_t SceneModule::CreateMesh(const std::string& name,
		std::vector<Triangle>& mesh_triangles,
		std::vector<uint32_t>& mesh_position_idxs,
		std::vector<float>& mesh_positions,
		std::vector<uint32_t>& mesh_normal_idxs,
		std::vector<float>& mesh_normals,
		std::vector<uint32_t>& mesh_texcoord_idxs,
		std::vector<float>& mesh_texcoords)
	{
		geometries.emplace_back(GeometryData().InitCustomMesh(mesh_triangles, 
			mesh_position_idxs, mesh_positions, 
			mesh_normal_idxs, mesh_normals,
			mesh_texcoord_idxs, mesh_texcoords));
		geometry_names.emplace_back(name);

		geometry_reference_primitive_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		auto& mesh = geometries.back();
		mesh.Upload();

		const auto mesh_index = (uint32_t)geometries.size() - 1;

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_GEOMETRY_CREATE);
		return mesh_index;
	}
	uint32_t SceneModule::CreateCube(const std::string& name)
	{
		geometries.emplace_back(GeometryData().InitCube());
		geometry_names.emplace_back(name);

		geometry_reference_primitive_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		auto& cube = geometries.back();
		cube.Upload();

		const auto cube_index = (uint32_t)geometries.size() - 1;

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_GEOMETRY_CREATE);
		return cube_index;
	}
	uint32_t SceneModule::CreateSphere(const std::string& name, const float radius)
	{
		geometries.emplace_back(GeometryData().InitSphere(radius));
		geometry_names.emplace_back(name);

		geometry_reference_primitive_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		auto& sphere = geometries.back();
		sphere.Upload();

		const auto sphere_index = (uint32_t)geometries.size() - 1;

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_GEOMETRY_CREATE);
		return sphere_index;
	}
	void SceneModule::DeleteGeometry(const uint32_t index)
	{
	
	}
	void SceneModule::UpdateGeometry(const uint32_t index)
	{
	
	}

	uint32_t SceneModule::CreatePrimitiveInstance(const std::string& name, const uint32_t geometry_index, const uint32_t transform_index, const uint32_t material_index, const int inner_volume_index, const bool treat_as_boundary)
	{
		if (!(geometry_index < geometries.size() && transform_index < transform_states.size())) {
			return EMPTY_UINT32;
		}

		const auto unique_index = primitive_index_generator.GetNextIndex();

		primitive_instances.emplace_back(PrimitiveInstance(unique_index, geometry_index, transform_index, material_index, inner_volume_index, treat_as_boundary));

		const auto primitive_instance_index = (uint32_t)primitive_instances.size() - 1;
		
		// note: for primitive, use unique index to specify it.
		if (geometry_index != EMPTY_UINT32) {
			geometry_reference_primitive_indices[geometry_index][unique_index]++;
		}
		if (transform_index != EMPTY_UINT32) {
			transform_reference_primitive_indices[transform_index][unique_index]++;
		}
		if (material_index != EMPTY_UINT32) {
			material_reference_primitive_indices[material_index][unique_index]++;
		}
		if (inner_volume_index != EMPTY_UINT32) {
			volume_reference_primitive_indices[inner_volume_index][unique_index]++;
		}

		primitive_index_to_index_map[unique_index] = primitive_instance_index;
		primitive_index_to_name_map[unique_index] = name;

		uint32_t change_flag = SCENE_CHANGE_FLAG::SCENE_INSTANCE_CREATE;
		if ((uint32_t)material_index < materials.size() && materials.at(material_index).material_type == LIGHT_MTL) {
			change_flag |= SCENE_CHANGE_FLAG::SCENE_SHAPE_LIGHT_CREATE;
		}
		AddSceneFlag(change_flag);

		return unique_index;
	}
	void SceneModule::DeletePrimitiveInstance(const uint32_t unique_index)
	{
		const auto iter = primitive_index_to_index_map.find(unique_index);
		if (iter == primitive_index_to_index_map.end() || primitive_instances.empty()) {
			return;
		}
		const uint32_t primitive_offset = iter->second;
		if (!(primitive_offset < primitive_instances.size())) {
			return;
		}

		auto& primitive_to_delete = primitive_instances.at(primitive_offset);
		if ((uint32_t)primitive_to_delete.geometry_idx < geometries.size()) {
			geometry_reference_primitive_indices[primitive_to_delete.geometry_idx].erase(unique_index);
		}
		if ((uint32_t)primitive_to_delete.transform_idx < transform_states.size()) {
			transform_reference_primitive_indices[primitive_to_delete.transform_idx].erase(unique_index);
		}
		if ((uint32_t)primitive_to_delete.material_idx < materials.size()) {
			material_reference_primitive_indices[primitive_to_delete.material_idx].erase(unique_index);
		}
		if ((uint32_t)primitive_to_delete.inner_volume_idx < volumes.size()) {
			volume_reference_primitive_indices[primitive_to_delete.inner_volume_idx].erase(unique_index);
		}

		uint32_t change_flag = SCENE_CHANGE_FLAG::SCENE_INSTANCE_DELETE;
		if ((uint32_t)primitive_to_delete.material_idx < materials.size() && materials.at(primitive_to_delete.material_idx).material_type == LIGHT_MTL) {
			change_flag |= SCENE_CHANGE_FLAG::SCENE_SHAPE_LIGHT_DELETE;
		}

		primitive_index_to_name_map.erase(unique_index);
		primitive_index_to_index_map.erase(unique_index);

		const auto original_size = primitive_instances.size();
		if (primitive_offset != (uint32_t)primitive_instances.size() - 1) {
			auto& last_primitive = primitive_instances.at(primitive_instances.size() - 1);
			primitive_index_to_index_map[last_primitive.unique_index] = primitive_offset;
			Swap(primitive_to_delete, last_primitive);
		}
		primitive_instances.resize(original_size - 1);

		AddSceneFlag(change_flag);
	}
	void SceneModule::UpdatePrimitiveInstance(const uint32_t unique_index)
	{
		const auto iter = primitive_index_to_index_map.find(unique_index);
		if (iter == primitive_index_to_index_map.end() || primitive_instances.empty()) {
			return;
		}
		const uint32_t offset = iter->second;
		if (!(offset < primitive_instances.size())) {
			return;
		}
		
		auto& scene_device_data = scene_resource.scene;
		assert(scene_device_data.primitive_instances != nullptr);
		TRANSFER_TO_GPU(scene_device_data.primitive_instances + offset, &primitive_instances[offset], sizeof(PrimitiveInstance));
	}

	uint32_t SceneModule::CreateDefaultMaterial(const std::string& name,
		const glm::vec3& diffuse_albedo, const glm::vec3& specular_albedo,
		const float roughness_x, const float roughness_y,
		const float ior_n,
		const float metalness,
		const float specular_weight, const float transmission_weight,
		const uint32_t ior_priority,

		const glm::vec3& coat_albedo,
		const float coat_weight,
		const float coat_thickness,
		const float coat_ior,
		const float coat_roughness_x,
		const float coat_roughness_y,

		const uint32_t diffuse_albedo_tex,
		const uint32_t alpha_x_tex,
		const uint32_t specular_albedo_tex,
		const uint32_t alpha_y_tex,
		const uint32_t specular_weight_tex,
		const uint32_t metalness_tex,
		const uint32_t transmission_weight_tex,
		const uint32_t normal_mapping_tex,
		const uint32_t bump_mapping_tex,
		
		const uint32_t coat_albedo_tex,
		const uint32_t coat_weight_tex,
		const uint32_t coat_thickness_tex,
		const uint32_t coat_roughness_x_tex,
		const uint32_t coat_roughness_y_tex,
		
		const uint32_t coat_normal_tex,
		const uint32_t coat_normal_tex_type)
	{
		auto default_material = Material().InitDefaultMtl(diffuse_albedo,
			specular_albedo,
			roughness_x,
			roughness_y,
			ior_n,
			metalness,
			specular_weight,
			transmission_weight,
			ior_priority,

			coat_albedo,
			coat_weight,
			coat_thickness,
			coat_ior,
			coat_roughness_x,
			coat_roughness_y,

			diffuse_albedo_tex,
			alpha_x_tex,
			specular_albedo_tex,
			alpha_y_tex,
			specular_weight_tex,
			metalness_tex,
			transmission_weight_tex,
			normal_mapping_tex,
			bump_mapping_tex,
			
			coat_albedo_tex, 
			coat_weight_tex, 
			coat_thickness_tex,
			coat_roughness_x_tex,
			coat_roughness_y_tex,
			
			coat_normal_tex,
			coat_normal_tex_type);

		materials.emplace_back(default_material);
		material_names.emplace_back(name);

		material_reference_primitive_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		const auto default_material_index = (uint32_t)materials.size() - 1;

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_MATERIAL_CREATE);
		return default_material_index;
	}
	uint32_t SceneModule::CreateLightMaterial(const std::string& name, const glm::vec3& light_color, const float intensity)
	{
		auto light_material = Material().InitLightMtl(light_color, intensity);

		materials.emplace_back(light_material);
		material_names.emplace_back(name);

		material_reference_primitive_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		const auto light_material_index = (uint32_t)materials.size() - 1;
		
		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_MATERIAL_CREATE);
		return light_material_index;
	}
	void SceneModule::DeleteMaterial(const uint32_t index)
	{
		
	}
	void SceneModule::UpdateMaterial(const uint32_t index)
	{
		if (materials.empty() || !(index < materials.size())) {
			return;
		}

		auto& scene_device_data = scene_resource.scene;
		assert(scene_device_data.materials != nullptr);
		TRANSFER_TO_GPU(scene_device_data.materials + index, &materials[index], sizeof(Material));
	}

	uint32_t SceneModule::CreateImageTexture(const std::string& name, const std::string& texture_file_name)
	{
		auto image_texture = LoadImageTexture(texture_file_name, image_texture_files, image_texture_tiles);
		if (image_texture.tile_offset == -1 || image_texture.file_offset == -1) {
			return EMPTY_UINT32;
		}

		const int tile_x_count = (image_texture.width + TEX_TILE_RES_X - 1) / TEX_TILE_RES_X;
		const int tile_y_count = (image_texture.height + TEX_TILE_RES_Y - 1) / TEX_TILE_RES_Y;
		for (int tile_index = 0; tile_index < tile_y_count * tile_x_count; ++tile_index) {
			auto &tile = image_texture_tiles[image_texture.tile_offset + tile_index];
			assert(tile.device_data == nullptr);
			UPLOAD_TO_GPU(tile.device_data, tile.host_data, tile.data_size);
		}

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_IMAGE_TILE_CREATE);

		textures.emplace_back(Texture().InitImageTexture(image_texture));
		texture_names.emplace_back(name);

		texture_reference_material_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_texture_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_volume_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		const auto texture_index = (uint32_t)textures.size() - 1;

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);
		return texture_index;
	}
	uint32_t SceneModule::CreateConstantTexture(const std::string& name, const float value)
	{
		auto constant_texture_float = Texture().InitConstantTextureFloat(value);

		textures.emplace_back(constant_texture_float);
		texture_names.emplace_back(name);

		texture_reference_material_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_texture_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_volume_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		const auto texture_index = (uint32_t)textures.size() - 1;

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);
		return texture_index;
	}
	uint32_t SceneModule::CreateConstantTexture(const std::string& name, const glm::vec3& color)
	{
		auto constant_texture_rgb = Texture().InitConstantTextureRGB(color);

		textures.emplace_back(constant_texture_rgb);
		texture_names.emplace_back(name);

		texture_reference_material_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_texture_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_volume_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		const auto texture_index = (uint32_t)textures.size() - 1;

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);
		return texture_index;
	}
	uint32_t SceneModule::CreateCheckerBoardTexture(const std::string& name,
		const uint32_t texture_black_idx, const uint32_t texture_white_idx,
		const float freqency) 
	{
		auto checkerboard_texture = Texture().InitCheckerBoardTexture(texture_black_idx, texture_white_idx, freqency);

		textures.emplace_back(checkerboard_texture);
		texture_names.emplace_back(name);

		texture_reference_material_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_texture_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_volume_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		const auto texture_index = (uint32_t)textures.size() - 1;
		if (texture_black_idx != EMPTY_UINT32) {
			texture_reference_texture_indices[texture_black_idx][texture_index]++;
		}
		if (texture_white_idx != EMPTY_UINT32) {
			texture_reference_texture_indices[texture_white_idx][texture_index]++;
		}

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);
		return texture_index;
	}
	uint32_t SceneModule::CreateNoiseTexture(const std::string& name,
		const uint32_t texture_black_idx, const uint32_t texture_white_idx,
		const float frequency,
		const bool normalized)
	{
		auto noise_texture = Texture().InitNoiseTexture(texture_black_idx, texture_white_idx, frequency, normalized);

		textures.emplace_back(noise_texture);
		texture_names.emplace_back(name);

		texture_reference_material_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_texture_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_volume_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		const auto texture_index = (uint32_t)textures.size() - 1;
		if (texture_black_idx != EMPTY_UINT32) {
			texture_reference_texture_indices[texture_black_idx][texture_index]++;
		}
		if (texture_white_idx != EMPTY_UINT32) {
			texture_reference_texture_indices[texture_white_idx][texture_index]++;
		}

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);
		return texture_index;
	}
	uint32_t SceneModule::CreateNoiseTextureFBM(const std::string& name,
		const uint32_t texture_black_idx, const uint32_t texture_white_idx,
		const float frequency,
		const float lacunarity, const float gain,
		const int layer_count,
		const float amplitude, const float offset) 
	{
		auto noise_texture_fbm = Texture().InitNoiseTextureFBM(texture_black_idx, texture_white_idx,
			frequency,
			lacunarity, gain,
			layer_count,
			amplitude, offset);

		textures.emplace_back(noise_texture_fbm);
		texture_names.emplace_back(name);

		texture_reference_material_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_texture_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_volume_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		const auto texture_index = (uint32_t)textures.size() - 1;
		if (texture_black_idx != EMPTY_UINT32) {
			texture_reference_texture_indices[texture_black_idx][texture_index]++;
		}
		if (texture_white_idx != EMPTY_UINT32) {
			texture_reference_texture_indices[texture_white_idx][texture_index]++;
		}

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);
		return texture_index;
	}
	uint32_t SceneModule::CreateNoiseTextureTurbulence(const std::string& name,
		const uint32_t texture_black_idx, const uint32_t texture_white_idx,
		const float frequency,
		const float lacunarity, const float gain,
		const int layer_count,
		const float amplitude, const float offset)
	{
		auto noise_texture_turbulence = Texture().InitNoiseTextureTurbulence(texture_black_idx, texture_white_idx,
			frequency,
			lacunarity, gain,
			layer_count,
			amplitude, offset);

		textures.emplace_back(noise_texture_turbulence);
		texture_names.emplace_back(name);

		texture_reference_material_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_texture_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_volume_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		const auto texture_index = (uint32_t)textures.size() - 1;
		if (texture_black_idx != EMPTY_UINT32) {
			texture_reference_texture_indices[texture_black_idx][texture_index]++;
		}
		if (texture_white_idx != EMPTY_UINT32) {
			texture_reference_texture_indices[texture_white_idx][texture_index]++;
		}

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);
		return texture_index;
	}
	uint32_t SceneModule::CreateNoiseTextureMarble(const std::string& name,
		const uint32_t texture_black_idx, const uint32_t texture_white_idx,
		const float frequency,
		const float lacunarity, const float gain,
		const int layer_count,
		const float variation,
		const bool x_turb, const bool y_turb, const bool z_turb) 
	{
		auto noise_texture_marble = Texture().InitNoiseTextureMarble(texture_black_idx, texture_white_idx,
			frequency,
			lacunarity, gain,
			layer_count,
			variation,
			x_turb, y_turb, z_turb);

		textures.emplace_back(noise_texture_marble);
		texture_names.emplace_back(name);

		texture_reference_material_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_texture_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_volume_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		const auto texture_index = (uint32_t)textures.size() - 1;
		if (texture_black_idx != EMPTY_UINT32) {
			texture_reference_texture_indices[texture_black_idx][texture_index]++;
		}
		if (texture_white_idx != EMPTY_UINT32) {
			texture_reference_texture_indices[texture_white_idx][texture_index]++;
		}

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);
		return texture_index;
	}
	uint32_t SceneModule::CreateNoiseTextureWood(const std::string& name,
		const uint32_t texture_black_idx, const uint32_t texture_white_idx,
		const float frequency,
		const float variation) 
	{
		auto noise_texture_wood = Texture().InitNoiseTextureWood(texture_black_idx, texture_white_idx,
			frequency,
			variation);

		textures.emplace_back(noise_texture_wood);
		texture_names.emplace_back(name);

		texture_reference_material_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_texture_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_volume_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		const auto texture_index = (uint32_t)textures.size() - 1;
		if (texture_black_idx != EMPTY_UINT32) {
			texture_reference_texture_indices[texture_black_idx][texture_index]++;
		}
		if (texture_white_idx != EMPTY_UINT32) {
			texture_reference_texture_indices[texture_white_idx][texture_index]++;
		}

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);
		return texture_index;
	}
	uint32_t SceneModule::CreateNoiseTexturePolkaDot(const std::string& name,
		const uint32_t texture_black_idx, const uint32_t texture_white_idx,
		const float frequency,
		const float radius) 
	{
		auto noise_texture_polka_dot = Texture().InitNoiseTexturePolkaDot(texture_black_idx, texture_white_idx,
			frequency,
			radius);

		textures.emplace_back(noise_texture_polka_dot);
		texture_names.emplace_back(name);

		texture_reference_material_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_texture_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_volume_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		const auto texture_index = (uint32_t)textures.size() - 1;
		if (texture_black_idx != EMPTY_UINT32) {
			texture_reference_texture_indices[texture_black_idx][texture_index]++;
		}
		if (texture_white_idx != EMPTY_UINT32) {
			texture_reference_texture_indices[texture_white_idx][texture_index]++;
		}

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);
		return texture_index;
	}
	uint32_t SceneModule::CreateNoiseTextureWave(const std::string& name,
		const uint32_t texture_black_idx, const uint32_t texture_white_idx,
		const float freq_0, const float lacunarity_0,
		const float gain_0, const int layer_count_0,
		const float freq_1, const float lacunarity_1,
		const float gain_1, const int layer_count_1) 
	{
		auto noise_texture_wave = Texture().InitNoiseTextureWave(texture_black_idx, texture_white_idx,
			freq_0, lacunarity_0,
			gain_0, layer_count_0,
			freq_1, lacunarity_1,
			gain_1, layer_count_1);

		textures.emplace_back(noise_texture_wave);
		texture_names.emplace_back(name);

		texture_reference_material_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_texture_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());
		texture_reference_volume_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		const auto texture_index = (uint32_t)textures.size() - 1;
		if (texture_black_idx != EMPTY_UINT32) {
			texture_reference_texture_indices[texture_black_idx][texture_index]++;
		}
		if (texture_white_idx != EMPTY_UINT32) {
			texture_reference_texture_indices[texture_white_idx][texture_index]++;
		}

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);
		return texture_index;
	}
	void SceneModule::DeleteTexture(const uint32_t index)
	{
	
	}
	void SceneModule::UpdateTexture(const uint32_t index)
	{
		if (textures.empty() || !(index < textures.size())) {
			return;
		}

		auto& scene_device_data = scene_resource.scene;
		assert(scene_device_data.textures != nullptr);
		TRANSFER_TO_GPU(scene_device_data.textures + index, &textures[index], sizeof(Texture));
	}

	uint32_t SceneModule::CreateDistantLight(const std::string& name, const glm::vec3& light_color, const uint32_t transform_index, float intensity, float theta_max)
	{
		if (!(transform_index < transform_states.size())) {
			return EMPTY_UINT32;
		}

		DistantLight distant_light(light_color, transform_index, intensity, theta_max);

		distant_lights.emplace_back(distant_light);
		distant_light_names.emplace_back(name);

		const auto distant_light_index = (uint32_t)distant_lights.size() - 1;
		if (transform_index != EMPTY_UINT32) {
			transform_reference_light_indices[transform_index][distant_light_index]++;
		}

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_DISTANT_LIGHT_CREATE);
		return distant_light_index;
	}
	void SceneModule::DeleteDistantLight(const uint32_t index)
	{
	
	}
	void SceneModule::UpdateDistantLight(const uint32_t index)
	{
		if (distant_lights.empty() || !(index < distant_lights.size())) {
			return;
		}

		auto& scene_device_data = scene_resource.scene;
		assert(scene_device_data.distant_lights != nullptr);
		TRANSFER_TO_GPU(scene_device_data.distant_lights + index, &distant_lights[index], sizeof(DistantLight));
	}

	void SceneModule::LoadShapeLight()
	{
		shape_lights.clear();

		const size_t multi_thread_count_threshold = 512u;

		std::mutex shape_light_array_mutex;
		auto shape_light_handle_function = [&](uint32_t primitive_index) {
			const PrimitiveInstance &primitive = primitive_instances[primitive_index];
			const Material &material = materials[primitive.material_idx];
			if (material.material_type != LIGHT_MTL) {
				return;
			}

			const glm::mat4 &otw_matrix = transforms[primitive.transform_idx];
			const TransformState &transform_state = transform_states[primitive.transform_idx];
			const GeometryData &geometry = geometries[primitive.geometry_idx];
			const float irradiance = (material.light_mtl.light_color.x + material.light_mtl.light_color.y + material.light_mtl.light_color.z) * material.light_mtl.intensity * ONE_PI;
			if (geometry.geometry_type == TRIANGLE_MESH) {
				const TriangleMesh &triangle_mesh = geometry.triangle_mesh;
				const Triangle *mesh_triangles = triangle_mesh.GetTrianglesHost();
				const uint32_t *mesh_vidxs = triangle_mesh.GetPositionIndicesHost();
				const glm::vec3 *mesh_positions = triangle_mesh.GetPositionsHost();

				auto triangle_shape_light_handle_function = [&](uint32_t triangle_index) {
					const Triangle &triangle = mesh_triangles[triangle_index];

					const uint32_t vid0 = mesh_vidxs[triangle.id0];
					const uint32_t vid1 = mesh_vidxs[triangle.id1];
					const uint32_t vid2 = mesh_vidxs[triangle.id2];

					const glm::vec3  p0 = TransformPosition(otw_matrix, mesh_positions[vid0]);
					const glm::vec3  p1 = TransformPosition(otw_matrix, mesh_positions[vid1]);
					const glm::vec3  p2 = TransformPosition(otw_matrix, mesh_positions[vid2]);
					
					const float area = glm::length(glm::cross(p1 - p0, p2 - p0));

					ShapeLight shape_light;
					shape_light.instance_idx = primitive_index;
					shape_light.triangle_idx = triangle_index;
					shape_light.power = irradiance * area;
					shape_light.area = 0.5f * area;
					
					{
						std::lock_guard<std::mutex> shape_light_array_lock(shape_light_array_mutex);
						shape_lights.emplace_back(shape_light);
					}
				};
				if (triangle_mesh.triangle_count < multi_thread_count_threshold) {
					for (uint32_t triangle_index = 0; triangle_index < triangle_mesh.triangle_count; ++triangle_index) {
						triangle_shape_light_handle_function(triangle_index);
					}
				}
				else {
					concurrency::parallel_for(0u, (uint32_t)triangle_mesh.triangle_count, triangle_shape_light_handle_function);
				}
			}
			else if (geometry.geometry_type == SPHERE) {
				const float radius = geometry.sphere.radius;
				const float a = radius * transform_state.scale_xyz.x;
				const float b = radius * transform_state.scale_xyz.y;
				const float c = radius * transform_state.scale_xyz.z;
				const float area = (4.0f / 3.0f) * ONE_PI * (a * b + b * c + c * a);

				ShapeLight shape_light;
				shape_light.instance_idx = primitive_index;
				shape_light.triangle_idx = EMPTY_UINT32;
				shape_light.power = irradiance * area;
				shape_light.area = area;

				{
					std::lock_guard<std::mutex> shape_light_array_lock(shape_light_array_mutex);
					shape_lights.emplace_back(shape_light);
				}
			}
			else {
			
			}
		};
		
		if (primitive_instances.size() < multi_thread_count_threshold) {
			for (uint32_t primitive_index = 0; primitive_index < (uint32_t)primitive_instances.size(); ++primitive_index) {
				shape_light_handle_function(primitive_index);
			}
		}
		else {
			concurrency::parallel_for(0u, (uint32_t)primitive_instances.size(), shape_light_handle_function);
		}

		shape_light_sample_table.resize(shape_lights.size());

		float total_power = 0.0f;
		for (uint32_t shape_light_index = 0; shape_light_index < (uint32_t)shape_lights.size(); ++shape_light_index) {
			total_power += shape_lights[shape_light_index].power;
			shape_light_sample_table[shape_light_index] = total_power;
		}
		float total_power_rcp = SafeRcp(total_power);
		for (uint32_t shape_light_index = 0; shape_light_index < (uint32_t)shape_light_sample_table.size(); ++shape_light_index) {
			shape_light_sample_table[shape_light_index] = total_power == 0.0f ? 0.0f : (shape_light_sample_table[shape_light_index] * total_power_rcp);
		}
	}

	uint32_t SceneModule::CreateVolume(const std::string& name, const uint32_t transform_index, const glm::vec3& sigma_t, const glm::vec3& albedo, const float g, const int density_texture_index)
	{
		if (!(transform_index < transforms.size())) {
			return EMPTY_UINT32;
		}

		Volume volume;
		volume.sigma_t = sigma_t;
		volume.albedo = albedo;
		volume.transform_idx = transform_index;
		volume.density_texture_idx = density_texture_index;
		volume.g = g;

		volumes.emplace_back(volume);
		volume_names.emplace_back(name);

		volume_reference_primitive_indices.emplace_back(std::unordered_map<uint32_t, uint32_t>());

		const auto volume_index = (uint32_t)volumes.size() - 1;
		if (transform_index != EMPTY_UINT32) {
			transform_reference_volume_indices[transform_index][volume_index]++;
		}
		if (density_texture_index >= 0 && density_texture_index < textures.size()) {
			texture_reference_volume_indices[density_texture_index][volume_index]++;
		}

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_VOLUME_CREATE);
		return volume_index;
	}
	void SceneModule::DeleteVolume(const uint32_t index)
	{
	
	}
	void SceneModule::UpdateVolume(const uint32_t index)
	{
		if (volumes.empty() || !(index < volumes.size())) {
			return;
		}

		auto& scene_device_data = scene_resource.scene;
		assert(scene_device_data.volumes != nullptr);
		TRANSFER_TO_GPU(scene_device_data.volumes + index, &volumes[index], sizeof(Volume));
	}

	void SceneModule::InitHaltonPermuteTable()
	{
		size_t total_permutes = 0;
		for (uint32_t i = 0; i < prime_count; ++i) { 
			total_permutes += primes[i]; 
		}

		std::random_device rd;
		std::mt19937 rng(rd());

		permute_table.resize(total_permutes, 0);
		uint32_t permutes_offset = 0;
		for (uint32_t i = 0; i < prime_count; ++i) {
			uint32_t *permutes = permute_table.data() + permutes_offset;
			for (uint32_t j = 0; j < primes[i]; ++j) {
				permutes[j] = j;
			}
			std::shuffle(permutes, permutes + primes[i], rng);
			permutes_offset += primes[i];
		}
	}

	Scene SceneModule::GetHostSceneDataPointer()
	{
		Scene scene_host;
		scene_host.geometry_count = (uint32_t)geometries.size();
		scene_host.geometries = geometries.data();
		scene_host.primitive_instance_count = (uint32_t)primitive_instances.size();
		scene_host.primitive_instances = primitive_instances.data();
		scene_host.top_node_count = (uint32_t)top_nodes.size();
		scene_host.top_nodes = top_nodes.data();
		scene_host.transform_count = (uint32_t)transforms.size();
		scene_host.transforms = transforms.data();
		scene_host.i_transforms = i_transforms.data();
		scene_host.material_count = (uint32_t)materials.size();
		scene_host.materials = materials.data();
		scene_host.distant_light_count = (uint32_t)distant_lights.size();
		scene_host.distant_lights = distant_lights.data();
		scene_host.shape_light_count = (uint32_t)shape_lights.size();
		scene_host.shape_lights = shape_lights.data();
		scene_host.shape_light_sample_table = shape_light_sample_table.data();
		scene_host.volume_count = (uint32_t)volumes.size();
		scene_host.volumes = volumes.data();
		scene_host.texture_count = (uint32_t)textures.size();
		scene_host.textures = textures.data();
		scene_host.image_tile_cache = &image_tile_cache;
		scene_host.sampler_data.halton_permute_table = permute_table.data();
		scene_host.sampler_data.sobol_matrices = (uint32_t*)sobol_matrices32;
		scene_host.camera_count = CAMERA_COUNT;
		scene_host.camera = camera;
		return scene_host;
	}

	static bool ReadMeshData(const aiMesh *mesh, 
		std::vector<Triangle>& mesh_triangles,
		std::vector<uint32_t>& mesh_position_idxs,
		std::vector<float>& mesh_positions,
		std::vector<uint32_t>& mesh_normal_idxs,
		std::vector<float>& mesh_normals,
		std::vector<uint32_t>& mesh_texcoord_idxs,
		std::vector<float>& mesh_texcoords) 
	{
		const aiMesh& assimp_mesh = (*mesh);
		if (assimp_mesh.mNumFaces == 0) {
			return false;
		}
		for (int face_index = 0; face_index < assimp_mesh.mNumFaces; ++face_index) {
			Triangle triangle;
			triangle.id0 = (face_index * 3) + 0;
			triangle.id1 = (face_index * 3) + 1;
			triangle.id2 = (face_index * 3) + 2;
			mesh_triangles.push_back(triangle);

			for (int vertex_index = 0; vertex_index < assimp_mesh.mFaces[face_index].mNumIndices; ++vertex_index) {
				uint32_t idx = assimp_mesh.mFaces[face_index].mIndices[vertex_index];
				// note: in most of model they use the same indices set.
				mesh_position_idxs.push_back(idx);
				mesh_normal_idxs.push_back(idx);
				mesh_texcoord_idxs.push_back(idx);
			}
		}

		if (!assimp_mesh.HasPositions()) {
			return false;
		}
				
		for (int position_index = 0; position_index < assimp_mesh.mNumVertices; ++position_index) {
			float p_x, p_y, p_z;
			p_x = assimp_mesh.mVertices[position_index].x;
			p_y = assimp_mesh.mVertices[position_index].y;
			p_z = assimp_mesh.mVertices[position_index].z;
			mesh_positions.push_back(p_x);
			mesh_positions.push_back(p_y);
			mesh_positions.push_back(p_z);
		}

		// TODO: generate smooth normal by yourself.
		if (!assimp_mesh.HasNormals()) {
			return false;
		}

		for (int normal_index = 0; normal_index < assimp_mesh.mNumVertices; ++normal_index) {
			float n_x, n_y, n_z;
			n_x = assimp_mesh.mNormals[normal_index].x;
			n_y = assimp_mesh.mNormals[normal_index].y;
			n_z = assimp_mesh.mNormals[normal_index].z;
			mesh_normals.push_back(n_x);
			mesh_normals.push_back(n_y);
			mesh_normals.push_back(n_z);
		}

		if (assimp_mesh.HasTextureCoords(0)) {
			for (int uv_index = 0; uv_index < assimp_mesh.mNumVertices; ++uv_index)
			{
				float s, t;
				s = assimp_mesh.mTextureCoords[0][uv_index].x;
				t = assimp_mesh.mTextureCoords[0][uv_index].y;
				mesh_texcoords.push_back(s);
				mesh_texcoords.push_back(t);
			}
		}
		return true;
	}

	static uint32_t ReadMeshGeometry(const aiMesh *mesh, SceneModule *scene_module) 
	{
		if (mesh == nullptr) {
			return EMPTY_UINT32;
		}

		auto& scene_manager = *scene_module;
		const std::string mesh_name = std::string(mesh->mName.C_Str());

		std::vector<Triangle> mesh_triangles;
		std::vector<uint32_t> mesh_position_idxs;
		std::vector<float> mesh_positions;
		std::vector<uint32_t> mesh_normal_idxs;
		std::vector<float> mesh_normals;
		std::vector<uint32_t> mesh_texcoord_idxs;
		std::vector<float> mesh_texcoords;
		const bool success = ReadMeshData(mesh, mesh_triangles, mesh_position_idxs, mesh_positions, mesh_normal_idxs, mesh_normals, mesh_texcoord_idxs, mesh_texcoords);
		if (!success) {
			return EMPTY_UINT32;
		}

		const auto mesh_index = scene_manager.CreateMesh(mesh_name, mesh_triangles, mesh_position_idxs, mesh_positions, mesh_normal_idxs, mesh_normals, mesh_texcoord_idxs, mesh_texcoords);
		return mesh_index;
	}

	static uint32_t ReadMeshMaterial(const std::string& file_root_path,
		const aiMesh *mesh,
		const aiScene *scene,
		SceneModule *scene_module, 
		std::unordered_map<uint32_t, uint32_t> &material_map)
	{
		if (mesh == nullptr) {
			return EMPTY_UINT32;
		}

		auto ai_material_index = mesh->mMaterialIndex;
		if (material_map.find(ai_material_index) != material_map.end()) {
			return material_map[ai_material_index];
		}

		auto& ai_material = scene->mMaterials[ai_material_index];
		auto& scene_manager = *scene_module;
		const std::string material_name = std::string(mesh->mName.C_Str()) + std::string("_material");

		glm::vec3 diffuse_albedo = glm::vec3(0.5f);
		glm::vec3 specular_albedo = glm::vec3(1.0f);
		float roughness_x = 0.2f;
		float roughness_y = 0.2f;
		float ior_n = 1.3f;
		float metalness = 0.0f;
		float specular_weight = 0.0f;
		float transmission_weight = 0.0f;
		uint32_t ior_priority = 0;

		aiColor4D ai_diffuse_albedo;
		if (AI_SUCCESS == aiGetMaterialColor(ai_material, AI_MATKEY_COLOR_DIFFUSE, &ai_diffuse_albedo)) {
			diffuse_albedo.x = ai_diffuse_albedo.r, diffuse_albedo.y = ai_diffuse_albedo.g, diffuse_albedo.z = ai_diffuse_albedo.b;
		}
		aiColor4D ai_specular_albedo;
		if (AI_SUCCESS == aiGetMaterialColor(ai_material, AI_MATKEY_COLOR_SPECULAR, &ai_specular_albedo)) {
			specular_albedo.x = ai_specular_albedo.r, specular_albedo.y = ai_specular_albedo.g, specular_albedo.z = ai_specular_albedo.b;
		}
		float ai_roughness;
		if (AI_SUCCESS == aiGetMaterialFloat(ai_material, AI_MATKEY_ROUGHNESS_FACTOR, &ai_roughness)) {
			ai_roughness = glm::clamp(ai_roughness, 0.0f, 1.0f);
			roughness_x = roughness_y = ai_roughness;
		}
		float ai_ior;
		if (AI_SUCCESS == aiGetMaterialFloat(ai_material, AI_MATKEY_REFRACTI, &ai_ior)) {
			ai_ior = glm::max(0.0f, ai_ior);
			ior_n = ai_ior;
		}
		float ai_metalness;
		if (AI_SUCCESS == aiGetMaterialFloat(ai_material, AI_MATKEY_METALLIC_FACTOR, &ai_metalness)) {
			ai_metalness = glm::clamp(ai_metalness, 0.0f, 1.0f);
			metalness = ai_metalness;
		}
		float ai_specular_weight;
		if (AI_SUCCESS == aiGetMaterialFloat(ai_material, AI_MATKEY_SPECULAR_FACTOR, &ai_specular_weight)) {
			ai_specular_weight = glm::clamp(ai_specular_weight, 0.0f, 1.0f);
			specular_weight = ai_specular_weight;
		}
		float ai_transmission_weight;
		if (AI_SUCCESS == aiGetMaterialFloat(ai_material, AI_MATKEY_TRANSMISSION_FACTOR, &ai_transmission_weight)) {
			ai_transmission_weight = glm::clamp(ai_transmission_weight, 0.0f, 1.0f);
			transmission_weight = ai_transmission_weight;
		}

		uint32_t diffuse_albedo_tex = EMPTY_UINT32;
		uint32_t alpha_x_tex = EMPTY_UINT32;
		uint32_t specular_albedo_tex = EMPTY_UINT32;
		uint32_t alpha_y_tex = EMPTY_UINT32;
		uint32_t specular_weight_tex = EMPTY_UINT32;
		uint32_t metalness_tex = EMPTY_UINT32;
		uint32_t transmission_weight_tex = EMPTY_UINT32;
		uint32_t normal_mapping_tex = EMPTY_UINT32;
		uint32_t bump_mapping_tex = EMPTY_UINT32;

		auto get_ai_texture = [&](const std::string& texture_name, const aiTextureType texture_type) {
			if (ai_material->GetTextureCount(texture_type) > 0) {
				aiString file_name;
				if (AI_SUCCESS == ai_material->GetTexture(texture_type, 0, &file_name)) {
					const std::string texture_file_path = file_root_path + std::string(file_name.C_Str());
					 return scene_manager.CreateImageTexture(texture_name, texture_file_path);
				}
			}
			return EMPTY_UINT32;
		};

		diffuse_albedo_tex = get_ai_texture(material_name + std::string("_diffuse_texture"), aiTextureType_DIFFUSE);

		const uint32_t specular_texture = get_ai_texture(material_name + std::string("_specular_texture"), aiTextureType_SPECULAR);
		specular_albedo_tex = specular_weight_tex = specular_texture;

		const uint32_t roughness_texture = get_ai_texture(material_name + std::string("_roughness_texture"), aiTextureType_DIFFUSE_ROUGHNESS);
		alpha_x_tex = alpha_y_tex = roughness_texture;

		metalness_tex = get_ai_texture(material_name + std::string("_metalness_texture"), aiTextureType_METALNESS);

		transmission_weight_tex = get_ai_texture(material_name + std::string("_transmission_texture"), aiTextureType_TRANSMISSION);

		normal_mapping_tex = get_ai_texture(material_name + std::string("_normal_texture"), aiTextureType_NORMALS);

		bump_mapping_tex = get_ai_texture(material_name + std::string("_normal_texture"), aiTextureType_HEIGHT);

		const auto material_index = scene_manager.CreateDefaultMaterial(material_name, diffuse_albedo, specular_albedo, roughness_x, roughness_y, ior_n, metalness, specular_weight, transmission_weight, ior_priority,
			glm::vec3(1.f), 0.0f, 1.0f, 1.6f, 0.2f, 0.2f,
			diffuse_albedo_tex, alpha_x_tex, specular_albedo_tex, alpha_y_tex, specular_weight_tex, metalness_tex, transmission_weight_tex, normal_mapping_tex, bump_mapping_tex);
		material_map[ai_material_index] = material_index;
		
		return material_index;
	}

	static void ReadNodeData(const std::string &file_root_path, 
		const aiNode *node, 
		const aiScene *scene, 
		SceneModule *scene_module,
		std::unordered_map<uint32_t, uint32_t> &material_map, // [key, value]: [assimp material index, material index].
		const uint32_t parent_transform_index, 
		const int inner_volume_index, 
		const bool treat_as_boundary)
	{
		if (node == nullptr) {
			return;
		}
		auto& scene_manager = *scene_module;

		auto node_transform = node->mTransformation;
		aiVector3D node_translate;
		aiVector3D node_scale;
		aiVector3D node_rotate;
		node_transform.Decompose(node_scale, node_rotate, node_translate);

		auto ai_to_glm = [](const aiVector3D &ai_vec) {
			return glm::vec3(ai_vec.x, ai_vec.y, ai_vec.z);
		};

		const std::string transform_name = std::string(node->mName.C_Str()) + std::string("_transform");
		const uint32_t node_transform_index = scene_manager.CreateTransform(transform_name, 
			ai_to_glm(node_translate), 
			ai_to_glm(node_scale), 
			ai_to_glm(node_rotate), 
			parent_transform_index);

		for (int node_mesh_index = 0; node_mesh_index < node->mNumMeshes; ++node_mesh_index) {
			const aiMesh *mesh = scene->mMeshes[node->mMeshes[node_mesh_index]];
			const auto geometry_index = ReadMeshGeometry(mesh, &scene_manager);
			if (geometry_index == EMPTY_UINT32) {
				continue;
			}
			const auto transform_index = node_transform_index;
			const auto material_index = ReadMeshMaterial(file_root_path, mesh, scene, &scene_manager, material_map);
			
			const std::string mesh_name = std::string(mesh->mName.C_Str());
			const std::string instance_name = mesh_name + "_" + transform_name + "_instance";
			scene_manager.CreatePrimitiveInstance(instance_name, geometry_index, transform_index, material_index, inner_volume_index, treat_as_boundary);
		}

		for (int child_index = 0; child_index < node->mNumChildren; ++child_index) {
			ReadNodeData(file_root_path, node->mChildren[child_index], scene, scene_module, material_map, node_transform_index, inner_volume_index, treat_as_boundary);
		}
	}

	void SceneModule::LoadModelFromFile(const std::string& file_name, const uint32_t transform_index, const int inner_volume_index, const bool treat_as_boundary)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(file_name, aiProcess_GenSmoothNormals | aiProcess_Triangulate);
		if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || scene->mRootNode == nullptr) {
			printf("Fail to load model from path:\t %s.\n", file_name.c_str());
			importer.FreeScene();
			return;
		}

		std::unordered_map<uint32_t, uint32_t> material_map;
		ReadNodeData(file_name.substr(0, file_name.rfind("\\") + 1), scene->mRootNode, scene, this, material_map, transform_index, inner_volume_index, treat_as_boundary);
		importer.FreeScene();
	}
};