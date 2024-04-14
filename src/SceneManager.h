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

	class SceneManager
	{
	public:
		static inline std::shared_ptr<SceneManager> GetInstance()
		{
			static std::shared_ptr<SceneManager> ptr(new SceneManager());
			return ptr;
		}

		inline SceneManager(const SceneManager&) = delete;
		inline SceneManager(const SceneManager&&) = delete;
		inline SceneManager& operator=(const SceneManager&) = delete;
		inline ~SceneManager() {} // the clear work is handled by MainSystem!
		
		void DestroyResources();
		void Init();
		void UploadSceneSetting();
		void UpdateScene(const Camera &cam, uint32_t *highlight_prim_idx);
		void ClearInvaildTransform();

		inline uint32_t AddGeometry(const std::string &name, const GeometryData& geometry);
		inline uint32_t RemoveGeometry(uint32_t geometry_idx);

		// this function only record the transform state: T, S, R
		inline uint32_t AddTransform(const glm::vec3 &T = glm::vec3(0.0f), const glm::vec3 &S = glm::vec3(1.0f), const glm::vec3 &R = glm::vec3(0.0f));
		inline void UpdatePrimTransform(uint32_t selected_prim_idx);
		inline void UpdateDistantLightTransform(uint32_t selected_light_idx);
		inline void UpdateVolumeTransform(uint32_t selected_volume_idx);
		
		// here actually generate the transform matrix, because I need the geometry info
		inline uint32_t AddPrimInstance(uint32_t geometry_idx, uint32_t transform_idx, uint32_t material_idx, float external_ior = 1.0f, int outer_vol_idx = -1, int inner_vol_idx = -1, bool is_volume_boundary = false);
		inline uint32_t RemovePrimInstance(uint32_t selected_prim_idx);
		inline void ChangePrimVolumeAttribute(uint32_t prim_idx, const int *outer_vol_idx, const int *inner_vol_idx, const float *outer_ior);

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
																 uint32_t diff_tex_idx = INVALID_UINT_32);
		inline uint32_t RemoveMaterial(uint32_t material_idx);
		inline void UpdateMaterial(uint32_t material_idx);
		inline void AssignMaterialToPrim(uint32_t prim_idx, uint32_t material_idx); // you should use it when only the material change...

		// distant light
		inline uint32_t AddDistantLight(const std::string &name, 
														   uint32_t transform_idx, 
														   const glm::vec3 &light_color = glm::vec3(1.0f), 
														   float intensity = 1.0f, 
														   float theta_max = 5.0f);
		inline uint32_t RemoveDistantLight(uint32_t light_idx);
		inline void UpdateDistantLight(uint32_t distant_light_idx);

		// shape light
		inline void LoadShapeLights();

		// volume
		inline uint32_t AddVolume(const std::string &name, uint32_t transform_idx, 
			const glm::vec3 &sigma_s = glm::vec3(0.0f), 
			const glm::vec3 &sigma_a = glm::vec3(0.0f), float g = 0.0f, int density_texture_idx = -1);
		inline uint32_t RemoveVolume(uint32_t volume_idx);
		inline void UpdateVolume(uint32_t volume_idx);

		// texture
		inline uint32_t AddTexture(const std::string &name, const Texture& texture);
		inline uint32_t RemoveTexture(uint32_t texture_idx);
		inline void UpdateTexture(uint32_t texture_idx);

		// a bunch of short function
		inline Camera& GetCameraRef() { return camera; }
		inline const Scene& GetScene() const { return scene; }
		inline const TextureManager& GetTextureManager() const { return texture_manager; }
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
		TextureManager texture_manager;

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

		SceneManager() {}
		
		void InitHaltonPermuteTable();
		void InitSobolMatrices();
	};

	inline uint32_t SceneManager::AddGeometry(const std::string &name, const GeometryData& geometry)
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
	inline uint32_t SceneManager::RemoveGeometry(uint32_t geometry_idx)
	{
		if (geometries.empty()) { return INVALID_UINT_32; }
		if (geometry_idx >(uint32_t)geometries.size() - 1) { return INVALID_UINT_32; }

		// first clear all the instance
		bool instances_are_change = false;
		uint32_t head = 0, tail = (uint32_t)prim_instances.size() - 1;
		for (; head <= tail && tail != INVALID_UINT_32; )
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

		GeometryData &geometry_to_deleted = geometries[geometry_idx];
		geometry_to_deleted.Destory();

		// remove geometry
		geometries.erase(geometries.begin() + geometry_idx);
		geometry_reference_counters.erase(geometry_reference_counters.begin() + geometry_idx);
		geometry_names.erase(geometry_names.begin() + geometry_idx);

		AddSceneFlag(SCENECHANGE_FLAG::GEOMETRY_REMOVE);

		if (geometries.empty()) { return INVALID_UINT_32; }
		// give the first element back
		return 0;
	}

	inline uint32_t SceneManager::AddTransform(const glm::vec3 &T, const glm::vec3 &S, const glm::vec3 &R)
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
	inline void SceneManager::UpdatePrimTransform(uint32_t prim_idx)
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
	inline void SceneManager::UpdateDistantLightTransform(uint32_t selected_light_idx)
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
	inline void SceneManager::UpdateVolumeTransform(uint32_t selected_volume_idx)
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

	inline uint32_t SceneManager::AddPrimInstance(uint32_t geometry_idx, uint32_t transform_idx, uint32_t material_idx, float external_ior, int outer_vol_idx, int inner_vol_idx, bool is_volume_boundary)
	{
		assert(geometry_idx >= 0 && geometry_idx < geometries.size());
		assert(material_idx >= 0 && material_idx < materials.size());
		prim_instances.push_back(PrimitiveInstance(geometry_idx, transform_idx, material_idx, external_ior, outer_vol_idx, inner_vol_idx, is_volume_boundary));
		
		geometry_reference_counters[geometry_idx]++;
		material_reference_counters[material_idx]++;

		const BBox3 object_bbox = GetGeometryBound(geometries[geometry_idx]);
		const glm::vec3 bbox_center = BBox3Center(object_bbox);

		transforms[transform_idx] = transform_states[transform_idx].GetTransformMatrix(bbox_center);
		i_transforms[transform_idx] = glm::inverse(transforms[transform_idx]);

		AddSceneFlag(SCENECHANGE_FLAG::INSTANCE_ADD);
		return (uint32_t)prim_instances.size() - 1;
	}
	inline uint32_t SceneManager::RemovePrimInstance(uint32_t selected_prim_idx)
	{
		if (prim_instances.empty()) { return INVALID_UINT_32; }
		if (selected_prim_idx >(uint32_t)prim_instances.size() - 1) { return INVALID_UINT_32; }

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

		if (prim_instances.empty()) { return INVALID_UINT_32; }

		// give the first element back
		return 0;
	}
	inline void SceneManager::ChangePrimVolumeAttribute(uint32_t prim_idx, const int *outer_vol_idx, const int *inner_vol_idx, const float *outer_ior)
	{
		if (prim_instances.empty() || prim_idx > (uint32_t)prim_instances.size() - 1) { return; }

		if (outer_vol_idx != nullptr) { prim_instances[prim_idx].outer_volume_idx = *outer_vol_idx; }
		if (inner_vol_idx != nullptr) { prim_instances[prim_idx].inner_volume_idx = *inner_vol_idx; }
		if (outer_ior != nullptr) { prim_instances[prim_idx].external_ior = *outer_ior; }

		assert(scene.prim_instances != nullptr);
		CUDA_CHECK(cudaMemcpy(scene.prim_instances + prim_idx, &prim_instances[prim_idx], sizeof(PrimitiveInstance), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaDeviceSynchronize());

		AddSceneFlag(SCENECHANGE_FLAG::INSTANCE_VOLUME_CHANGE);
	}

	// TODO: Material add, remove ,edit...
	inline uint32_t SceneManager::AddLightMaterial(const std::string &name, const glm::vec3 &light_color, float intensity)
	{
		materials.push_back(Material().InitLightMtl(light_color, intensity));
		material_names.push_back(name);
		material_reference_counters.push_back(0);

		AddSceneFlag(SCENECHANGE_FLAG::MATERIAL_ADD);
		return (uint32_t)materials.size() - 1;
	}
	inline uint32_t SceneManager::AddSurfaceMaterial(const std::string &name,
																				   const glm::vec3 &diffuse_albedo,
																				   const glm::vec3 &specular_albedo,
																				   float roughness_x,
																				   float roughness_y,
																				   float ior_n, 
																				   float metalness,
																				   float specular_weight,
																				   float transmission_weight, 
																				   uint32_t diff_tex_idx)
	{	
		materials.push_back(Material().InitDefaultMtl(diffuse_albedo, specular_albedo, roughness_x, roughness_y, ior_n, metalness, specular_weight, transmission_weight, diff_tex_idx));
		material_names.push_back(name);
		material_reference_counters.push_back(0);
	
		AddSceneFlag(SCENECHANGE_FLAG::MATERIAL_ADD);
		return (uint32_t)materials.size() - 1;
	}
	inline uint32_t SceneManager::RemoveMaterial(uint32_t material_idx)
	{
		if (materials.empty()) { return INVALID_UINT_32; }
		if (material_idx < 0 || material_idx >(uint32_t)materials.size() - 1) { return INVALID_UINT_32; }

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

		return materials.empty()? INVALID_UINT_32: 0;
	}
	inline void SceneManager::UpdateMaterial(uint32_t material_idx)
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
	inline void SceneManager::AssignMaterialToPrim(uint32_t prim_idx, uint32_t material_idx)
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
	inline uint32_t SceneManager::AddDistantLight(const std::string &name, uint32_t transform_idx, 
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
	inline uint32_t SceneManager::RemoveDistantLight(uint32_t light_idx)
	{
		if (distant_lights.empty()) { return INVALID_UINT_32; }
		if (light_idx >(uint32_t)distant_lights.size() - 1) { return INVALID_UINT_32; }

		transform_states[distant_lights[light_idx].transform_idx].SetInvalid();

		// TODO: set the transform to invalid
		distant_lights.erase(distant_lights.begin() + light_idx);
		distant_light_names.erase(distant_light_names.begin() + light_idx);
		
		AddSceneFlag(SCENECHANGE_FLAG::DISTANT_LIGHT_ADD_REMOVE);

		return distant_lights.empty() ? INVALID_UINT_32 : 0;
	}
	inline void SceneManager::UpdateDistantLight(uint32_t distant_light_idx)
	{
		if (distant_lights.empty()) { return; }
		if (distant_light_idx >(uint32_t)distant_lights.size() - 1) { return; }

		assert(scene.distant_lights != nullptr);
		CUDA_CHECK(cudaMemcpy(scene.distant_lights + distant_light_idx, &distant_lights[distant_light_idx], sizeof(DistantLight), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaDeviceSynchronize());

		AddSceneFlag(SCENECHANGE_FLAG::DISTANT_LIGHT_CHANGE);
	}
	
	inline void SceneManager::LoadShapeLights()
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
				shape_light.triangle_idx = INVALID_UINT_32;
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
	inline uint32_t SceneManager::AddVolume(const std::string &name, uint32_t transform_idx,
		const glm::vec3 &sigma_s, const glm::vec3 &sigma_a, float g, int density_texture_idx)
	{
		Volume volume;
		volume.sigma_a = sigma_a;
		volume.sigma_s = sigma_s;
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
	inline uint32_t SceneManager::RemoveVolume(uint32_t volume_idx)
	{
		if (volumes.empty()) { return INVALID_UINT_32; }
		if (volume_idx > (uint32_t)volumes.size() - 1) { return INVALID_UINT_32; }

		transform_states[volumes[volume_idx].transform_idx].SetInvalid();

		// just leave user to manually set...
		volumes.erase(volumes.begin() + volume_idx);
		volume_names.erase(volume_names.begin() + volume_idx);
		
		AddSceneFlag(VOLUME_ADD_REMOVE);
		CUDA_CHECK(cudaDeviceSynchronize());

		return volumes.empty() ? INVALID_UINT_32 : 0;
	}
	inline void SceneManager::UpdateVolume(uint32_t volume_idx)
	{
		if (volumes.empty()) { return; }
		if (volume_idx > (uint32_t)volumes.size() - 1) { return; }

		assert(scene.volumes != nullptr);
		CUDA_CHECK(cudaMemcpy(scene.volumes + volume_idx, &volumes[volume_idx], sizeof(Volume), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaDeviceSynchronize());

		AddSceneFlag(SCENECHANGE_FLAG::VOLUME_CHANGE);
	}

	// TODO: texture GUI
	inline uint32_t SceneManager::AddTexture(const std::string &name, const Texture& texture)
	{
		textures.push_back(texture);
		texture_names.push_back(name);

		AddSceneFlag(SCENECHANGE_FLAG::TEXTURE_ADD_REMOVE);
		return (uint32_t)textures.size() - 1;
	}
	inline void SceneManager::UpdateTexture(uint32_t texture_idx)
	{
		if (textures.empty()) { return; }
		if (texture_idx > (uint32_t)textures.size() - 1) { return; }

		assert(scene.textures != nullptr);
		CUDA_CHECK(cudaMemcpy(scene.textures + texture_idx, &textures[texture_idx], sizeof(Texture), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaDeviceSynchronize());

		AddSceneFlag(SCENECHANGE_FLAG::TEXTURE_CHANGE);
	}
};