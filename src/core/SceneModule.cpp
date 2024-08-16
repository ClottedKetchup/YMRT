#include "src/core/SceneModule.h"

namespace YumeRT {
	SceneModule::SceneModule() :scene_resource{}, camera(),  render_setting{}, scene_change_flag(SCENE_INIT)
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

	uint32_t SceneModule::CreateTransform(const std::string& name, const glm::vec3& translate, const glm::vec3& scale, const glm::vec3& rotate, const glm::vec3& pivot)
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
		transform_names.emplace_back(name);

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
		assert(geometry_index < geometries.size() && transform_index < transform_states.size() && material_index < materials.size());
		auto& geometry_reference_counter = geometry_reference_counters[geometry_index];
		auto& transform_reference_counter = transform_reference_counters[transform_index];
		auto& material_reference_counter = material_reference_counters[material_index];

		primitive_instances.emplace_back(PrimitiveInstance(geometry_index, transform_index, material_index, inner_volume_index, treat_as_boundary));
		++geometry_reference_counter, ++transform_reference_counter, ++material_reference_counter;

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_INSTANCE_CREATE);
		if (materials[material_index].material_type == LIGHT_MTL) {
			AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_SHAPE_LIGHT_CREATE);
		}

		const auto primitive_instance_index = (uint32_t)primitive_instances.size() - 1;
		return primitive_instance_index;
	}
	void SceneModule::DeletePrimitiveInstance(const uint32_t index)
	{

	}
	void SceneModule::UpdatePrimitiveInstance(const uint32_t index)
	{
	
	}

	uint32_t SceneModule::CreateDefaultMaterial(const std::string& name,
		const glm::vec3& diffuse_albedo, const glm::vec3& specular_albedo,
		const float roughness_x, const float roughness_y,
		const float ior_n,
		const float metalness,
		const float specular_weight, const float transmission_weight,
		const uint32_t ior_priority,

		const uint32_t diffuse_albedo_tex,
		const uint32_t alpha_x_tex,
		const uint32_t specular_albedo_tex,
		const uint32_t alpha_y_tex,
		const uint32_t specular_weight_tex,
		const uint32_t metalness_tex,
		const uint32_t transmission_weight_tex,
		const uint32_t normal_mapping_tex,
		const uint32_t bump_mapping_tex)
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

			diffuse_albedo_tex,
			alpha_x_tex,
			specular_albedo_tex,
			alpha_y_tex,
			specular_weight_tex,
			metalness_tex,
			transmission_weight_tex,
			normal_mapping_tex,
			bump_mapping_tex);

		materials.emplace_back(default_material);
		material_reference_counters.emplace_back(0);
		material_names.emplace_back(name);

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_MATERIAL_CREATE);

		const auto default_material_index = (uint32_t)materials.size() - 1;
		return default_material_index;
	}
	uint32_t SceneModule::CreateLightMaterial(const std::string& name, const glm::vec3& light_color, const float intensity)
	{
		auto light_material = Material().InitLightMtl(light_color, intensity);

		materials.emplace_back(light_material);
		material_reference_counters.emplace_back(0);
		material_names.emplace_back(name);
		
		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_MATERIAL_CREATE);
		
		const auto light_material_index = (uint32_t)materials.size() - 1;
		return light_material_index;
	}
	void SceneModule::DeleteMaterial(const uint32_t index)
	{
		
	}
	void SceneModule::UpdateMaterial(const uint32_t index)
	{
	
	}

	uint32_t SceneModule::CreateImageTexture(const std::string& name,
		const int width, const int height,
		const int tile_offset, const int file_offset,
		const int16_t channel_count,
		const int16_t *mipmap_tile_offset_list,
		const int16_t mipmap_count,
		const int warp_mode ,
		const float u_scale, const float v_scale,
		const float u_offset, const float v_offset) 
	{
		auto image_texture = Texture().InitImageTexture(width, height,
			tile_offset, file_offset,
			channel_count,
			mipmap_tile_offset_list,
			mipmap_count,
			warp_mode,
			u_scale, v_scale,
			u_offset, v_offset);

		textures.emplace_back(image_texture);
		texture_names.emplace_back(name);

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);

		const auto texture_index = (uint32_t)textures.size() - 1;
		return texture_index;
	}
	uint32_t SceneModule::CreateImageTexture(const std::string& name, const ImageTexture& img_tex)
	{
		auto image_texture = Texture().InitImageTexture(img_tex);

		textures.emplace_back(image_texture);
		texture_names.emplace_back(name);

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);

		const auto texture_index = (uint32_t)textures.size() - 1;
		return texture_index;
	}
	uint32_t SceneModule::CreateConstantTexture(const std::string& name, const float value)
	{
		auto constant_texture_float = Texture().InitConstantTextureFloat(value);

		textures.emplace_back(constant_texture_float);
		texture_names.emplace_back(name);

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);

		const auto texture_index = (uint32_t)textures.size() - 1;
		return texture_index;
	}
	uint32_t SceneModule::CreateConstantTexture(const std::string& name, const glm::vec3& color)
	{
		auto constant_texture_rgb = Texture().InitConstantTextureRGB(color);

		textures.emplace_back(constant_texture_rgb);
		texture_names.emplace_back(name);

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);

		const auto texture_index = (uint32_t)textures.size() - 1;
		return texture_index;
	}
	uint32_t SceneModule::CreateCheckerBoardTexture(const std::string& name,
		const uint32_t texture_black_idx, const uint32_t texture_white_idx,
		const float freqency) 
	{
		auto checkerboard_texture = Texture().InitCheckerBoardTexture(texture_black_idx, texture_white_idx, freqency);

		textures.emplace_back(checkerboard_texture);
		texture_names.emplace_back(name);

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);

		const auto texture_index = (uint32_t)textures.size() - 1;
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

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);

		const auto texture_index = (uint32_t)textures.size() - 1;
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

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);

		const auto texture_index = (uint32_t)textures.size() - 1;
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

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);

		const auto texture_index = (uint32_t)textures.size() - 1;
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

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);

		const auto texture_index = (uint32_t)textures.size() - 1;
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

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);

		const auto texture_index = (uint32_t)textures.size() - 1;
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

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);

		const auto texture_index = (uint32_t)textures.size() - 1;
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

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE);

		const auto texture_index = (uint32_t)textures.size() - 1;
		return texture_index;
	}
	void SceneModule::DeleteTexture(const uint32_t index)
	{
	
	}
	void SceneModule::UpdateTexture(const uint32_t index)
	{
	
	}

	uint32_t SceneModule::CreateDistantLight(const std::string& name, const glm::vec3& light_color, const uint32_t transform_index, float intensity, float theta_max)
	{
		assert(transform_index < transform_states.size());
		DistantLight distant_light(light_color, transform_index, intensity, theta_max);

		distant_lights.emplace_back(distant_light);
		distant_light_names.emplace_back(name);

		auto& transform_reference_counter = transform_reference_counters[transform_index];
		++transform_reference_counter;

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_DISTANT_LIGHT_CREATE);

		const auto distant_light_index = (uint32_t)distant_lights.size() - 1;
		return distant_light_index;
	}
	void SceneModule::DeleteDistantLight(const uint32_t index)
	{
	
	}
	void SceneModule::UpdateDistantLight(const uint32_t index)
	{
	
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
		assert(transform_index < transform_states.size());
		Volume volume;
		volume.sigma_t = sigma_t;
		volume.albedo = albedo;
		volume.transform_idx = transform_index;
		volume.density_texture_idx = density_texture_index;
		volume.g = g;

		volumes.emplace_back(volume);
		volume_names.emplace_back(name);

		auto& transform_reference_counter = transform_reference_counters[transform_index];
		++transform_reference_counter;

		AddSceneFlag(SCENE_CHANGE_FLAG::SCENE_VOLUME_CREATE);

		const auto volume_index = (uint32_t)volumes.size() - 1;
		return volume_index;
	}
	void SceneModule::DeleteVolume(const uint32_t index)
	{
	
	}
	void SceneModule::UpdateVolume(const uint32_t index)
	{
	
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
};