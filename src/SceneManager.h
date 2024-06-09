#pragma once

#include <vector>
#include <memory>
#include <iostream>
#include <random>
#include <string>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "SceneDefines.h"
#include "Camera.h"
#include "GeometryDefines.h"
#include "GeometryUtilities.h"
#include "BottomBVH.h"
#include "TopBVH.h"
#include "PrimtiveInstance.h"
#include "Material.h"
#include "Light.h"
#include "Volume.h"
#include "Texture.h"
#include "LowDiscrepancy.h"

namespace YumeRT
{
	struct TransformState
	{
		glm::vec3 T;
		glm::vec3 S;
		glm::vec3 R; // should turn into radians

		inline TransformState() { SetInvalid(); }

		// this function have to specify a pivot.
		inline glm::mat4 GetTransformMatrix(const glm::vec3 &prim_center)
		{
			return  Matrix_T(T) *
						Matrix_T(prim_center) *
						Matrix_R(glm::vec3(0.0f, 0.0f, 1.0f), glm::radians(R.z)) *
						Matrix_R(glm::vec3(0.0f, 1.0f, 0.0f), glm::radians(R.y)) *
						Matrix_R(glm::vec3(1.0f, 0.0f, 0.0f), glm::radians(R.x)) * 
						Matrix_S(S) *
						Matrix_T(-prim_center);
		}

		// always scale and rotate around origin.
		inline glm::mat4 GetTransformMatrix()
		{
			return  Matrix_T(T) *
						Matrix_R(glm::vec3(0.0f, 0.0f, 1.0f), glm::radians(R.z)) *
						Matrix_R(glm::vec3(0.0f, 1.0f, 0.0f), glm::radians(R.y)) *
						Matrix_R(glm::vec3(1.0f, 0.0f, 0.0f), glm::radians(R.x)) *
						Matrix_S(S);
		}

		inline void SetInvalid() { S = glm::vec3(YumeRT_FLOAT_MAX); }
		inline bool Invalid() { return S == glm::vec3(YumeRT_FLOAT_MAX); }
	};

	enum SCENECHANGE_FLAG
	{
		NONE = 0,
		INSTANCE_MOVE = (1 << 0),
		INSTANCE_ADD = (1 << 1),
		INSTANCE_MATERIAL_CHANGE = (1 << 2),
		INSTANCE_REMOVE = (1 << 3),
		GEOMETRY_ADD = (1 << 4),
		GEOMETRY_REMOVE = (1 << 5),
		TRANSFORM_ADD = (1 << 6),
		TRANSFORM_REMOVE = (1 << 7),
		MATERIAL_ADD= (1 << 8),
		MATERIAL_ATTRIBUTE_CHANGE = (1 << 9),
		MATERIAL_REMOVE= (1 << 10),
		MATERIAL_ASSIGN_TO_A_INSTANCE = (1 << 11),
		INSTANCE_EX_IOR_CHANGE = (1 << 12),
		DISTANT_LIGHT_CHANGE = (1 << 13), 
		SHAPE_LIGHT_CHANGE = (1 << 14),
		INSTANCE_VOLUME_CHANGE = (1 << 15),
		VOLUME_CHANGE = (1 << 16),
		DISTANT_LIGHT_ADD_REMOVE = (1 << 17),
		VOLUME_ADD_REMOVE = (1 << 18),
		TEXTURE_ADD_REMOVE = (1 << 19),
		TEXTURE_CHANGE = (1 << 20)
	};

	class SceneModule
	{
	public:
		inline SceneModule(const SceneModule&) = delete;
		inline SceneModule(const SceneModule&&) = delete;
		inline SceneModule& operator=(const SceneModule&) = delete;
		inline SceneModule() 
		{
			printf("SceneManager module init...\n");
			InitHaltonPermuteTable();
			InitSobolMatrices();
		}
		inline ~SceneModule() 
		{
			printf("SceneManager module exit...\n");
			DestroyResources();
		}
		
		inline void DestroyResources();
		inline Scene GetHostSceneData();
		inline void UploadSceneSetting();
		inline void UpdateScene(const Camera &cam, uint32_t *highlight_prim_idx);
		inline void ClearInvaildTransform();

		inline uint32_t AddGeometry(const std::string &name, const GeometryData& geometry);
		inline uint32_t RemoveGeometry(uint32_t geometry_idx);

		// this function only record the transform state: T, S, R
		inline uint32_t AddTransform(const glm::vec3 &T = glm::vec3(0.0f), const glm::vec3 &S = glm::vec3(1.0f), const glm::vec3 &R = glm::vec3(0.0f));
		inline void UpdatePrimTransform(uint32_t selected_prim_idx);
		inline void UpdateDistantLightTransform(uint32_t selected_light_idx);
		inline void UpdateVolumeTransform(uint32_t selected_volume_idx);
		
		// here actually generate the transform matrix, because I need the geometry info
		inline uint32_t AddPrimInstance(uint32_t geometry_idx, uint32_t transform_idx, uint32_t material_idx, int inner_volume_idx = -1, bool treat_as_boundary = 0);
		inline uint32_t RemovePrimInstance(uint32_t selected_prim_idx);
		inline void UpdatePrimVolume(uint32_t selected_prim_idx);

		// material
		inline uint32_t AddLightMaterial(const std::string &name, const glm::vec3 &light_color = glm::vec3(0.5f), float intensity = 1.0f);
		inline uint32_t AddSurfaceMaterial(const std::string &name,
																 const glm::vec3 &diffuse_albedo = glm::vec3(0.5f),
																 const glm::vec3 &specular_albedo = glm::vec3(1.0f),
																 float roughness_x = 0.2f,
																 float roughness_y = 0.2f,
																 float ior_n = 1.3f, 
																 float metalness = 0.0f,
																 float specular_weight = 1.0f,
																 float transmission_weight = 0.0f, 
																 uint32_t ior_priority = 0,

																 uint32_t diffuse_albedo_tex = EMPTY_UINT32,
																 uint32_t alpha_x_tex = EMPTY_UINT32,
																 uint32_t specular_albedo_tex = EMPTY_UINT32,
																 uint32_t alpha_y_tex = EMPTY_UINT32,
																 uint32_t specular_weight_tex = EMPTY_UINT32,
																 uint32_t metalness_tex = EMPTY_UINT32,
																 uint32_t transmission_weight_tex = EMPTY_UINT32, 
																 uint32_t normal_mapping_tex = EMPTY_UINT32,
																 uint32_t bump_mapping_tex = EMPTY_UINT32);
		inline uint32_t RemoveMaterial(uint32_t material_idx);
		inline void UpdateMaterial(uint32_t material_idx);
		inline void AssignMaterialToPrim(uint32_t prim_idx, uint32_t material_idx); // you should use it when only the material change...

