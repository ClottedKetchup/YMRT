#include "SceneManager.h"

namespace YumeRT 
{
	void SceneManager::InitHaltonPermuteTable()
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
	void SceneManager::InitSobolMatrices()
	{
		UPLOAD_TO_GPU(scene.sampler_data.sobol_matrices, sobol_matrices32, sizeof(uint32_t) * 1024u * 32u);
	}
	void SceneManager::DestroyResources()
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
	void SceneManager::Init()
	{
		InitHaltonPermuteTable();
		InitSobolMatrices();
	}

	void SceneManager::UploadSceneSetting()
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

		UPLOAD_TO_GPU(scene.camera, &camera, sizeof(Camera));

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

		CUDA_CHECK(cudaDeviceSynchronize());

		ResetSceneFlag();
	}
	void SceneManager::ClearInvaildTransform()
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

/*void SceneManager::ProcessSceneNode(aiNode *node, const aiScene *scene, float external_ior, int outer_vol_idx, int inner_vol_idx, bool is_volume_boundary)
	{
		if (node == nullptr) { return; }

		for (int i = 0; i < node->mNumMeshes; ++i)
		{
			aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];

			const std::string mesh_name = std::string(mesh->mName.C_Str());

			uint32_t mesh_idx = CreateCustomMesh(mesh);

			uint32_t material_idx = AddSurfaceMaterial(mesh_name + "_mtl");

			AddPrimInstance(mesh_idx, AddTransform(), material_idx, external_ior, outer_vol_idx, inner_vol_idx, is_volume_boundary);
		}

		for (int i = 0; i < node->mNumChildren; ++i)
		{
			ProcessSceneNode(node->mChildren[i], scene, external_ior, outer_vol_idx, inner_vol_idx, is_volume_boundary);
		}
	}*/

//inline uint32_t SceneManager::AddCustom(const std::string &name,
//	const std::string &file_path,
//	float external_ior,
//	int outer_vol_idx,
//	int inner_vol_idx,
//	bool is_volume_boundary)
//{
//	Assimp::Importer importer;
//	const aiScene *scene = importer.ReadFile(file_path, aiProcess_GenSmoothNormals | aiProcess_Triangulate);
//	if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || scene->mRootNode == nullptr)
//	{
//		printf("Fail to load model from path:\t %s \n", file_path.c_str());
//		importer.FreeScene();
//		return INVALID_UINT_32;
//	}
//	ProcessSceneNode(scene->mRootNode, scene, external_ior, outer_vol_idx, inner_vol_idx, is_volume_boundary);
//	importer.FreeScene();
//}

	//uint32_t SceneManager::CreateCustomMesh(const aiMesh *ai_mesh)
	//{
	//	return 0;
	//	const aimesh &assimp_mesh = (*ai_mesh);

	//	geometrydata mesh;
	//	mesh.geometry_type = geometry_type::triangle_mesh;

	//	mesh.tri_mesh.triangle_offset = (uint32_t)triangles.size();
	//	mesh.tri_mesh.vidx_offset = (uint32_t)vidxs.size();
	//	mesh.tri_mesh.position_offset = (uint32_t)positions.size();
	//	mesh.tri_mesh.nidx_offset = (uint32_t)nidxs.size();
	//	mesh.tri_mesh.normal_offset = (uint32_t)normals.size();
	//	mesh.tri_mesh.uvidx_offset = (uint32_t)uvidxs.size();
	//	mesh.tri_mesh.texcoord_offset = (uint32_t)texcoords.size();

	//	for (int i = 0; i < assimp_mesh.mnumfaces; ++i)
	//	{
	//		assert(assimp_mesh.mfaces[i].mnumindices == 3);
	//		
	//		triangle triangle;
	//		triangle.id0 = (i * 3) + 0;
	//		triangle.id1 = (i * 3) + 1;
	//		triangle.id2 = (i * 3) + 2;
	//		triangles.push_back(triangle);

	//		for (int vid = 0; vid < assimp_mesh.mfaces[i].mnumindices; ++vid)
	//		{
	//			uint32_t idx = assimp_mesh.mfaces[i].mindices[vid];
	//			vidxs.push_back(idx);
	//			nidxs.push_back(idx);
	//			uvidxs.push_back(idx);
	//		}
	//	}
	//	
	//	if (assimp_mesh.haspositions())
	//	{
	//		for (int vert_idx = 0; vert_idx < assimp_mesh.mnumvertices; ++vert_idx)
	//		{
	//			glm::vec3 position;
	//			position.x = assimp_mesh.mvertices[vert_idx].x;
	//			position.y = assimp_mesh.mvertices[vert_idx].y;
	//			position.z = assimp_mesh.mvertices[vert_idx].z;
	//			positions.push_back(position);
	//		}
	//	}

	//	if (assimp_mesh.hasnormals())
	//	{
	//		for (int n_idx = 0; n_idx < assimp_mesh.mnumvertices; ++n_idx)
	//		{
	//			glm::vec3 normal;
	//			normal.x = assimp_mesh.mnormals[n_idx].x;
	//			normal.y = assimp_mesh.mnormals[n_idx].y;
	//			normal.z = assimp_mesh.mnormals[n_idx].z;
	//			normals.push_back(normal);
	//		}
	//	}

	//	if (assimp_mesh.hastexturecoords(0))
	//	{
	//		for (int uv_idx = 0; uv_idx < assimp_mesh.mnumvertices; ++uv_idx)
	//		{
	//			glm::vec2 texcoord;
	//			texcoord.x = assimp_mesh.mtexturecoords[0][uv_idx].x;
	//			texcoord.y = assimp_mesh.mtexturecoords[0][uv_idx].y;
	//			texcoords.push_back(texcoord);
	//		}
	//	}

	//	mesh.tri_mesh.triangle_count = (uint32_t)triangles.size() - mesh.tri_mesh.triangle_offset;
	//	mesh.tri_mesh.idx_count = (uint32_t)vidxs.size() - mesh.tri_mesh.vidx_offset;
	//	mesh.tri_mesh.position_count =(uint32_t)positions.size() - mesh.tri_mesh.position_offset;
	//	mesh.tri_mesh.normal_count = (uint32_t)normals.size() - mesh.tri_mesh.normal_offset;
	//	mesh.tri_mesh.texcoord_count = (uint32_t)texcoords.size() - mesh.tri_mesh.texcoord_offset;

	//	// add sphere
	//	geometries.push_back(mesh);
	//	// add its counter to 0
	//	geometry_reference_counters.push_back(0);
	//	// add its name, now only for display
	//	geometry_names.push_back(std::string(assimp_mesh.mname.c_str()));

	//	geometrydata &mesh_data = geometries[geometries.size() - 1];
	//	bottombvhbuilder meshbuilder(triangles.data() + mesh_data.tri_mesh.triangle_offset,
	//		mesh_data.tri_mesh.triangle_count,
	//		positions.data() + mesh_data.tri_mesh.position_offset,
	//		vidxs.data() + mesh_data.tri_mesh.vidx_offset);
	//	auto m_bottom_nodes = meshbuilder.buildmeshbvh(&bottom_bvh_time);
	//	
	//	mesh_data.tri_mesh.bottom_node_offset = (uint32_t)bottom_nodes.size();
	//	mesh_data.tri_mesh.bottom_node_count = (uint32_t)m_bottom_nodes.size();
	//	bottom_nodes.insert(bottom_nodes.end(), m_bottom_nodes.begin(), m_bottom_nodes.end());

	//	addsceneflag(scenechange_flag::geometry_add);
	//	return (uint32_t)geometries.size() - 1;
	//}