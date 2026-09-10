#pragma once

#include "Helper.h"
#include "MathCommon.h"
#include "BottomBVH.h"
#include "TriangleDefines.h"

namespace YMRT
{
	enum GEOMETRY_TYPE
	{
		TRIANGLE_MESH = 0,
		SPHERE = 1
	};


	struct TriangleMesh 
	{
		uint8_t *device_data_ptr;
		uint8_t *host_data_ptr;
		size_t data_size;
		int64_t triangle_count;
		int64_t triangle_offset; // Triangle
		int64_t position_idx_offset;
		int64_t position_offset; // vec3
		int64_t normal_idx_offset;
		int64_t normal_offset; // vec3
		int64_t texcoord_idx_offset;
		int64_t texcoord_offset; // vec2
		int64_t node_offset; // node
		int64_t boundingbox_offset;

		int64_t position_array_size; // in terms of float.
		int64_t normal_array_size;
		int64_t texcoord_array_size;
		
		__device__ __host__ inline TriangleMesh() : device_data_ptr(nullptr), host_data_ptr(nullptr),
			data_size(0),
			triangle_count(-1),
			triangle_offset(-1),
			position_idx_offset(-1),
			position_offset(-1),
			normal_idx_offset(-1),
			normal_offset(-1),
			texcoord_idx_offset(-1),
			texcoord_offset(-1),
			node_offset(-1),
			boundingbox_offset(-1),

			position_array_size(0),
			normal_array_size(0),
			texcoord_array_size(0)
		{}
		__device__ __host__ inline Triangle* GetTrianglesDevice() const 
		{
#ifdef __CUDA_ARCH__
			return  device_data_ptr != nullptr && triangle_offset != -1 ? (Triangle*)(device_data_ptr + triangle_offset) : nullptr;
#else
			return GetTrianglesHost();
#endif 
		}
		__device__ __host__ inline uint32_t* GetPositionIndicesDevice() const 
		{
#ifdef __CUDA_ARCH__
			return  device_data_ptr != nullptr && position_idx_offset != -1 ? (uint32_t*)(device_data_ptr + position_idx_offset) : nullptr;
#else
			return GetPositionIndicesHost();
#endif
		}
		__device__ __host__ inline glm::vec3* GetPositionsDevice() const
		{
#ifdef __CUDA_ARCH__
			return  device_data_ptr != nullptr && position_offset != -1 ? (glm::vec3*)(device_data_ptr + position_offset) : nullptr;
#else
			return GetPositionsHost();
#endif
		}
		__device__ __host__ inline uint32_t* GetNormalIndicesDevice() const
		{
#ifdef __CUDA_ARCH__
			return  device_data_ptr != nullptr && normal_idx_offset != -1 ? (uint32_t*)(device_data_ptr + normal_idx_offset) : nullptr;
#else
			return GetNormalIndicesHost();
#endif
		}
		__device__ __host__ inline glm::vec3* GetNormalsDevice() const 
		{
#ifdef __CUDA_ARCH__
			return  device_data_ptr != nullptr && normal_offset != -1 ? (glm::vec3*)(device_data_ptr + normal_offset) : nullptr;
#else
			return GetNormalsHost();
#endif
		}
		__device__ __host__ inline uint32_t* GetTexcoordIndicesDevice() const 
		{
#ifdef __CUDA_ARCH__
			return  device_data_ptr != nullptr && texcoord_idx_offset != -1 ? (uint32_t*)(device_data_ptr + texcoord_idx_offset) : nullptr;
#else
			return GetTexcoordIndicesHost();
#endif
		}
		__device__ __host__ inline glm::vec2* GetTexcoordsDevice() const 
		{
#ifdef __CUDA_ARCH__
			return  device_data_ptr != nullptr && texcoord_offset != -1 ? (glm::vec2*)(device_data_ptr + texcoord_offset) : nullptr;
#else
			return GetTexcoordsHost();
#endif
		}
		__device__ __host__ inline BottomNode* GetNodesDevice() const 
		{
#ifdef __CUDA_ARCH__
			return  device_data_ptr != nullptr && node_offset != -1 ? (BottomNode*)(device_data_ptr + node_offset) : nullptr;
#else 
			return GetNodesHost();
#endif
		}
		__device__ __host__ inline BBox3* GetBoundingBoxDevice() const 
		{
#ifdef __CUDA_ARCH__
			return  device_data_ptr != nullptr && boundingbox_offset != -1 ? (BBox3*)(device_data_ptr + boundingbox_offset) : nullptr;
#else
			return GetBoundingBoxHost();
#endif
		}