		// distant light
		inline uint32_t AddDistantLight(const std::string &name, uint32_t transform_idx, 
			const glm::vec3 &light_color = glm::vec3(1.0f), float intensity = 1.0f, float theta_max = 5.0f);
		inline uint32_t RemoveDistantLight(uint32_t light_idx);
		inline void UpdateDistantLight(uint32_t distant_light_idx);

		// shape light
		inline void LoadShapeLights();

		// volume
		inline uint32_t AddVolume(const std::string &name, uint32_t transform_idx, 
			const glm::vec3 &sigma_t = glm::vec3(0.0f), const glm::vec3 &albedo = glm::vec3(1.0f), float g = 0.0f, int density_texture_idx = -1);
		inline uint32_t RemoveVolume(uint32_t volume_idx);
		inline void UpdateVolume(uint32_t volume_idx);

		// texture
		inline uint32_t AddTexture(const std::string &name, const Texture& texture);
		inline uint32_t RemoveTexture(uint32_t texture_idx);
		inline void UpdateTexture(uint32_t texture_idx);

		// a bunch of short function
		inline Camera& GetCameraRef() { return camera; }
		inline const Scene& GetScene() const { return scene; }
		inline ImageTextureManager& GetImageTextureManager()  { return image_texture_manager; }
		inline const Camera& GetCamera()const { return camera; }
		inline const std::vector<PrimitiveInstance>& GetPrimInstances() const { return prim_instances; }
		inline const std::vector<GeometryData>& GetGeometries() const { return geometries; }
		inline const std::vector<uint32_t>& GetGeometryCounters() const { return geometry_reference_counters; }
		inline const std::vector<std::string>& GetGeometryNames() const { return geometry_names; }
		inline const std::vector<Material>& GetMaterials() const { return materials; }
		inline const std::vector<uint32_t>& GetMaterialCounters() const { return material_reference_counters; }
		inline const std::vector<std::string>& GetMaterialNames() const { return material_names; }
		inline const std::vector<DistantLight>& GetDistantLights() const { return distant_lights; }
		inline const std::vector<std::string>& GetDistantLightNames() const { return distant_light_names; }
		inline const std::vector<Volume>& GetVolumes() const { return volumes; }
		inline const std::vector<std::string>& GetVolumeNames() const { return volume_names; }
		inline const std::vector<Texture>& GetTextures()const { return textures; }
		inline const std::vector<std::string>& GetTextureNames() const { return texture_names; }

		inline float GetTopBVHTime() const { return top_BVH_time; }
		inline float GetBottomBVHTime() const { return bottom_BVH_time; }

		inline TransformState* GetTransformState(uint32_t transform_idx)
		{
			if (transform_states.empty()) { return nullptr; }
			if (transform_idx >(uint32_t)transform_states.size() - 1) { return nullptr; }
			if (transform_states[transform_idx].Invalid()) { return nullptr; }
			return &transform_states[transform_idx];
		}
		inline glm::vec3* GetPrimTransformStateT(uint32_t prim_idx)
		{
			if (prim_instances.empty()) { return nullptr; }
			if (prim_idx >(uint32_t)prim_instances.size() - 1) { return nullptr; }
			uint32_t transform_idx = prim_instances[prim_idx].transform_idx;
			return &transform_states[transform_idx].T;
		}
		inline glm::vec3* GetPrimTransformStateS(uint32_t prim_idx)
		{
			if (prim_instances.empty()) { return nullptr; }
			if (prim_idx >(uint32_t)prim_instances.size() - 1) { return nullptr; }
			uint32_t transform_idx = prim_instances[prim_idx].transform_idx;
			return &transform_states[transform_idx].S;
		}
		inline glm::vec3* GetPrimTransformStateR(uint32_t prim_idx)
		{
			if (prim_instances.empty()) { return nullptr; }
			if (prim_idx >(uint32_t)prim_instances.size() - 1) { return nullptr; }
			uint32_t transform_idx = prim_instances[prim_idx].transform_idx;
			return &transform_states[transform_idx].R;
		}

		inline PrimitiveInstance* GetPrimitiveInstance(uint32_t prim_idx)
		{
			if (prim_instances.empty()) { return nullptr; }
			if (prim_idx >(uint32_t)prim_instances.size() - 1) { return nullptr; }
			return &prim_instances[prim_idx];
		}
		inline Material* GetMaterial(uint32_t material_idx)
		{
			if (materials.empty()) { return nullptr; }
			if (material_idx >(uint32_t)materials.size() - 1) { return nullptr; }
			return &materials[material_idx];
		}
		inline DistantLight* GetDistantLight(uint32_t distant_light_idx)
		{
			if (distant_lights.empty()) { return nullptr; }
			if (distant_light_idx >(uint32_t)distant_lights.size() - 1) { return nullptr; }
			return &distant_lights[distant_light_idx];
		}
		inline Volume* GetVolume(uint32_t vol_idx) 
		{
			if (volumes.empty()) { return nullptr; }
			if (vol_idx > (uint32_t)volumes.size() - 1) { return nullptr; }
			return &volumes[vol_idx];
		}
		inline Texture* GetTexture(uint32_t tex_idx) 
		{
			if (textures.empty()) { return nullptr; }
			if (tex_idx > (uint32_t)textures.size() - 1) { return nullptr; }
			return &textures[tex_idx];
		}

		// get per-frame scene flag
		inline bool IsSceneChange()
		{
			return (scene_camera_change) ||
				(scene_change_flag & SCENECHANGE_FLAG::INSTANCE_ADD) ||
				(scene_change_flag & SCENECHANGE_FLAG::INSTANCE_REMOVE) ||
				(scene_change_flag & SCENECHANGE_FLAG::INSTANCE_MOVE) ||
				(scene_change_flag & SCENECHANGE_FLAG::INSTANCE_MATERIAL_CHANGE) ||
				(scene_change_flag & SCENECHANGE_FLAG::MATERIAL_ASSIGN_TO_A_INSTANCE) ||
				(scene_change_flag & SCENECHANGE_FLAG::INSTANCE_EX_IOR_CHANGE) ||
				(scene_change_flag & SCENECHANGE_FLAG::DISTANT_LIGHT_CHANGE) ||
				(scene_change_flag & SCENECHANGE_FLAG::SHAPE_LIGHT_CHANGE) ||
				(scene_change_flag & SCENECHANGE_FLAG::INSTANCE_VOLUME_CHANGE) ||
				(scene_change_flag & SCENECHANGE_FLAG::VOLUME_CHANGE) ||
				(scene_change_flag & SCENECHANGE_FLAG::DISTANT_LIGHT_ADD_REMOVE) ||
				(scene_change_flag & SCENECHANGE_FLAG::VOLUME_ADD_REMOVE) ||
				(scene_change_flag & SCENECHANGE_FLAG::TEXTURE_ADD_REMOVE) ||
				(scene_change_flag & SCENECHANGE_FLAG::TEXTURE_CHANGE);
		}
		inline void AddSceneFlag(SCENECHANGE_FLAG flag) { scene_change_flag |= flag; }
		inline void ResetSceneFlag() { scene_change_flag = 0; }

