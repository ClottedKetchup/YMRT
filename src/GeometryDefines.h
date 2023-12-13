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

};