		__device__ __host__ inline Triangle* GetTrianglesHost() const
		{
			return  host_data_ptr != nullptr && triangle_offset != -1 ? (Triangle*)(host_data_ptr + triangle_offset) : nullptr;
		}
		__device__ __host__ inline uint32_t* GetPositionIndicesHost() const
		{
			return  host_data_ptr != nullptr && position_idx_offset != -1 ? (uint32_t*)(host_data_ptr + position_idx_offset) : nullptr;
		}
		__device__ __host__ inline glm::vec3* GetPositionsHost() const
		{
			return  host_data_ptr != nullptr && position_offset != -1 ? (glm::vec3*)(host_data_ptr + position_offset) : nullptr;
		}
		__device__ __host__ inline uint32_t* GetNormalIndicesHost() const
		{
			return  host_data_ptr != nullptr && normal_idx_offset != -1 ? (uint32_t*)(host_data_ptr + normal_idx_offset) : nullptr;
		}
		__device__ __host__ inline glm::vec3* GetNormalsHost() const
		{
			return  host_data_ptr != nullptr && normal_offset != -1 ? (glm::vec3*)(host_data_ptr + normal_offset) : nullptr;
		}
		__device__ __host__ inline uint32_t* GetTexcoordIndicesHost() const
		{
			return  host_data_ptr != nullptr && texcoord_idx_offset != -1 ? (uint32_t*)(host_data_ptr + texcoord_idx_offset) : nullptr;
		}
		__device__ __host__ inline glm::vec2* GetTexcoordsHost() const
		{
			return  host_data_ptr != nullptr && texcoord_offset != -1 ? (glm::vec2*)(host_data_ptr + texcoord_offset) : nullptr;
		}
		__device__ __host__ inline BottomNode* GetNodesHost() const
		{
			return  host_data_ptr != nullptr && node_offset != -1 ? (BottomNode*)(host_data_ptr + node_offset) : nullptr;
		}
		__device__ __host__ inline BBox3* GetBoundingBoxHost() const
		{
			return  host_data_ptr != nullptr && boundingbox_offset != -1 ? (BBox3*)(host_data_ptr + boundingbox_offset) : nullptr;
		}
		
		__host__ inline void Destory()
		{
			PRINT_GPU_FREE_MEMORY("before destroy geometry");
			if (device_data_ptr != nullptr) { FREE_GPU_RESOURCE(device_data_ptr); }
			if (host_data_ptr != nullptr) { _aligned_free(host_data_ptr); host_data_ptr = nullptr; }
			PRINT_GPU_FREE_MEMORY("after destroy geometry");
		}
		__host__ inline void Upload() 
		{
			if (device_data_ptr == nullptr) { UPLOAD_TO_GPU(device_data_ptr, host_data_ptr, data_size); }
		}
	};
	struct Sphere 
	{
		float radius;
		float theta_min, theta_max;
		float phi_min, phi_max;

		__device__ __host__ inline Sphere(): radius(1.0f) {}
	};
	struct GeometryData 
	{
		uint32_t geometry_type;
		union 
		{
			TriangleMesh triangle_mesh;
			Sphere sphere;
		};