	private:
		bool scene_camera_change = false;
		uint32_t scene_change_flag = 0;

		float bottom_BVH_time = 0.0f;
		float top_BVH_time = 0.0f;

		Scene scene;
		ImageTextureManager image_texture_manager;

		Camera camera;
		std::vector<TransformState> transform_states;
		std::vector<glm::mat4> transforms;
		std::vector<glm::mat4> i_transforms;

		std::vector<PrimitiveInstance> prim_instances;

		std::vector<GeometryData> geometries;
		std::vector<uint32_t> geometry_reference_counters;
		std::vector<std::string> geometry_names;

		std::vector<TopNode> top_nodes;

		std::vector<Material> materials;
		std::vector<uint32_t> material_reference_counters;
		std::vector<std::string> material_names;

		std::vector<DistantLight> distant_lights;
		std::vector<std::string> distant_light_names;

		std::vector<ShapeLight> shape_lights;
		std::vector<float> shape_light_sample_table;

		std::vector<Volume> volumes;
		std::vector<std::string> volume_names;

		std::vector<Texture> textures;
		std::vector<std::string> texture_names;

		std::vector<uint32_t> permute_table;

		inline void InitHaltonPermuteTable();
		inline void InitSobolMatrices();
	};

	inline uint32_t SceneModule::AddGeometry(const std::string &name, const GeometryData& geometry)
	{
		// add a shape
		geometries.push_back(geometry);
		// add its counter to 0
		geometry_reference_counters.push_back(0);
		// add its name, now only for display
		geometry_names.push_back(name);

		AddSceneFlag(SCENECHANGE_FLAG::GEOMETRY_ADD);
		return (uint32_t)geometries.size() - 1;
	}
	inline uint32_t SceneModule::RemoveGeometry(uint32_t geometry_idx)
	{
		if (geometries.empty()) { return EMPTY_UINT32; }
		if (geometry_idx >(uint32_t)geometries.size() - 1) { return EMPTY_UINT32; }

		// first clear all the instance
		bool instances_are_change = false;
		uint32_t head = 0, tail = (uint32_t)prim_instances.size() - 1;
		for (; head <= tail && tail != EMPTY_UINT32; )
		{
			if (prim_instances[head].geometry_idx < geometry_idx)
			{
				++head;
			}
			else if (prim_instances[head].geometry_idx > geometry_idx)
			{
				instances_are_change = true;
				prim_instances[head].geometry_idx -= 1;
				++head;
			}
			else
			{
				instances_are_change = true;
				std::swap(prim_instances[head], prim_instances[tail--]);
			}
		}
		prim_instances.resize(head);

		if (instances_are_change) { AddSceneFlag(SCENECHANGE_FLAG::INSTANCE_REMOVE); }

		// release memory
		GeometryData &geometry_to_deleted = geometries[geometry_idx];
		geometry_to_deleted.Destory();

		// remove geometry
		geometries.erase(geometries.begin() + geometry_idx);
		geometry_reference_counters.erase(geometry_reference_counters.begin() + geometry_idx);
		geometry_names.erase(geometry_names.begin() + geometry_idx);

		AddSceneFlag(SCENECHANGE_FLAG::GEOMETRY_REMOVE);

		if (geometries.empty()) { return EMPTY_UINT32; }
		// give the first element back
		return 0;
	}

