#pragma once

#include <vector>
#include <memory>
#include <iostream>
#include <random>
#include <string>

#include "SceneDefines.h"
#include "Camera.h"
#include "GeometryDefines.h"
#include "GeometryUtilities.h"
#include "BottomBVH.h"
#include "PrimtiveInstance.h"
#include "Material.h"
#include "Light.h"
#include "Volume.h"

namespace YumeRT
{
	struct TransformState
	{
		glm::vec3 T;
		glm::vec3 S;
		glm::vec3 R; // should turn into radians

		TransformState() { SetInvalid(); }

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
		INSTANCE_VOLUME_CHANGE = (1 << 15)
	};

	class SceneManager
	{
	public:
		static std::shared_ptr<SceneManager> GetInstance()
		{
			static std::shared_ptr<SceneManager> ptr(new SceneManager());
			return ptr;
		}

		~SceneManager()
		{
			// the clear work is handled by MainSystem!
		}
		
		void TestScene(int screen_width, int screen_height);
		void CornellBox(int screen_width, int screen_height);

		void DestroyResources();

		void InitScene(int screen_width, int screen_height);
		void UpdateScene(const Camera &cam, uint32_t *highlight_prim_idx);

		inline uint32_t AddQube(const std::string &name);
		inline uint32_t AddSphere(const std::string &name, float radius);
		inline uint32_t RemoveGeometry(uint32_t geometry_idx);

		// this function only record the transform state: T, S, R
		inline uint32_t AddTransform(const glm::vec3 &T = glm::vec3(0.0f), const glm::vec3 &S = glm::vec3(1.0f), const glm::vec3 &R = glm::vec3(0.0f));
		inline void UpdatePrimTransform(uint32_t selected_prim_idx);
		void ClearInvaildTransform();
		 
		// here actually generate the transform matrix, because I need the geometry info
		inline uint32_t AddPrimInstance(uint32_t geometry_idx, uint32_t transform_idx, uint32_t material_idx, float external_ior = 1.0f, int outer_vol_idx = -1, int inner_vol_idx = -1);
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
																 float transmission_weight = 0.0f);
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
		inline void UpdateDistantLightTransform(uint32_t selected_light_idx);

		// shape light
		inline void LoadShapeLights();

		// volume
		inline uint32_t AddVolume(const std::string &name, const glm::vec3 &sigma_s, const glm::vec3 &sigma_a);

		// a bunch of short function
		inline const Scene& GetScene() const { return scene; }
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
		inline const std::vector<Volume>& GetVolumes()const { return volumes; }

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

		inline PrimitiveInstance* GetPrimitiveInstance(uint32_t idx)
		{
			if (prim_instances.empty()) { return nullptr; }
			if (idx >(uint32_t)prim_instances.size() - 1) { return nullptr; }
			return &prim_instances[idx];
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
				(scene_change_flag & SCENECHANGE_FLAG::INSTANCE_VOLUME_CHANGE);
		}
		inline void AddSceneFlag(SCENECHANGE_FLAG flag) { scene_change_flag |= flag; }
		inline void ResetSceneFlag() { scene_change_flag = 0; }

	private:
		bool scene_camera_change = false;
		uint32_t scene_change_flag = 0;

		float bottom_BVH_time = 0.0f;
		float top_BVH_time = 0.0f;

		Scene scene;
		Camera camera;
		std::vector<TransformState> transform_states;
		std::vector<glm::mat4> transforms;
		std::vector<glm::mat4> i_transforms;

		std::vector<PrimitiveInstance> prim_instances;

		std::vector<GeometryData> geometries;
		std::vector<uint32_t> geometry_reference_counters;
		std::vector<std::string> geometry_names;

		std::vector<Triangle> triangles;
		std::vector<uint32_t> vidxs;
		std::vector<glm::vec3> positions;
		std::vector<uint32_t> nidxs;
		std::vector<glm::vec3> normals;
		std::vector<uint32_t> uvidxs;
		std::vector<glm::vec2> texcoords;

		std::vector<BottomNode> bottom_nodes;
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