		__device__ __host__ inline GeometryData()
		{
			
		}
		__host__ inline GeometryData& InitCustomMesh(std::vector<Triangle>& mesh_triangles, 
			std::vector<uint32_t>& mesh_position_idxs,
			std::vector<float>& mesh_positions,
			std::vector<uint32_t> & mesh_normal_idxs,
			std::vector<float>& mesh_normals,
			std::vector<uint32_t>& mesh_texcoord_idxs,
			std::vector<float>& mesh_texcoords)
		{
			assert(!mesh_triangles.empty());
			geometry_type = GEOMETRY_TYPE::TRIANGLE_MESH;

			assert(mesh_triangles.size() * 3 == mesh_position_idxs.size());
			assert(mesh_triangles.size() * 3 == mesh_normal_idxs.size());
			assert(mesh_triangles.size() * 3 == mesh_texcoord_idxs.size());

			triangle_mesh.device_data_ptr = nullptr;
			triangle_mesh.host_data_ptr = nullptr;
			triangle_mesh.data_size = 0;
			triangle_mesh.triangle_count = -1;
			triangle_mesh.triangle_offset = -1;
			triangle_mesh.position_idx_offset = -1;
			triangle_mesh.position_offset = -1;
			triangle_mesh.normal_idx_offset = -1;
			triangle_mesh.normal_offset = -1;
			triangle_mesh.texcoord_idx_offset = -1;
			triangle_mesh.texcoord_offset = -1;
			triangle_mesh.node_offset = -1;
			triangle_mesh.boundingbox_offset = -1;

			triangle_mesh.position_array_size = 0;
			triangle_mesh.normal_array_size = 0;
			triangle_mesh.texcoord_array_size = 0;

			std::vector<uint8_t> geometry_data_buffer;
			auto append_to_buffer = [&](const void* data, size_t data_size)->size_t {
				if (data_size == 0) {
					return -1;
				}
				// align in 16 bytes
				size_t aligned_data_size = AlignedMemorySize(data_size, 16);
				size_t data_offset = geometry_data_buffer.size();
				geometry_data_buffer.resize(geometry_data_buffer.size() + aligned_data_size);
				memcpy(&geometry_data_buffer[data_offset], data, data_size);
				return data_offset;
			};


			triangle_mesh.triangle_count = mesh_triangles.size();
			triangle_mesh.triangle_offset = append_to_buffer(mesh_triangles.data(), sizeof(Triangle) * mesh_triangles.size());
			triangle_mesh.position_idx_offset = append_to_buffer(mesh_position_idxs.data(), sizeof(uint32_t) * mesh_position_idxs.size());
			triangle_mesh.position_offset = append_to_buffer(mesh_positions.data(), sizeof(float) * mesh_positions.size());
			triangle_mesh.position_array_size = mesh_positions.size();

			triangle_mesh.normal_idx_offset = append_to_buffer(mesh_normal_idxs.data(), sizeof(uint32_t) * mesh_normal_idxs.size());
			triangle_mesh.normal_offset = append_to_buffer(mesh_normals.data(), sizeof(float) * mesh_normals.size());
			triangle_mesh.normal_array_size = mesh_normals.size();

			triangle_mesh.texcoord_idx_offset = append_to_buffer(mesh_texcoord_idxs.data(), sizeof(uint32_t) * mesh_texcoord_idxs.size());
			triangle_mesh.texcoord_offset = append_to_buffer(mesh_texcoords.data(), sizeof(float) * mesh_texcoords.size());
			triangle_mesh.texcoord_array_size = mesh_texcoords.size();

			BottomBVHBuilder MeshBuilder((Triangle*)(geometry_data_buffer.data() + triangle_mesh.triangle_offset),
																triangle_mesh.triangle_count,
																(glm::vec3*)(geometry_data_buffer.data() + triangle_mesh.position_offset),
																(uint32_t*)(geometry_data_buffer.data() + triangle_mesh.position_idx_offset));
			std::vector<BottomNode> bottom_nodes = MeshBuilder.BuildMeshBVH();
			triangle_mesh.node_offset = append_to_buffer(bottom_nodes.data(), sizeof(BottomNode) * bottom_nodes.size());

			const BBox3 bounding_box = bottom_nodes.empty()? BBox3(): bottom_nodes.front().bbox;
			triangle_mesh.boundingbox_offset = append_to_buffer(&bounding_box, sizeof(BBox3));

			triangle_mesh.host_data_ptr = (uint8_t*)(_aligned_malloc(sizeof(uint8_t) * geometry_data_buffer.size(), 16));
			memcpy(triangle_mesh.host_data_ptr, geometry_data_buffer.data(), sizeof(uint8_t) * geometry_data_buffer.size());

			triangle_mesh.data_size = sizeof(uint8_t) * geometry_data_buffer.size();
			return *this;
		}
		__host__ inline GeometryData& InitCube()
		{
			geometry_type = GEOMETRY_TYPE::TRIANGLE_MESH;

			triangle_mesh.device_data_ptr = nullptr;
			triangle_mesh.host_data_ptr = nullptr;
			triangle_mesh.data_size = 0;
			triangle_mesh.triangle_count = -1;
			triangle_mesh.triangle_offset = -1;
			triangle_mesh.position_idx_offset = -1;
			triangle_mesh.position_offset = -1;
			triangle_mesh.normal_idx_offset = -1;
			triangle_mesh.normal_offset = -1;
			triangle_mesh.texcoord_idx_offset = -1;
			triangle_mesh.texcoord_offset = -1;
			triangle_mesh.node_offset = -1;
			triangle_mesh.boundingbox_offset = -1;

			triangle_mesh.position_array_size = 0;
			triangle_mesh.normal_array_size = 0;
			triangle_mesh.texcoord_array_size = 0;

			std::vector<uint8_t> geometry_data_buffer;
			auto append_to_buffer = [&](const void *data, size_t data_size)->size_t {
				if (data_size == 0) {
					return -1;
				}
				// align in 16 bytes
				size_t aligned_data_size = AlignedMemorySize(data_size, 16);
				size_t data_offset = geometry_data_buffer.size();
				geometry_data_buffer.resize(geometry_data_buffer.size() + aligned_data_size);
				memcpy(&geometry_data_buffer[data_offset], data, data_size);
				return data_offset;
			};

			std::vector<Triangle> cube_triangles(12);
			for (int triangle_idx = 0, i = 0; triangle_idx < (int)cube_triangles.size(); ++triangle_idx, i += 3)
			{
				cube_triangles[triangle_idx].id0 = i;
				cube_triangles[triangle_idx].id1 = i + 1;
				cube_triangles[triangle_idx].id2 = i + 2;
			}
			triangle_mesh.triangle_count = cube_triangles.size();
			triangle_mesh.triangle_offset = append_to_buffer(cube_triangles.data(), sizeof(Triangle) * cube_triangles.size());

			std::vector<uint32_t> cube_position_idxs = {
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
			33, 34, 35
			};
			triangle_mesh.position_idx_offset = append_to_buffer(cube_position_idxs.data(), sizeof(uint32_t) * cube_position_idxs.size());

			std::vector<float> cube_positions = {
			-0.5f, -0.5f, -0.5f,
			0.5f, 0.5f, -0.5f,
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
			-0.5f,  0.5f,  0.5f
			};
			triangle_mesh.position_offset = append_to_buffer(cube_positions.data(), sizeof(float) * cube_positions.size());
			triangle_mesh.position_array_size = cube_positions.size();

			std::vector<uint32_t> cube_normal_idxs = {
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
			33, 34, 35
			};
			triangle_mesh.normal_idx_offset = append_to_buffer(cube_normal_idxs.data(), sizeof(uint32_t) * cube_normal_idxs.size());

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
			0.0f,  1.0f,  0.0f
			};
			triangle_mesh.normal_offset = append_to_buffer(cube_normals.data(), sizeof(float) * cube_normals.size());
			triangle_mesh.normal_array_size = cube_normals.size();

