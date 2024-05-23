#include <ppl.h>

#include "FileIO.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace YumeRT 
{
	ImageTexture LoadImageTexture(const std::string &file_name, ImageTextureManager &image_texture_manager)
	{
		// load tex file
		ImageTexture image_texture;
		image_texture.tile_offset = -1;
		image_texture.file_offset = -1;

		image_texture.warp_mode = WARP_MODE_REPEAT;

		image_texture.u_scale = 1.0f;
		image_texture.v_scale = 1.0f;
		image_texture.u_offset = 0.0f;
		image_texture.v_offset = 0.0f;

		std::vector<float> image_buffer;
		
		// read file
		{
			unsigned char *image_handle = nullptr;
			int image_width = 0, image_height = 0, image_channel = 0;
			image_handle = stbi_load(file_name.c_str(), &image_width, &image_height, &image_channel, 0);
			if (image_width == 0 || image_height == 0 || image_channel == 0 || image_handle == nullptr) 
			{
				printf("Image file %s load fail.\n", file_name.c_str());
				if (image_handle != nullptr) { stbi_image_free(image_handle); }
				return image_texture;
			}

			image_buffer.resize((size_t)image_width * (size_t)image_height * (size_t)image_channel, 0.0f);
			constexpr float inv_255 = 1.0f / 255.0f;
			for (size_t i = 0; i < image_buffer.size(); ++i) {
				image_buffer[i] = SRGBToLinear(float((int)(image_handle[i])) * inv_255);
			}
			printf("Image file %s load success.\n", file_name.c_str());
			stbi_image_free(image_handle);

			image_texture.width = image_width;
			image_texture.height = image_height;
			image_texture.channel_count = image_channel;
		}

		image_texture.file_offset = (int)image_texture_manager.image_texture_files.size();
		image_texture_manager.image_texture_files.push_back(ImageFile(file_name));

		image_texture.mipmap_count = 1;
		image_texture.mipmap_tile_offsets[0] = 0;

		image_texture.tile_offset = (int)image_texture_manager.image_texture_tiles.size();
		const int tile_x_count = (image_texture.width + TEX_TILE_RES_X - 1) / TEX_TILE_RES_X;
		const int tile_y_count = (image_texture.height + TEX_TILE_RES_Y - 1) / TEX_TILE_RES_Y;
		for (int i = 0; i < tile_y_count * tile_x_count; ++i) { image_texture_manager.image_texture_tiles.push_back(ImageTile()); }
		
		concurrency::parallel_for(0, tile_y_count * tile_x_count, [&](int tile_idx) 
		{
			ImageTile &image_tile = image_texture_manager.image_texture_tiles[image_texture.tile_offset + tile_idx];
			image_tile.data_size = sizeof(float) * image_texture.channel_count * TEX_TILE_RES_X * TEX_TILE_RES_Y;
			image_tile.host_data = (void*)malloc(image_tile.data_size);
			memset(image_tile.host_data, 0, image_tile.data_size);

			const int tile_j = tile_idx / tile_x_count;
			const int tile_i = tile_idx % tile_x_count;
			const int tile_x_base = tile_i * TEX_TILE_RES_X;
			const int tile_y_base = tile_j * TEX_TILE_RES_Y;
			for (int j = 0; j < TEX_TILE_RES_Y; ++j)
			{
				for (int i = 0; i < TEX_TILE_RES_X; ++i)
				{
					const int pixel_x = tile_x_base + i;
					const int pixel_y = tile_y_base + j;
					if (!(pixel_x < image_texture.width && pixel_y < image_texture.height)) { continue; }
					const size_t pixel_offset = (pixel_y * image_texture.width + pixel_x) * image_texture.channel_count;
					const size_t tile_pixel = (j * TEX_TILE_RES_X + i) * image_texture.channel_count;
					for (int channel_idx = 0; channel_idx < image_texture.channel_count; ++channel_idx){
						((float*)(image_tile.host_data))[tile_pixel + channel_idx] = image_buffer[pixel_offset + channel_idx];
					}
				}
			}
		});
		return image_texture;
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