		SceneManager() {}
		
	};

	void SceneManager::DestroyResources()
	{
		FREE_GPU_RESOURCE(scene.camera);

		// mesh data
		FREE_GPU_RESOURCE(scene.vidxs);
		FREE_GPU_RESOURCE(scene.positions);
		FREE_GPU_RESOURCE(scene.nidxs);
		FREE_GPU_RESOURCE(scene.normals);
		FREE_GPU_RESOURCE(scene.uvidxs);
		FREE_GPU_RESOURCE(scene.texcoords);
		FREE_GPU_RESOURCE(scene.triangles);

		scene.geometry_count = 0;
		FREE_GPU_RESOURCE(scene.geometries);

		scene.bottom_node_count = 0;
		FREE_GPU_RESOURCE(scene.bottom_nodes);

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

		assert(scene.camera == nullptr);
		assert(scene.vidxs == nullptr);
		assert(scene.positions == nullptr);
		assert(scene.nidxs == nullptr);
		assert(scene.normals == nullptr);
		assert(scene.uvidxs == nullptr);
		assert(scene.texcoords == nullptr);
		assert(scene.triangles == nullptr);
		assert(scene.geometries == nullptr);
		assert(scene.prim_instances == nullptr);
		assert(scene.bottom_nodes == nullptr);
		assert(scene.top_nodes == nullptr);
		assert(scene.transforms == nullptr);
		assert(scene.i_transforms == nullptr);
		assert(scene.materials == nullptr);
		assert(scene.distant_lights == nullptr);
		assert(scene.shape_lights == nullptr);
		assert(scene.shape_light_sample_table == nullptr);
		assert(scene.volumes == nullptr);
	}

	void SceneManager::InitScene(int screen_width, int screen_height)
	{
		CornellBox(screen_width, screen_height);

		UPLOAD_TO_GPU(scene.camera, &camera, sizeof(Camera));

		UPLOAD_TO_GPU(scene.vidxs, vidxs.data(), sizeof(uint32_t) * vidxs.size());
		UPLOAD_TO_GPU(scene.positions, positions.data(), sizeof(glm::vec3) * positions.size());
		UPLOAD_TO_GPU(scene.nidxs, nidxs.data(), sizeof(uint32_t) * nidxs.size());
		UPLOAD_TO_GPU(scene.normals, normals.data(), sizeof(glm::vec3) * normals.size());
		UPLOAD_TO_GPU(scene.uvidxs, uvidxs.data(), sizeof(uint32_t) * uvidxs.size());
		UPLOAD_TO_GPU(scene.texcoords, texcoords.data(), sizeof(glm::vec2) * texcoords.size());
		UPLOAD_TO_GPU(scene.triangles, triangles.data(), sizeof(Triangle) * triangles.size());

		scene.geometry_count = (uint32_t)geometries.size();
		UPLOAD_TO_GPU(scene.geometries, geometries.data(), sizeof(GeometryData) * geometries.size());

		scene.bottom_node_count = (uint32_t)bottom_nodes.size();
		UPLOAD_TO_GPU(scene.bottom_nodes, bottom_nodes.data(), sizeof(BottomNode) * bottom_nodes.size());

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

		ResetSceneFlag();
	}
	void SceneManager::UpdateScene(const Camera &cam, uint32_t *highlight_prim_idx)
	{
		// reset scene flag
		scene_camera_change = IsCameraEqual(camera, cam) ? false : true;
		assert(scene.camera != nullptr);

		if (scene_camera_change)
		{
			camera = cam;
			CUDA_CHECK(cudaMemcpy(scene.camera, &camera, sizeof(Camera), cudaMemcpyHostToDevice));
		}

		// handle transform compact
		ClearInvaildTransform();

		if ((scene_change_flag & GEOMETRY_ADD) ||
			(scene_change_flag & GEOMETRY_REMOVE))
		{
			FREE_GPU_RESOURCE(scene.vidxs);
			UPLOAD_TO_GPU(scene.vidxs, vidxs.data(), sizeof(uint32_t) * vidxs.size());

			FREE_GPU_RESOURCE(scene.positions);
			UPLOAD_TO_GPU(scene.positions, positions.data(), sizeof(glm::vec3) * positions.size());

			FREE_GPU_RESOURCE(scene.nidxs);
			UPLOAD_TO_GPU(scene.nidxs, nidxs.data(), sizeof(uint32_t) * nidxs.size());

			FREE_GPU_RESOURCE(scene.normals);
			UPLOAD_TO_GPU(scene.normals, normals.data(), sizeof(glm::vec3) * normals.size());

			FREE_GPU_RESOURCE(scene.uvidxs);
			UPLOAD_TO_GPU(scene.uvidxs, uvidxs.data(), sizeof(uint32_t) * uvidxs.size());

			FREE_GPU_RESOURCE(scene.texcoords);
			UPLOAD_TO_GPU(scene.texcoords, texcoords.data(), sizeof(glm::vec2) * texcoords.size());

			FREE_GPU_RESOURCE(scene.triangles);
			UPLOAD_TO_GPU(scene.triangles, triangles.data(), sizeof(Triangle) * triangles.size());

			FREE_GPU_RESOURCE(scene.geometries);
			scene.geometry_count = (uint32_t)geometries.size();
			UPLOAD_TO_GPU(scene.geometries, geometries.data(), sizeof(GeometryData) * geometries.size());

			FREE_GPU_RESOURCE(scene.bottom_nodes);
			scene.bottom_node_count = (uint32_t)bottom_nodes.size();
			UPLOAD_TO_GPU(scene.bottom_nodes, bottom_nodes.data(), sizeof(BottomNode) * bottom_nodes.size());
		}

		bool instances_has_rebuild = false;
		if ((scene_change_flag & INSTANCE_MOVE) ||
			(scene_change_flag & INSTANCE_ADD) ||
			(scene_change_flag & INSTANCE_REMOVE))
		{
			ACBVHBuilder SceneBuilder(prim_instances.data(),
				(uint32_t)prim_instances.size(),
				transforms.data(),
				geometries.data(),
				bottom_nodes.data());
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

		CUDA_CHECK(cudaDeviceSynchronize());

		ResetSceneFlag();
	}
	
	inline uint32_t SceneManager::AddQube(const std::string &name)
	{
		GeometryData cube;
		cube.geometry_type = GEOMETRY_TYPE::TRIANGLE_MESH;

		cube.tri_mesh.idx_count = 36;
		cube.tri_mesh.vidx_offset = (uint32_t)vidxs.size();
		cube.tri_mesh.position_offset = (uint32_t)positions.size();
		cube.tri_mesh.nidx_offset = (uint32_t)nidxs.size();
		cube.tri_mesh.normal_offset = (uint32_t)normals.size();
		cube.tri_mesh.uvidx_offset = (uint32_t)uvidxs.size();
		cube.tri_mesh.texcoord_offset = (uint32_t)texcoords.size();

		cube.tri_mesh.position_count = 36;
		cube.tri_mesh.normal_count = 36;
		cube.tri_mesh.texcoord_count = 36;

		cube.tri_mesh.triangle_count = 12;
		cube.tri_mesh.triangle_offset = (uint32_t)triangles.size();

		// add a shape
		geometries.push_back(cube);
		// add its counter to 0
		geometry_reference_counters.push_back(0);
		// add its name, now only for display
		geometry_names.push_back(name);

		std::vector<uint32_t> cube_idxs = {
		0, 1, 2,
		3, 4, 5,
		6, 7, 8,
		9, 10, 11,
		12, 13, 14,
		15, 16, 17,
		18, 19, 20,
		21, 22, 23,
		24, 25, 26,
		27, 28, 29,
		30, 31, 32,
		33, 34, 35,
		};

		uint32_t pre_triangles_size = triangles.size();
		triangles.resize(pre_triangles_size + 12);
		for (uint32_t i = 0, tri_id = 0; i < cube_idxs.size(); i += 3, ++tri_id)
		{
			(triangles.data() + pre_triangles_size)[tri_id].id0 = i;
			(triangles.data() + pre_triangles_size)[tri_id].id1 = i + 1;
			(triangles.data() + pre_triangles_size)[tri_id].id2 = i + 2;
		}

		std::vector<float> cube_positions = {
		-0.5f, -0.5f, -0.5f,
		0.5f,  0.5f, -0.5f,
		0.5f, -0.5f, -0.5f,
		0.5f,  0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,
		-0.5f,  0.5f, -0.5f,

		-0.5f, -0.5f,  0.5f,
		0.5f, -0.5f,  0.5f,
		0.5f,  0.5f,  0.5f,
		0.5f,  0.5f,  0.5f,
		-0.5f,  0.5f,  0.5f,
		-0.5f, -0.5f,  0.5f,

		-0.5f,  0.5f,  0.5f,
		-0.5f,  0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f,  0.5f,
		-0.5f,  0.5f,  0.5f,

		0.5f,  0.5f,  0.5f,
		0.5f, -0.5f, -0.5f,
		0.5f,  0.5f, -0.5f,
		0.5f, -0.5f, -0.5f,
		0.5f,  0.5f,  0.5f,
		0.5f, -0.5f,  0.5f,

		-0.5f, -0.5f, -0.5f,
		0.5f, -0.5f, -0.5f,
		0.5f, -0.5f,  0.5f,
		0.5f, -0.5f,  0.5f,
		-0.5f, -0.5f,  0.5f,
		-0.5f, -0.5f, -0.5f,

		-0.5f,  0.5f, -0.5f,
		0.5f,  0.5f,  0.5f,
		0.5f,  0.5f, -0.5f,
		0.5f,  0.5f,  0.5f,
		-0.5f,  0.5f, -0.5f,
		-0.5f,  0.5f,  0.5f,
		};
		std::vector<float> cube_normals = {
		0.0f,  0.0f, -1.0f,
		0.0f,  0.0f, -1.0f,
		0.0f,  0.0f, -1.0f,
		0.0f,  0.0f, -1.0f,
		0.0f,  0.0f, -1.0f,
		0.0f,  0.0f, -1.0f,

		0.0f,  0.0f,  1.0f,
		0.0f,  0.0f,  1.0f,
		0.0f,  0.0f,  1.0f,
		0.0f,  0.0f,  1.0f,
		0.0f,  0.0f,  1.0f,
		0.0f,  0.0f,  1.0f,

		-1.0f,  0.0f,  0.0f,
		-1.0f,  0.0f,  0.0f,
		-1.0f,  0.0f,  0.0f,
		-1.0f,  0.0f,  0.0f,
		-1.0f,  0.0f,  0.0f,
		-1.0f,  0.0f,  0.0f,

		1.0f,  0.0f,  0.0f,
		1.0f,  0.0f,  0.0f,
		1.0f,  0.0f,  0.0f,
		1.0f,  0.0f,  0.0f,
		1.0f,  0.0f,  0.0f,
		1.0f,  0.0f,  0.0f,

		0.0f, -1.0f,  0.0f,
		0.0f, -1.0f,  0.0f,
		0.0f, -1.0f,  0.0f,
		0.0f, -1.0f,  0.0f,
		0.0f, -1.0f,  0.0f,
		0.0f, -1.0f,  0.0f,

		0.0f,  1.0f,  0.0f,
		0.0f,  1.0f,  0.0f,
		0.0f,  1.0f,  0.0f,
		0.0f,  1.0f,  0.0f,
		0.0f,  1.0f,  0.0f,
		0.0f,  1.0f,  0.0f,
		};
		std::vector<float> cube_texcoords = {
		0.0f,  0.0f,
		1.0f,  1.0f,
		1.0f,  0.0f,
		1.0f,  1.0f,
		0.0f,  0.0f,
		0.0f,  1.0f,

		0.0f,  0.0f,
		1.0f,  0.0f,
		1.0f,  1.0f,
		1.0f,  1.0f,
		0.0f,  1.0f,
		0.0f,  0.0f,

		1.0f,  0.0f,
		1.0f,  1.0f,
		0.0f,  1.0f,
		0.0f,  1.0f,
		0.0f,  0.0f,
		1.0f,  0.0f,

		1.0f,  0.0f,
		0.0f,  1.0f,
		1.0f,  1.0f,
		0.0f,  1.0f,
		1.0f,  0.0f,
		0.0f,  0.0f,

		0.0f,  1.0f,
		1.0f,  1.0f,
		1.0f,  0.0f,
		1.0f,  0.0f,
		0.0f,  0.0f,
		0.0f,  1.0f,

		0.0f,  1.0f,
		1.0f,  0.0f,
		1.0f,  1.0f,
		1.0f,  0.0f,
		0.0f,  1.0f,
		0.0f,  0.0f
		};

		uint32_t pre_vidxs_size = vidxs.size();
		vidxs.resize(pre_vidxs_size + 36);
		memcpy(vidxs.data() + pre_vidxs_size, cube_idxs.data(), sizeof(uint32_t) * 36);

		uint32_t pre_nidxs_size = nidxs.size();
		nidxs.resize(pre_nidxs_size + 36);
		memcpy(nidxs.data() + pre_nidxs_size, cube_idxs.data(), sizeof(uint32_t) * 36);

		uint32_t pre_uvidxs_size = uvidxs.size();
		uvidxs.resize(pre_uvidxs_size + 36);
		memcpy(uvidxs.data() + pre_uvidxs_size, cube_idxs.data(), sizeof(uint32_t) * 36);

		uint32_t pre_positions_size = positions.size();
		positions.resize(pre_positions_size + 36);
		memcpy(positions.data() + pre_positions_size, cube_positions.data(), sizeof(glm::vec3) * 36);

		uint32_t pre_normals_size = normals.size();
		normals.resize(pre_normals_size + 36);
		memcpy(normals.data() + pre_normals_size, cube_normals.data(), sizeof(glm::vec3) * 36);

		uint32_t pre_texcoords_size = texcoords.size();
		texcoords.resize(pre_texcoords_size + 36);
		memcpy(texcoords.data() + pre_texcoords_size, cube_texcoords.data(), sizeof(glm::vec2) * 36);

		GeometryData &cube_data = geometries[geometries.size() - 1];
		BottomBVHBuilder MeshBuilder(&cube_data,
			triangles.data() + cube_data.tri_mesh.triangle_offset,
			cube_data.tri_mesh.triangle_count,
			positions.data() + cube_data.tri_mesh.position_offset,
			vidxs.data() + cube_data.tri_mesh.vidx_offset);
		MeshBuilder.BuildMeshBVH(bottom_nodes, &bottom_BVH_time);

		AddSceneFlag(SCENECHANGE_FLAG::GEOMETRY_ADD);
		return (uint32_t)geometries.size() - 1;
	}
	inline uint32_t SceneManager::AddSphere(const std::string &name, float radius)
	{
		GeometryData sphere;
		sphere.geometry_type = GEOMETRY_TYPE::SPHERE;
		sphere.sphere.radius = radius;

		// add sphere
		geometries.push_back(sphere);
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

		if (instances_are_change) 
		{ 
			AddSceneFlag(SCENECHANGE_FLAG::INSTANCE_REMOVE);
		}

		const GeometryData &geometry_to_deleted = geometries[geometry_idx];
		if (geometry_to_deleted.geometry_type == TRIANGLE_MESH)
		{
			for (auto& geometry : geometries)
			{
				if (geometry.geometry_type == TRIANGLE_MESH)
				{
					if (geometry.tri_mesh.triangle_offset > geometry_to_deleted.tri_mesh.triangle_offset)
					{
						geometry.tri_mesh.triangle_offset -= geometry_to_deleted.tri_mesh.triangle_count;
					}
					if (geometry.tri_mesh.bottom_node_offset > geometry_to_deleted.tri_mesh.bottom_node_offset)
					{
						geometry.tri_mesh.bottom_node_offset -= geometry_to_deleted.tri_mesh.bottom_node_count;
					}
					if (geometry.tri_mesh.vidx_offset > geometry_to_deleted.tri_mesh.vidx_offset)
					{
						geometry.tri_mesh.vidx_offset -= geometry_to_deleted.tri_mesh.idx_count;
					}
					if (geometry.tri_mesh.position_offset > geometry_to_deleted.tri_mesh.position_offset)
					{
						geometry.tri_mesh.position_offset -= geometry_to_deleted.tri_mesh.position_count;
					}
					if (geometry.tri_mesh.nidx_offset > geometry_to_deleted.tri_mesh.nidx_offset)
					{
						geometry.tri_mesh.nidx_offset -= geometry_to_deleted.tri_mesh.idx_count;
					}
					if (geometry.tri_mesh.normal_offset > geometry_to_deleted.tri_mesh.normal_offset)
					{
						geometry.tri_mesh.normal_offset -= geometry_to_deleted.tri_mesh.normal_count;
					}
					if (geometry.tri_mesh.uvidx_offset > geometry_to_deleted.tri_mesh.uvidx_offset)
					{
						geometry.tri_mesh.uvidx_offset -= geometry_to_deleted.tri_mesh.idx_count;
					}
					if (geometry.tri_mesh.texcoord_offset > geometry_to_deleted.tri_mesh.texcoord_offset)
					{
						geometry.tri_mesh.texcoord_offset -= geometry_to_deleted.tri_mesh.texcoord_count;
					}
				}
			}

			triangles.erase(triangles.begin() + geometry_to_deleted.tri_mesh.triangle_offset, triangles.begin() + geometry_to_deleted.tri_mesh.triangle_offset + geometry_to_deleted.tri_mesh.triangle_count);

			bottom_nodes.erase(bottom_nodes.begin() + geometry_to_deleted.tri_mesh.bottom_node_offset, bottom_nodes.begin() + geometry_to_deleted.tri_mesh.bottom_node_offset + geometry_to_deleted.tri_mesh.bottom_node_count);

			vidxs.erase(vidxs.begin() + geometry_to_deleted.tri_mesh.vidx_offset, vidxs.begin() + geometry_to_deleted.tri_mesh.vidx_offset + geometry_to_deleted.tri_mesh.idx_count);

			positions.erase(positions.begin() + geometry_to_deleted.tri_mesh.position_offset, positions.begin() + geometry_to_deleted.tri_mesh.position_offset + geometry_to_deleted.tri_mesh.position_count);

			nidxs.erase(nidxs.begin() + geometry_to_deleted.tri_mesh.nidx_offset, nidxs.begin() + geometry_to_deleted.tri_mesh.nidx_offset + geometry_to_deleted.tri_mesh.idx_count);

			normals.erase(normals.begin() + geometry_to_deleted.tri_mesh.normal_offset, normals.begin() + geometry_to_deleted.tri_mesh.normal_offset + geometry_to_deleted.tri_mesh.normal_count);

			uvidxs.erase(uvidxs.begin() + geometry_to_deleted.tri_mesh.uvidx_offset, uvidxs.begin() + geometry_to_deleted.tri_mesh.uvidx_offset + geometry_to_deleted.tri_mesh.idx_count);

			texcoords.erase(texcoords.begin() + geometry_to_deleted.tri_mesh.texcoord_offset, texcoords.begin() + geometry_to_deleted.tri_mesh.texcoord_offset + geometry_to_deleted.tri_mesh.texcoord_count);
		}

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

		const BBox3 object_bbox = GetGeometryBound(geometries[geometry_idx], bottom_nodes.data());
		const glm::vec3 bbox_center = BBox3Center(object_bbox);

		transforms[transform_idx] = transform_states[transform_idx].GetTransformMatrix(bbox_center);
		i_transforms[transform_idx] = glm::inverse(transforms[transform_idx]);

		assert(scene.transforms != nullptr && scene.i_transforms != nullptr);
		CUDA_CHECK(cudaMemcpy(scene.transforms + transform_idx, &transforms[transform_idx], sizeof(glm::mat4), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaMemcpy(scene.i_transforms + transform_idx, &i_transforms[transform_idx], sizeof(glm::mat4), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaDeviceSynchronize());

		AddSceneFlag(SCENECHANGE_FLAG::INSTANCE_MOVE);
	}
	void SceneManager::ClearInvaildTransform()
	{
		// batch update...
		// TODO: handle distantlight's clear
		if (prim_instances.size() >= transform_states.size() / 2) { return; }

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

	inline uint32_t SceneManager::AddPrimInstance(uint32_t geometry_idx, uint32_t transform_idx, uint32_t material_idx, float external_ior, int outer_vol_idx, int inner_vol_idx)
	{
		assert(geometry_idx >= 0 && geometry_idx < geometries.size());
		assert(material_idx >= 0 && material_idx < materials.size());
		prim_instances.push_back(PrimitiveInstance(geometry_idx, transform_idx, material_idx, external_ior, outer_vol_idx, inner_vol_idx));
		
		geometry_reference_counters[geometry_idx]++;
		material_reference_counters[material_idx]++;

		const BBox3 object_bbox = GetGeometryBound(geometries[geometry_idx], bottom_nodes.data());
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
		Material mtl;
		mtl.material_type = LIGHT_MTL;
		mtl.light_material.light_color = light_color;
		mtl.light_material.intensity = intensity;

		material_names.push_back(name);
		material_reference_counters.push_back(0);
		materials.push_back(mtl);

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
																				   float transmission_weight)
	{
		Material mtl;
		mtl.material_type = SURFACE_MTL_DEFAULT;
		mtl.surface_material.diffuse_albedo = diffuse_albedo;
		mtl.surface_material.specular_albedo = specular_albedo;
		mtl.surface_material.alpha_x = roughness_x;
		mtl.surface_material.alpha_y = roughness_y;
		mtl.surface_material.ior_n = ior_n;
		mtl.surface_material.metalness = metalness;
		mtl.surface_material.specular_weight = specular_weight;
		mtl.surface_material.transmission_weight = transmission_weight;
		
		material_names.push_back(name);
		material_reference_counters.push_back(0);
		materials.push_back(mtl);
	
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

		// should not happen
		if (materials.empty())
		{
			return INVALID_UINT_32;
		}

		CUDA_CHECK(cudaDeviceSynchronize());

		return 0;
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
	inline uint32_t SceneManager::AddDistantLight(const std::string &name, uint32_t transform_idx, const glm::vec3 &light_color , float intensity, float theta_max)
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
		return (uint32_t)distant_lights.size() - 1;
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
	inline void SceneManager::UpdateDistantLightTransform(uint32_t selected_light_idx)
	{
		if (distant_lights.empty() || transform_states.empty()) { return; }
		if (selected_light_idx >(uint32_t)distant_lights.size() - 1) { return; }

		uint32_t transform_idx = distant_lights[selected_light_idx].transform_idx;

		transforms[transform_idx] = transform_states[transform_idx].GetTransformMatrix();
		i_transforms[transform_idx] = glm::inverse(transforms[transform_idx]);

		assert(scene.transforms != nullptr && scene.i_transforms != nullptr);
		CUDA_CHECK(cudaMemcpy(scene.transforms + transform_idx, &transforms[transform_idx], sizeof(glm::mat4), cudaMemcpyHostToDevice));
		CUDA_CHECK(cudaMemcpy(scene.i_transforms + transform_idx, &i_transforms[transform_idx], sizeof(glm::mat4), cudaMemcpyHostToDevice));
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
			float irradiance = (mtl.light_material.light_color.x + mtl.light_material.light_color.y + mtl.light_material.light_color.z) * mtl.light_material.intensity * ONE_PI;

			if (geometry.geometry_type == TRIANGLE_MESH)
			{
				const uint32_t *mesh_vidxs = vidxs.data() + geometry.tri_mesh.vidx_offset;
				const glm::vec3 *mesh_positions = positions.data() + geometry.tri_mesh.position_offset;
				const Triangle *mesh_triangles = triangles.data() + geometry.tri_mesh.triangle_offset;

				for (uint32_t triangle_idx = 0; triangle_idx < geometry.tri_mesh.triangle_count; ++triangle_idx)
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
	inline uint32_t SceneManager::AddVolume(const std::string &name, const glm::vec3 &sigma_s, const glm::vec3 &sigma_a)
	{
		Volume volume;
		volume.sigma_a = sigma_a;
		volume.sigma_s = sigma_s;

		volumes.push_back(volume);
		volume_names.push_back(name);
		return (uint32_t)volumes.size() - 1;
	}

	void SceneManager::TestScene(int screen_width, int screen_height)
	{
		AddSurfaceMaterial("default material", glm::vec3(0.0f));

		uint32_t cube_idx = AddQube("Cube"), sphere_idx = AddSphere("Sphere", 1.0f);

		AddPrimInstance(cube_idx,
			AddTransform(glm::vec3(0.0f, 0.0f, -4.0f)),
			AddSurfaceMaterial("green", glm::vec3(0.0f, 0.5f, 0.0f)));

		AddPrimInstance(cube_idx,
			AddTransform(glm::vec3(0.0f, 1.5f, -4.0f)),
			AddSurfaceMaterial("red", glm::vec3(0.5f, 0.0f, 0.0f)));

		AddPrimInstance(cube_idx,
			AddTransform(glm::vec3(1.5f, 0.0f, -4.0f)),
			AddSurfaceMaterial("purple", glm::vec3(0.5f, 0.0f, 0.5f)));

		AddPrimInstance(cube_idx,
			AddTransform(glm::vec3(-1.5f, 0.0f, -4.0f)),
			AddSurfaceMaterial("blue", glm::vec3(0.0f, 0.0f, 0.5f)));

		AddPrimInstance(cube_idx,
			AddTransform(glm::vec3(0.0f, 4.0f, -3.5f), glm::vec3(0.6f, 0.05f, 0.6f)),
			AddLightMaterial("Light", glm::vec3(1.0f), 3.0f));

		AddPrimInstance(cube_idx,
			AddTransform(glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(10000.0f, 0.1f, 10000.0f)),
			AddSurfaceMaterial("Floor", glm::vec3(0.5f, 0.5f, 0.5f)));

		AddPrimInstance(sphere_idx,
			AddTransform(glm::vec3(0.0f, 0.05f, -1.0f)),
			AddSurfaceMaterial("Sphere Mtl", glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f)));

		ACBVHBuilder SceneBuilder(prim_instances.data(),
			(uint32_t)prim_instances.size(),
			transforms.data(),
			geometries.data(),
			bottom_nodes.data());
		SceneBuilder.BuildSceneBVH(top_nodes, &top_BVH_time);

		AddDistantLight("Distant Light", AddTransform(glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(0.0f, 0.0f, 0.0f)), glm::vec3(1.0f), 2.0f, 8.0f);

		LoadShapeLights();
	}
	void SceneManager::CornellBox(int screen_width, int screen_height)
	{
		uint32_t cube_idx = AddQube("Cube"), sphere_idx = AddSphere("Sphere", 1.0f);
		uint32_t world_volume_idx = AddVolume("world volume", glm::vec3(0.03f), glm::vec3(0.01f));

		glm::vec3 camera_from = glm::vec3(0.0f, 0.0f, 6.0f);
		glm::vec3 camera_look = glm::vec3(0.0f, 0.0f, 0.0f);
		float camera_fov = glm::radians(45.0f);
		float camera_aspect = float(screen_width) / float(screen_height);

		camera.SetFov(camera_fov);
		camera.SetAspectRatio(camera_aspect);
		camera.SetPosition(camera_from);
		camera.SetDir(camera_from - camera_look);
		camera.SetIOR(1.0f);
		camera.SetVolumeIndex(world_volume_idx);

		AddSurfaceMaterial("default material", glm::vec3(0.25f));

		// left
		AddPrimInstance(cube_idx,
			AddTransform(glm::vec3(-5.025f, 0.0f, -5.0f), glm::vec3(0.05f, 10.0f, 10.0f)),
			AddSurfaceMaterial("left wall", glm::vec3(0.65f, 0.05f, 0.05f)), 
			1.0f,
			world_volume_idx, -1);
		
		// right
		AddPrimInstance(cube_idx,
			AddTransform(glm::vec3(5.025f, 0.0f, -5.0f), glm::vec3(0.05f, 10.0f, 10.0f)),
			AddSurfaceMaterial("right wall", glm::vec3(0.15f, 0.55f, 0.15f)), 
			1.0f,
			world_volume_idx, -1);

		// bottom
		AddPrimInstance(cube_idx,
			AddTransform(glm::vec3(0.0f, -5.025f, -5.0f), glm::vec3(10.0f, 0.05f, 10.0f)),
			AddSurfaceMaterial("bottom wall", glm::vec3(0.75f, 0.75f, 0.75f)),
			1.0f,
			world_volume_idx, -1);

		AddPrimInstance(cube_idx,
			AddTransform(glm::vec3(0.0f, 5.025f, -5.0f), glm::vec3(10.0f, 0.05f, 10.0f)),
			AddSurfaceMaterial("top wall", glm::vec3(0.75f, 0.75f, 0.75f)),
			1.0f,
			world_volume_idx, -1);

		// back
		AddPrimInstance(cube_idx,
			AddTransform(glm::vec3(0.0f, 0.0f, -10.025f), glm::vec3(10.0f, 10.0f, 0.05f)),
			AddSurfaceMaterial("back wall", glm::vec3(0.75f, 0.75f, 0.75f)), 
			1.0f,
			world_volume_idx, -1);

		AddPrimInstance(cube_idx,
			AddTransform(glm::vec3(-3.0f, -3.0f, -5.0f), glm::vec3(2.15f, 4.0f, 2.15f)),
			AddSurfaceMaterial("cube left", glm::vec3(0.75f, 0.75f, 0.75f)),
			1.0f,
			world_volume_idx, -1);

		AddPrimInstance(cube_idx,
			AddTransform(glm::vec3(0.0f, -3.0f, -5.0f), glm::vec3(2.15f, 4.0f, 2.15f)),
			AddSurfaceMaterial("cube mid", glm::vec3(0.75f, 0.75f, 0.75f)),
			1.0f,
			world_volume_idx, -1);

		AddPrimInstance(cube_idx,
			AddTransform(glm::vec3(3.0f, -3.0f, -5.0f), glm::vec3(2.15f, 4.0f, 2.15f)),
			AddSurfaceMaterial("cube right", glm::vec3(0.75f, 0.75f, 0.75f)),
			1.0f,
			world_volume_idx, -1);

		// light
		AddPrimInstance(cube_idx,
			AddTransform(glm::vec3(-3.0f, 4.15f, -5.0f), glm::vec3(2.0f, 0.3f, 2.0f)),
			AddLightMaterial("ceil light", glm::vec3(1.0f), 15.0f),
			1.0f,
			world_volume_idx, -1);

		// sphere light
		AddPrimInstance(sphere_idx,
			AddTransform(glm::vec3(3.0f, 4.15f, -5.0f), glm::vec3(0.3f, 0.3f, 0.3f)),
			AddLightMaterial("sphere light", glm::vec3(1.0f), 36.0f),
			1.0f,
			world_volume_idx, -1);

		// ground
		AddPrimInstance(cube_idx,
			AddTransform(glm::vec3(0.0f, -5.55f, 0.0f), glm::vec3(10000.0, 1.0f, 10000.0f)),
			AddSurfaceMaterial("ground", glm::vec3(0.55f, 0.55f, 0.55f), glm::vec3(0.0f), 0.2, 0.2, 1.3, 0.0f, 0.0f, 0.0f),
			1.0f,
			world_volume_idx, -1);

		ACBVHBuilder SceneBuilder(prim_instances.data(),
			(uint32_t)prim_instances.size(),
			transforms.data(),
			geometries.data(),
			bottom_nodes.data());
		SceneBuilder.BuildSceneBVH(top_nodes, &top_BVH_time);

		AddDistantLight("Distant Light", AddTransform(glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(0.0f, 0.0f, 0.0f)), glm::vec3(1.0f), 2.0f, 8.0f);

		LoadShapeLights();
	}
};