			std::vector<uint32_t> cube_texcoord_idxs = {
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
			33, 34, 35
			};
			triangle_mesh.texcoord_idx_offset = append_to_buffer(cube_texcoord_idxs.data(), sizeof(uint32_t) * cube_texcoord_idxs.size());

			std::vector<float> cube_texcoords = {
			0.0f,  1.0f,
			1.0f,  0.0f,
			1.0f,  1.0f,
			1.0f,  0.0f,
			0.0f,  1.0f,
			0.0f,  0.0f,

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

			1.0f,  1.0f,
			0.0f,  0.0f,
			1.0f,  0.0f,
			0.0f,  0.0f,
			1.0f,  1.0f,
			0.0f,  1.0f,

			0.0f,  0.0f,
			1.0f,  0.0f,
			1.0f,  1.0f,
			1.0f,  1.0f,
			0.0f,  1.0f,
			0.0f,  0.0f,

			0.0f,  1.0f,
			1.0f,  0.0f,
			1.0f,  1.0f,
			1.0f,  0.0f,
			0.0f,  1.0f,
			0.0f,  0.0f
			};
			triangle_mesh.texcoord_offset = append_to_buffer(cube_texcoords.data(), sizeof(float) * cube_texcoords.size());
			triangle_mesh.texcoord_array_size = cube_texcoords.size();

			BottomBVHBuilder MeshBuilder((Triangle*)(geometry_data_buffer.data() + triangle_mesh.triangle_offset),
																triangle_mesh.triangle_count,
																(glm::vec3*)(geometry_data_buffer.data() + triangle_mesh.position_offset),
																(uint32_t*)(geometry_data_buffer.data() + triangle_mesh.position_idx_offset));
			std::vector<BottomNode> bottom_nodes = MeshBuilder.BuildMeshBVH();
			triangle_mesh.node_offset = append_to_buffer(bottom_nodes.data(), sizeof(BottomNode) * bottom_nodes.size());

			const BBox3 bounding_box = bottom_nodes.front().bbox;
			triangle_mesh.boundingbox_offset = append_to_buffer(&bounding_box, sizeof(BBox3));

			triangle_mesh.host_data_ptr = (uint8_t*)(_aligned_malloc(sizeof(uint8_t) * geometry_data_buffer.size(), 16));
			memcpy(triangle_mesh.host_data_ptr, geometry_data_buffer.data(), sizeof(uint8_t) * geometry_data_buffer.size());
			
			triangle_mesh.data_size = sizeof(uint8_t) * geometry_data_buffer.size();
			return *this;
		}
		__host__ inline GeometryData& InitSphere(float radius)
		{
			geometry_type = GEOMETRY_TYPE::SPHERE;
			sphere.radius = radius;
			return *this;
		}
		__host__ inline void Destory() 
		{
			if (geometry_type == GEOMETRY_TYPE::TRIANGLE_MESH) 
			{
				triangle_mesh.Destory();
			}
			else if (geometry_type == GEOMETRY_TYPE::SPHERE)
			{
			
			}
			else 
			{
			
			}
		}
		__host__ inline void Upload()
		{
			if (geometry_type == GEOMETRY_TYPE::TRIANGLE_MESH)
			{
				triangle_mesh.Upload();
			}
			else if (geometry_type == GEOMETRY_TYPE::SPHERE)
			{

			}
			else
			{

			}
		}
	};
};