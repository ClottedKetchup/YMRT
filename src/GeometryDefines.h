#pragma once
#include "MathCommon.h"

namespace YumeRT
{
	struct Triangle
	{
		uint32_t id0, id1, id2;
	};
	enum GEOMETRY_TYPE
	{
		TRIANGLE_MESH = 0,
		SPHERE = 1
	};
#pragma pack(push, 16)
	struct GeometryData
	{
		uint32_t geometry_type;
		uint32_t padding;
		union
		{
			struct
			{
				uint32_t triangle_count;
				uint32_t triangle_offset;
				uint32_t bottom_node_count;
				uint32_t bottom_node_offset;

				uint32_t idx_count;

				uint32_t vidx_offset;
				uint32_t position_count;
				uint32_t position_offset;

				uint32_t nidx_offset;
				uint32_t normal_count;
				uint32_t normal_offset;

				uint32_t uvidx_offset;
				uint32_t texcoord_count;
				uint32_t texcoord_offset;
			}tri_mesh;
			struct
			{
				float radius;
				float theta_min, theta_max;
				float phi_min, phi_max;
			}sphere;
		};
	};
#pragma pack(pop)

	struct TriangleMesh 
	{
		uint8_t *device_data_ptr;
		uint8_t *host_data_ptr;
		int64_t triangle_count;
		int64_t triangle_offset;
		int64_t node_offset;
		int64_t position_idx_offset;
		int64_t position_offset;
		int64_t normal_idx_offset;
		int64_t normal_offset;
		int64_t texcoord_idx_offset;
		int64_t texcoord_offset;
		
		__device__ __host__ inline TriangleMesh(): device_data_ptr(nullptr), host_data_ptr(nullptr), 
		triangle_count(-1),
		triangle_offset(-1),
		node_offset(-1),
		position_idx_offset(-1),
		position_offset(-1),
		normal_idx_offset(-1),
		normal_offset(-1),
		texcoord_idx_offset(-1),
		texcoord_offset(-1) {}
	};
	struct Sphere 
	{
		float radius;
		float theta_min, theta_max;
		float phi_min, phi_max;

		__device__ __host__ inline Sphere(): radius(1.0f) {}
	};
	struct Geometry 
	{
		uint32_t geometry_type;
		union 
		{
			TriangleMesh triangle_mesh;
			Sphere sphere;
		};
	};
};