	inline uint32_t SceneModule::AddTransform(const glm::vec3 &T, const glm::vec3 &S, const glm::vec3 &R)
	{
		TransformState transform_state;
		transform_state.T = T;
		transform_state.S = S;
		transform_state.R = R;
		transform_states.push_back(transform_state);

		transforms.push_back(glm::mat4(0.0f));
		i_transforms.push_back(glm::mat4(0.0f));

		AddSceneFlag(SCENECHANGE_FLAG::TRANSFORM_ADD);
		return (uint32_t)transform_states.size() - 1;
	}
	inline void SceneModule::UpdatePrimTransform(uint32_t prim_idx)
	{
		if (prim_instances.empty() || transform_states.empty()) { return; }
		if (prim_idx >(uint32_t)prim_instances.size() - 1) { return; }

		uint32_t transform_idx = prim_instances[prim_idx].transform_idx;
		uint32_t geometry_idx = prim_instances[prim_idx].geometry_idx;

		const BBox3 object_bbox = GetGeometryBound(geometries[geometry_idx]);
		const glm::vec3 bbox_center = BBox3Center(object_bbox);

		transforms[transform_idx] = transform_states[transform_idx].GetTransformMatrix(bbox_center);
		i_transforms[transform_idx] = glm::inverse(transforms[transform_idx]);

		assert(scene.transforms != nullptr && scene.i_transforms != nullptr);
		CUDA_CHECK(cudaMemcpy(scene.transforms + transform_idx, &transforms[transform_idx], sizeof(glm::mat4), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaMemcpy(scene.i_transforms + transform_idx, &i_transforms[transform_idx], sizeof(glm::mat4), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaDeviceSynchronize());

		AddSceneFlag(SCENECHANGE_FLAG::INSTANCE_MOVE);
	}
	inline void SceneModule::UpdateDistantLightTransform(uint32_t selected_light_idx)
	{
		if (distant_lights.empty() || transform_states.empty()) { return; }
		if (selected_light_idx > (uint32_t)distant_lights.size() - 1) { return; }

		uint32_t transform_idx = distant_lights[selected_light_idx].transform_idx;

		transforms[transform_idx] = transform_states[transform_idx].GetTransformMatrix();
		i_transforms[transform_idx] = glm::inverse(transforms[transform_idx]);

		assert(scene.transforms != nullptr && scene.i_transforms != nullptr);
		CUDA_CHECK(cudaMemcpy(scene.transforms + transform_idx, &transforms[transform_idx], sizeof(glm::mat4), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaMemcpy(scene.i_transforms + transform_idx, &i_transforms[transform_idx], sizeof(glm::mat4), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaDeviceSynchronize());

		AddSceneFlag(SCENECHANGE_FLAG::DISTANT_LIGHT_CHANGE);
	}
	inline void SceneModule::UpdateVolumeTransform(uint32_t selected_volume_idx)
	{
		if (volumes.empty() || transform_states.empty()) { return; }
		if (selected_volume_idx > (uint32_t)volumes.size() - 1) { return; }

		uint32_t transform_idx = volumes[selected_volume_idx].transform_idx;

		transforms[transform_idx] = transform_states[transform_idx].GetTransformMatrix();
		i_transforms[transform_idx] = glm::inverse(transforms[transform_idx]);

		assert(scene.transforms != nullptr && scene.i_transforms != nullptr);
		CUDA_CHECK(cudaMemcpy(scene.transforms + transform_idx, &transforms[transform_idx], sizeof(glm::mat4), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaMemcpy(scene.i_transforms + transform_idx, &i_transforms[transform_idx], sizeof(glm::mat4), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaDeviceSynchronize());
		
		AddSceneFlag(SCENECHANGE_FLAG::VOLUME_CHANGE);
	}

	inline uint32_t SceneModule::AddPrimInstance(uint32_t geometry_idx, uint32_t transform_idx, uint32_t material_idx, int inner_volume_idx, bool treat_as_boundary)
	{
		assert(geometry_idx >= 0 && geometry_idx < geometries.size());
		assert(material_idx >= 0 && material_idx < materials.size());
		prim_instances.push_back(PrimitiveInstance(geometry_idx, transform_idx, material_idx, inner_volume_idx, (int)treat_as_boundary));
		
		geometry_reference_counters[geometry_idx]++;
		material_reference_counters[material_idx]++;

		const BBox3 object_bbox = GetGeometryBound(geometries[geometry_idx]);
		const glm::vec3 bbox_center = BBox3Center(object_bbox);

		transforms[transform_idx] = transform_states[transform_idx].GetTransformMatrix(bbox_center);
		i_transforms[transform_idx] = glm::inverse(transforms[transform_idx]);

		AddSceneFlag(SCENECHANGE_FLAG::INSTANCE_ADD);
		return (uint32_t)prim_instances.size() - 1;
	}
	inline uint32_t SceneModule::RemovePrimInstance(uint32_t selected_prim_idx)
	{
		if (prim_instances.empty()) { return EMPTY_UINT32; }
		if (selected_prim_idx >(uint32_t)prim_instances.size() - 1) { return EMPTY_UINT32; }

		const uint32_t transform_idx = prim_instances[selected_prim_idx].transform_idx;
		const uint32_t geometry_idx = prim_instances[selected_prim_idx].geometry_idx;
		const uint32_t material_idx = prim_instances[selected_prim_idx].material_idx;

		const uint32_t old_size = prim_instances.size();
		std::swap(prim_instances[old_size - 1], prim_instances[selected_prim_idx]);
		prim_instances.resize(old_size - 1);

		// mark its unique transform, we will delete them later
		transform_states[transform_idx].SetInvalid();

		// geometry reference counter...
		geometry_reference_counters[geometry_idx]--;

		// material counter
		material_reference_counters[material_idx]--;

		AddSceneFlag(SCENECHANGE_FLAG::INSTANCE_REMOVE);

		if (prim_instances.empty()) { return EMPTY_UINT32; }

		// give the first element back
		return 0;
	}
	inline void SceneModule::UpdatePrimVolume(uint32_t selected_prim_idx)
	{
		if (prim_instances.empty() || 
			selected_prim_idx > (uint32_t)prim_instances.size() - 1) { 
			return; 
		}
		assert(scene.prim_instances != nullptr);
		CUDA_CHECK(cudaMemcpy(scene.prim_instances + selected_prim_idx, 
			&prim_instances[selected_prim_idx], 
			sizeof(PrimitiveInstance), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaDeviceSynchronize());
		AddSceneFlag(SCENECHANGE_FLAG::INSTANCE_VOLUME_CHANGE);
	}

	// TODO: Material add, remove ,edit...
	inline uint32_t SceneModule::AddLightMaterial(const std::string &name, const glm::vec3 &light_color, float intensity)
	{
		materials.push_back(Material().InitLightMtl(light_color, intensity));
		material_names.push_back(name);
		material_reference_counters.push_back(0);

		AddSceneFlag(SCENECHANGE_FLAG::MATERIAL_ADD);
		return (uint32_t)materials.size() - 1;
	}
	inline uint32_t SceneModule::AddSurfaceMaterial(const std::string &name,
																				const glm::vec3 &diffuse_albedo,
																				const glm::vec3 &specular_albedo,
																				float roughness_x,
																				float roughness_y,
																				float ior_n, 
																				float metalness,
																				float specular_weight,
																				float transmission_weight, 
																				uint32_t ior_priority,

																				uint32_t diffuse_albedo_tex,
																				uint32_t alpha_x_tex,
																				uint32_t specular_albedo_tex,
																				uint32_t alpha_y_tex,
																				uint32_t specular_weight_tex,
																				uint32_t metalness_tex,
																				uint32_t transmission_weight_tex, 
																				uint32_t normal_mapping_tex,
																				uint32_t bump_mapping_tex)
	{	
		materials.push_back(Material().InitDefaultMtl(diffuse_albedo,
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
			bump_mapping_tex));
		material_names.push_back(name);
		material_reference_counters.push_back(0);
	
		AddSceneFlag(SCENECHANGE_FLAG::MATERIAL_ADD);
		return (uint32_t)materials.size() - 1;
	}
	inline uint32_t SceneModule::RemoveMaterial(uint32_t material_idx)
	{
		if (materials.empty()) { return EMPTY_UINT32; }
		if (material_idx < 0 || material_idx >(uint32_t)materials.size() - 1) { return EMPTY_UINT32; }

		// always preserve a default material...
		if (material_idx == 0) { return 0; }

		std::atomic<uint32_t> material_change_instance_count(0);
		std::atomic<uint32_t> null_material_instance_count(0);
		std::vector<PrimitiveInstance> &prims = prim_instances;
		concurrency::parallel_for(0u, (uint32_t)prims.size(), [&](uint32_t prim_idx)
		{
			if (prims[prim_idx].material_idx == material_idx)
			{
				null_material_instance_count.fetch_add(1);
				
				prims[prim_idx].material_idx = 0;
				CUDA_CHECK(cudaMemcpy(scene.prim_instances + prim_idx, &prim_instances[prim_idx], sizeof(PrimitiveInstance), cudaMemcpyHostToDevice));
			}
			else if (prims[prim_idx].material_idx > material_idx)
			{
				material_change_instance_count.fetch_add(1);
				
				prims[prim_idx].material_idx -= 1;
				CUDA_CHECK(cudaMemcpy(scene.prim_instances + prim_idx, &prim_instances[prim_idx], sizeof(PrimitiveInstance), cudaMemcpyHostToDevice));
			}
		});

		// TODO : just update instance at here
		material_reference_counters[0] += null_material_instance_count;
		if (material_change_instance_count > 0 || null_material_instance_count > 0)
		{
			AddSceneFlag(SCENECHANGE_FLAG::INSTANCE_MATERIAL_CHANGE);
			if (materials[material_idx].material_type == LIGHT_MTL)
			{
				AddSceneFlag(SCENECHANGE_FLAG::SHAPE_LIGHT_CHANGE);
			}
		}

		materials.erase(materials.begin() + material_idx);
		material_reference_counters.erase(material_reference_counters.begin() + material_idx);
		material_names.erase(material_names.begin() + material_idx);

		AddSceneFlag(SCENECHANGE_FLAG::MATERIAL_REMOVE);
		CUDA_CHECK(cudaDeviceSynchronize());

		return materials.empty()? EMPTY_UINT32: 0;
	}
	inline void SceneModule::UpdateMaterial(uint32_t material_idx)
	{
		if (materials.empty()) { return; }
		if (material_idx >(uint32_t)materials.size() - 1) { return; }

		assert(scene.materials != nullptr);
		CUDA_CHECK(cudaMemcpy(scene.materials + material_idx, &materials[material_idx], sizeof(Material), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaDeviceSynchronize());

		// TODO: don't update material at updateScene
		if (material_reference_counters[material_idx] > 0)
		{
			AddSceneFlag(SCENECHANGE_FLAG::INSTANCE_MATERIAL_CHANGE);
			if (materials[material_idx].material_type == LIGHT_MTL)
			{
				AddSceneFlag(SCENECHANGE_FLAG::SHAPE_LIGHT_CHANGE);
			}
		}
	}
	inline void SceneModule::AssignMaterialToPrim(uint32_t prim_idx, uint32_t material_idx)
	{
		if (materials.empty() || prim_instances.empty()) { return; }
		if (prim_idx >(uint32_t)prim_instances.size() - 1) { return; }
		if (material_idx >(uint32_t)materials.size() - 1) { return; }
		if (prim_instances[prim_idx].material_idx == material_idx) { return; }

		uint32_t previous_mtl_idx = prim_instances[prim_idx].material_idx;
		material_reference_counters[previous_mtl_idx] -= 1;

		prim_instances[prim_idx].material_idx = material_idx;
		material_reference_counters[material_idx] += 1;

		assert(scene.prim_instances != nullptr);
		CUDA_CHECK(cudaMemcpy(scene.prim_instances + prim_idx, &prim_instances[prim_idx], sizeof(PrimitiveInstance), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaDeviceSynchronize());

		AddSceneFlag(SCENECHANGE_FLAG::MATERIAL_ASSIGN_TO_A_INSTANCE);
		if (materials[material_idx].material_type == LIGHT_MTL)
		{
			AddSceneFlag(SCENECHANGE_FLAG::SHAPE_LIGHT_CHANGE);
		}
	}

	// TODO : distant light gui
	inline uint32_t SceneModule::AddDistantLight(const std::string &name, uint32_t transform_idx, 
		const glm::vec3 &light_color , float intensity, float theta_max)
	{
		DistantLight light;
		light.light_color = light_color;
		light.intensity = intensity;
		light.theta_max = theta_max;
		light.cos_theta_max = glm::cos(glm::radians(theta_max));

		light.transform_idx = transform_idx;

		distant_lights.push_back(light);
		distant_light_names.push_back(name);

		transforms[transform_idx] = transform_states[transform_idx].GetTransformMatrix(glm::vec3(0.0f));
		i_transforms[transform_idx] = glm::inverse(transforms[transform_idx]);

		// AddSceneFlag...
		AddSceneFlag(SCENECHANGE_FLAG::DISTANT_LIGHT_ADD_REMOVE);
		return (uint32_t)distant_lights.size() - 1;
	}
	inline uint32_t SceneModule::RemoveDistantLight(uint32_t light_idx)
	{
		if (distant_lights.empty()) { return EMPTY_UINT32; }
		if (light_idx >(uint32_t)distant_lights.size() - 1) { return EMPTY_UINT32; }

		transform_states[distant_lights[light_idx].transform_idx].SetInvalid();

		// TODO: set the transform to invalid
		distant_lights.erase(distant_lights.begin() + light_idx);
		distant_light_names.erase(distant_light_names.begin() + light_idx);
		
		AddSceneFlag(SCENECHANGE_FLAG::DISTANT_LIGHT_ADD_REMOVE);

		return distant_lights.empty() ? EMPTY_UINT32 : 0;
	}
	inline void SceneModule::UpdateDistantLight(uint32_t distant_light_idx)
	{
		if (distant_lights.empty()) { return; }
		if (distant_light_idx >(uint32_t)distant_lights.size() - 1) { return; }

		assert(scene.distant_lights != nullptr);
		CUDA_CHECK(cudaMemcpy(scene.distant_lights + distant_light_idx, &distant_lights[distant_light_idx], sizeof(DistantLight), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaDeviceSynchronize());

		AddSceneFlag(SCENECHANGE_FLAG::DISTANT_LIGHT_CHANGE);
	}
	
	inline void SceneModule::LoadShapeLights()
	{
		// delete last info
		shape_lights.clear();

		// Init shape lights
		for (uint32_t prim_idx = 0; prim_idx < (uint32_t)prim_instances.size(); ++prim_idx)
		{
			const PrimitiveInstance &prim = prim_instances[prim_idx];
			const Material &mtl = materials[prim.material_idx];
			if (mtl.material_type != LIGHT_MTL) { continue; }

			const glm::mat4& otw = transforms[prim.transform_idx];
			const TransformState &transform_state = transform_states[prim.transform_idx];
			const GeometryData &geometry = geometries[prim.geometry_idx];

			// compute irradiance
			float irradiance = (mtl.light_mtl.light_color.x + mtl.light_mtl.light_color.y + mtl.light_mtl.light_color.z) * mtl.light_mtl.intensity * ONE_PI;

			if (geometry.geometry_type == TRIANGLE_MESH)
			{
				const TriangleMesh &triangle_mesh = geometry.triangle_mesh;
				const Triangle *mesh_triangles = triangle_mesh.GetTrianglesHost();
				const uint32_t *mesh_vidxs = triangle_mesh.GetPositionIndicesHost();
				const glm::vec3 *mesh_positions = triangle_mesh.GetPositionsHost();
				
				for (uint32_t triangle_idx = 0; triangle_idx < triangle_mesh.triangle_count; ++triangle_idx)
				{
					const Triangle &triangle = mesh_triangles[triangle_idx];

					const uint32_t vid0 = mesh_vidxs[triangle.id0];
					const uint32_t vid1 = mesh_vidxs[triangle.id1];
					const uint32_t vid2 = mesh_vidxs[triangle.id2];

					const glm::vec3  p0 = glm::vec3(otw * glm::vec4(mesh_positions[vid0], 1.0f));
					const glm::vec3  p1 = glm::vec3(otw * glm::vec4(mesh_positions[vid1], 1.0f));
					const glm::vec3  p2 = glm::vec3(otw * glm::vec4(mesh_positions[vid2], 1.0f));

					const float area = glm::length(glm::cross(p1 - p0, p2 - p0));

					ShapeLight shape_light;
					shape_light.instance_idx = prim_idx;
					shape_light.triangle_idx = triangle_idx;
					shape_light.power = irradiance * area;
					shape_light.area = 0.5f * area;

					shape_lights.push_back(shape_light);
				}
			}
			else if (geometry.geometry_type == SPHERE)
			{
				const float radius = geometry.sphere.radius;
				const float a = radius * transform_state.S.x;
				const float b = radius * transform_state.S.y;
				const float c = radius * transform_state.S.z;
				const float area = (4.0f / 3.0f) * ONE_PI * (a * b + b * c + c * a);

				ShapeLight shape_light;
				shape_light.instance_idx = prim_idx;
				shape_light.triangle_idx = EMPTY_UINT32;
				shape_light.power = irradiance * area;
				shape_light.area = area;

				shape_lights.push_back(shape_light);
			}
			else
			{

			}
		}

		shape_light_sample_table.resize(shape_lights.size());

		float total_power = 0.0f;
		for (uint32_t i = 0; i < (uint32_t)shape_lights.size(); ++i)
		{
			total_power += shape_lights[i].power;
			shape_light_sample_table[i] = total_power;
		}

		for (uint32_t i = 0; i < (uint32_t)shape_light_sample_table.size(); ++i)
		{
			shape_light_sample_table[i] = total_power == 0.0f ? 0.0f : (shape_light_sample_table[i] / total_power);
		}
	}

	// TODO: volume GUI
	inline uint32_t SceneModule::AddVolume(const std::string &name, uint32_t transform_idx,
		const glm::vec3 &sigma_t, const glm::vec3 &albedo, float g, int density_texture_idx)
	{
		Volume volume;
		volume.sigma_t = sigma_t;
		volume.albedo = albedo;
		volume.transform_idx = transform_idx;
		volume.density_texture_idx = density_texture_idx;
		volume.g = g;

		transforms[transform_idx] = transform_states[transform_idx].GetTransformMatrix();
		i_transforms[transform_idx] = glm::inverse(transforms[transform_idx]);

		volumes.push_back(volume);
		volume_names.push_back(name);

		AddSceneFlag(VOLUME_ADD_REMOVE);
		return (uint32_t)volumes.size() - 1;
	}
	inline uint32_t SceneModule::RemoveVolume(uint32_t volume_idx)
	{
		if (volumes.empty()) { return EMPTY_UINT32; }
		if (volume_idx > (uint32_t)volumes.size() - 1) { return EMPTY_UINT32; }

		transform_states[volumes[volume_idx].transform_idx].SetInvalid();

		// just leave user to manually set...
		volumes.erase(volumes.begin() + volume_idx);
		volume_names.erase(volume_names.begin() + volume_idx);
		
		AddSceneFlag(VOLUME_ADD_REMOVE);
		CUDA_CHECK(cudaDeviceSynchronize());

		return volumes.empty() ? EMPTY_UINT32 : 0;
	}
	inline void SceneModule::UpdateVolume(uint32_t volume_idx)
	{
		if (volumes.empty()) { return; }
		if (volume_idx > (uint32_t)volumes.size() - 1) { return; }

		assert(scene.volumes != nullptr);
		CUDA_CHECK(cudaMemcpy(scene.volumes + volume_idx, &volumes[volume_idx], sizeof(Volume), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaDeviceSynchronize());

		AddSceneFlag(SCENECHANGE_FLAG::VOLUME_CHANGE);
	}

	// TODO: texture GUI
	inline uint32_t SceneModule::AddTexture(const std::string &name, const Texture& texture)
	{
		textures.push_back(texture);
		texture_names.push_back(name);

		AddSceneFlag(SCENECHANGE_FLAG::TEXTURE_ADD_REMOVE);
		return (uint32_t)textures.size() - 1;
	}
	inline void SceneModule::UpdateTexture(uint32_t texture_idx)
	{
		if (textures.empty()) { return; }
		if (texture_idx > (uint32_t)textures.size() - 1) { return; }

		assert(scene.textures != nullptr);
		CUDA_CHECK(cudaMemcpy(scene.textures + texture_idx, &textures[texture_idx], sizeof(Texture), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaDeviceSynchronize());

		AddSceneFlag(SCENECHANGE_FLAG::TEXTURE_CHANGE);
	}

	inline void SceneModule::InitHaltonPermuteTable()
	{
		size_t total_permutes = 0;
		for (uint32_t i = 0; i < prime_count; ++i) { total_permutes += primes[i]; }

		auto m_hash = [&](uint32_t base, uint32_t index)->uint32_t
		{
			const uint32_t PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
			const uint32_t PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
			uint32_t h32 = index + PRIME32_5 + base * PRIME32_3;
			h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
			h32 = PRIME32_2 * (h32 ^ (h32 >> 15));
			h32 = PRIME32_3 * (h32 ^ (h32 >> 13));
			return h32 ^ (h32 >> 16);
		};

		auto m_shuffle = [&](uint32_t *digits, uint32_t prime)
		{
			for (int i = 0; i < prime; ++i)
			{
				int rest_count = prime - i;
				int target = i + m_hash(prime, i) % rest_count;
				Swap(digits[i], digits[target]);
			}
		};

		std::random_device rd;
		std::mt19937 rng(rd());

		permute_table.resize(total_permutes, 0);
		uint32_t permutes_offset = 0;
		for (uint32_t i = 0; i < prime_count; ++i)
		{
			uint32_t *permutes = permute_table.data() + permutes_offset;
			for (uint32_t j = 0; j < primes[i]; ++j)
			{
				permutes[j] = j;
			}

			std::shuffle(permutes, permutes + primes[i], rng);
			permutes_offset += primes[i];
		}

		UPLOAD_TO_GPU(scene.sampler_data.halton_permute_table, permute_table.data(), sizeof(uint32_t) * permute_table.size());
	}
	inline void SceneModule::InitSobolMatrices()
	{
		UPLOAD_TO_GPU(scene.sampler_data.sobol_matrices, sobol_matrices32, sizeof(uint32_t) * 4096u * 32u);
	}
	inline void SceneModule::DestroyResources()
	{
		FREE_GPU_RESOURCE(scene.camera);

		scene.geometry_count = 0;
		FREE_GPU_RESOURCE(scene.geometries);

		scene.prim_instance_count = 0;
		FREE_GPU_RESOURCE(scene.prim_instances);

		scene.top_node_count = 0;
		FREE_GPU_RESOURCE(scene.top_nodes);

		scene.transform_count = 0;
		FREE_GPU_RESOURCE(scene.transforms);
		FREE_GPU_RESOURCE(scene.i_transforms);

		scene.material_count = 0;
		FREE_GPU_RESOURCE(scene.materials);

		scene.distant_light_count = 0;
		FREE_GPU_RESOURCE(scene.distant_lights);

		scene.shape_light_count = 0;
		FREE_GPU_RESOURCE(scene.shape_lights);
		FREE_GPU_RESOURCE(scene.shape_light_sample_table);

		scene.volume_count = 0;
		FREE_GPU_RESOURCE(scene.volumes);

		scene.texture_count = 0;
		FREE_GPU_RESOURCE(scene.textures);

		FREE_GPU_RESOURCE(scene.sampler_data.halton_permute_table);
		FREE_GPU_RESOURCE(scene.sampler_data.sobol_matrices);

		for (auto& geometry : geometries) { geometry.Destory(); }

		image_texture_manager.Release();

		assert(scene.camera == nullptr);
		assert(scene.geometries == nullptr);
		assert(scene.prim_instances == nullptr);
		assert(scene.top_nodes == nullptr);
		assert(scene.transforms == nullptr);
		assert(scene.i_transforms == nullptr);
		assert(scene.materials == nullptr);
		assert(scene.distant_lights == nullptr);
		assert(scene.shape_lights == nullptr);
		assert(scene.shape_light_sample_table == nullptr);
		assert(scene.volumes == nullptr);
		assert(scene.textures == nullptr);
		assert(scene.sampler_data.halton_permute_table == nullptr);
		assert(scene.sampler_data.sobol_matrices == nullptr);
	}

	inline Scene SceneModule::GetHostSceneData() 
	{
		Scene scene;

		scene.geometry_count = (uint32_t)geometries.size();
		scene.geometries = geometries.data();

		scene.prim_instance_count = (uint32_t)prim_instances.size();
		scene.prim_instances = prim_instances.data();

		scene.top_node_count = (uint32_t)top_nodes.size();
		scene.top_nodes = top_nodes.data();

		scene.transform_count = (uint32_t)transforms.size();
		scene.transforms = transforms.data();
		scene.i_transforms = i_transforms.data();

		scene.material_count = (uint32_t)materials.size();
		scene.materials = materials.data();

		scene.distant_light_count = (uint32_t)distant_lights.size();
		scene.distant_lights = distant_lights.data();

		scene.shape_light_count = (uint32_t)shape_lights.size();
		scene.shape_lights = shape_lights.data();
		scene.shape_light_sample_table = shape_light_sample_table.data();

		scene.volume_count = (uint32_t)volumes.size();
		scene.volumes = volumes.data();

		scene.texture_count = (uint32_t)textures.size();
		scene.textures = textures.data();

		image_texture_manager.RefreshHostData();

		scene.camera = &camera;
		return scene;
	}

	inline void SceneModule::UploadSceneSetting()
	{
		for (auto& geometry : geometries)
		{
			geometry.Upload();
		}
		ACBVHBuilder SceneBuilder(prim_instances.data(),
			(uint32_t)prim_instances.size(),
			transforms.data(),
			geometries.data());
		SceneBuilder.BuildSceneBVH(top_nodes, &top_BVH_time);

		LoadShapeLights();

		scene.geometry_count = (uint32_t)geometries.size();
		UPLOAD_TO_GPU(scene.geometries, geometries.data(), sizeof(GeometryData) * geometries.size());

		scene.prim_instance_count = (uint32_t)prim_instances.size();
		UPLOAD_TO_GPU(scene.prim_instances, prim_instances.data(), sizeof(PrimitiveInstance) * prim_instances.size());

		scene.top_node_count = (uint32_t)top_nodes.size();
		UPLOAD_TO_GPU(scene.top_nodes, top_nodes.data(), sizeof(TopNode) * top_nodes.size());

		scene.transform_count = (uint32_t)transforms.size();
		UPLOAD_TO_GPU(scene.transforms, transforms.data(), sizeof(glm::mat4) * transforms.size());
		UPLOAD_TO_GPU(scene.i_transforms, i_transforms.data(), sizeof(glm::mat4) * i_transforms.size());

		scene.material_count = (uint32_t)materials.size();
		UPLOAD_TO_GPU(scene.materials, materials.data(), sizeof(Material) * materials.size());

		scene.distant_light_count = (uint32_t)distant_lights.size();
		UPLOAD_TO_GPU(scene.distant_lights, distant_lights.data(), sizeof(DistantLight) * distant_lights.size());

		scene.shape_light_count = (uint32_t)shape_lights.size();
		UPLOAD_TO_GPU(scene.shape_lights, shape_lights.data(), sizeof(ShapeLight) * shape_lights.size());
		UPLOAD_TO_GPU(scene.shape_light_sample_table, shape_light_sample_table.data(), sizeof(float) * shape_light_sample_table.size());

		scene.volume_count = (uint32_t)volumes.size();
		UPLOAD_TO_GPU(scene.volumes, volumes.data(), sizeof(Volume) * volumes.size());

		scene.texture_count = (uint32_t)textures.size();
		UPLOAD_TO_GPU(scene.textures, textures.data(), sizeof(Texture) * textures.size());

		image_texture_manager.UploadToDevice();

		camera.InitCameraRayTransfer(this->GetHostSceneData());
		UPLOAD_TO_GPU(scene.camera, &camera, sizeof(Camera));

		ResetSceneFlag();
	}
	inline void SceneModule::UpdateScene(const Camera &cam, uint32_t *highlight_prim_idx)
	{
		// handle transform compact
		ClearInvaildTransform();

		if ((scene_change_flag & GEOMETRY_ADD) ||
			(scene_change_flag & GEOMETRY_REMOVE))
		{
			FREE_GPU_RESOURCE(scene.geometries);
			if ((scene_change_flag & GEOMETRY_ADD))
			{
				for (auto& geometry : geometries)
				{
					geometry.Upload();
				}
			}
			scene.geometry_count = (uint32_t)geometries.size();
			UPLOAD_TO_GPU(scene.geometries, geometries.data(), sizeof(GeometryData) * geometries.size());
		}

		bool instances_has_rebuild = false;
		if ((scene_change_flag & INSTANCE_MOVE) ||
			(scene_change_flag & INSTANCE_ADD) ||
			(scene_change_flag & INSTANCE_REMOVE))
		{
			ACBVHBuilder SceneBuilder(prim_instances.data(),
				(uint32_t)prim_instances.size(),
				transforms.data(),
				geometries.data());
			top_nodes.clear();
			SceneBuilder.BuildSceneBVH(top_nodes, &top_BVH_time, highlight_prim_idx);

			FREE_GPU_RESOURCE(scene.prim_instances);
			assert(scene.prim_instances == nullptr);
			scene.prim_instance_count = (uint32_t)prim_instances.size();
			UPLOAD_TO_GPU(scene.prim_instances, prim_instances.data(), sizeof(PrimitiveInstance) * prim_instances.size());

			FREE_GPU_RESOURCE(scene.top_nodes);
			assert(scene.top_nodes == nullptr);
			scene.top_node_count = (uint32_t)top_nodes.size();
			UPLOAD_TO_GPU(scene.top_nodes, top_nodes.data(), sizeof(TopNode) * top_nodes.size());

			instances_has_rebuild = true;
		}

		if ((scene_change_flag & TRANSFORM_ADD) ||
			(scene_change_flag & TRANSFORM_REMOVE))
		{
			FREE_GPU_RESOURCE(scene.transforms);
			assert(scene.transforms == nullptr);
			FREE_GPU_RESOURCE(scene.i_transforms);
			assert(scene.i_transforms == nullptr);

			scene.transform_count = (uint32_t)transforms.size();
			UPLOAD_TO_GPU(scene.transforms, transforms.data(), sizeof(glm::mat4) * transforms.size());
			UPLOAD_TO_GPU(scene.i_transforms, i_transforms.data(), sizeof(glm::mat4) * i_transforms.size());
		}

		if ((scene_change_flag & MATERIAL_ADD) ||
			(scene_change_flag & MATERIAL_REMOVE))
		{
			FREE_GPU_RESOURCE(scene.materials);
			assert(scene.materials == nullptr);
			scene.material_count = (uint32_t)materials.size();
			UPLOAD_TO_GPU(scene.materials, materials.data(), sizeof(Material) * materials.size());
		}

		if (scene_change_flag & DISTANT_LIGHT_ADD_REMOVE)
		{
			FREE_GPU_RESOURCE(scene.distant_lights);
			assert(scene.distant_lights == nullptr);

			scene.distant_light_count = (uint32_t)distant_lights.size();
			UPLOAD_TO_GPU(scene.distant_lights, distant_lights.data(), sizeof(DistantLight) * distant_lights.size());
		}

		if ((scene_change_flag & SHAPE_LIGHT_CHANGE) || instances_has_rebuild)
		{
			FREE_GPU_RESOURCE(scene.shape_lights);
			assert(scene.shape_lights == nullptr);
			FREE_GPU_RESOURCE(scene.shape_light_sample_table);
			assert(scene.shape_light_sample_table == nullptr);

			shape_lights.clear();
			shape_light_sample_table.clear();

			LoadShapeLights();

			scene.shape_light_count = (uint32_t)shape_lights.size();
			UPLOAD_TO_GPU(scene.shape_lights, shape_lights.data(), sizeof(ShapeLight) * shape_lights.size());
			UPLOAD_TO_GPU(scene.shape_light_sample_table, shape_light_sample_table.data(), sizeof(float) * shape_light_sample_table.size());
		}

		if (scene_change_flag & VOLUME_ADD_REMOVE)
		{
			FREE_GPU_RESOURCE(scene.volumes);
			assert(scene.volumes == nullptr);

			scene.volume_count = (uint32_t)volumes.size();
			UPLOAD_TO_GPU(scene.volumes, volumes.data(), sizeof(Volume) * volumes.size());
		}

		// reset scene flag
		scene_camera_change = IsCameraEqual(camera, cam) ? false : true;
		assert(scene.camera != nullptr);

		if (scene_camera_change || 
			(scene_change_flag & SCENECHANGE_FLAG::INSTANCE_MATERIAL_CHANGE) ||
			(scene_change_flag & SCENECHANGE_FLAG::MATERIAL_ASSIGN_TO_A_INSTANCE) ||
			(scene_change_flag & SCENECHANGE_FLAG::INSTANCE_EX_IOR_CHANGE) ||
			(scene_change_flag & SCENECHANGE_FLAG::INSTANCE_VOLUME_CHANGE))
		{
			camera = cam;
			camera.InitCameraRayTransfer(this->GetHostSceneData());
			CUDA_CHECK(cudaMemcpy(scene.camera, &camera, sizeof(Camera), cudaMemcpyHostToDevice));
		}

		CUDA_CHECK(cudaDeviceSynchronize());
		ResetSceneFlag();
	}
	inline void SceneModule::ClearInvaildTransform()
	{
		// batch update...
		// TODO: handle distantlight's clear
		if (prim_instances.size() + volumes.size() + distant_lights.size() >= transform_states.size() / 2) { return; }

		// compute their locations...
		// TODO: parallel prefix sum
		uint32_t valid_transform_count = 0;
		std::vector<uint32_t> locations(transform_states.size());
		for (uint32_t i = 0; i < (uint32_t)locations.size(); ++i)
		{
			locations[i] = valid_transform_count;
			valid_transform_count += transform_states[i].Invalid() ? 0 : 1;
		}

		// have to handle all the instance, cause their transform_idx should be changed...
		std::vector<PrimitiveInstance> &prims = prim_instances;
		concurrency::parallel_for(0u, (uint32_t)prim_instances.size(), [&](uint32_t i)
			{
				prims[i].transform_idx = locations[prims[i].transform_idx];
			});

		AddSceneFlag(SCENECHANGE_FLAG::INSTANCE_MOVE);

		// compact the transform data...
		std::vector<TransformState> &src_transform_states = transform_states;
		std::vector<TransformState> temp_transform_states(src_transform_states.size());

		std::vector<glm::mat4> &src_transforms = transforms;
		std::vector<glm::mat4> temp_transforms(src_transforms.size());

		std::vector<glm::mat4> &src_i_transforms = i_transforms;
		std::vector<glm::mat4> temp_i_transforms(src_i_transforms.size());

		concurrency::parallel_for(0u, (uint32_t)src_transform_states.size(), [&](uint32_t i)
			{
				// put the valid transform to new array
				if (!src_transform_states[i].Invalid())
				{
					temp_transform_states[locations[i]] = src_transform_states[i];
					temp_transforms[locations[i]] = src_transforms[i];
					temp_i_transforms[locations[i]] = src_i_transforms[i];
				}
			});

		// triger move to save time 
		std::swap(src_transform_states, temp_transform_states);
		std::swap(src_transforms, temp_transforms);
		std::swap(src_i_transforms, temp_i_transforms);

		src_transform_states.resize(valid_transform_count);
		src_transforms.resize(valid_transform_count);
		src_i_transforms.resize(valid_transform_count);

		for (uint32_t light_idx = 0; light_idx < (uint32_t)distant_lights.size(); ++light_idx)
		{
			if (distant_lights[light_idx].transform_idx != locations[distant_lights[light_idx].transform_idx])
			{
				distant_lights[light_idx].transform_idx = locations[distant_lights[light_idx].transform_idx];
				UpdateDistantLightTransform(light_idx);
			}
		}

		AddSceneFlag(SCENECHANGE_FLAG::TRANSFORM_REMOVE);
	}
};
