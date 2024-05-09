#pragma once

#include <driver_types.h>

#include "MathCommon.h"

namespace YumeRT
{
	struct PrimitiveInstance
	{
		uint32_t geometry_idx = EMPTY_UINT32;
		uint32_t transform_idx = EMPTY_UINT32;
		uint32_t material_idx = EMPTY_UINT32;
		int outer_volume_idx = -1;
		int inner_volume_idx = -1;
		float external_ior = 1.0f;
		bool treat_as_boundary = false;

		__device__ __host__ inline PrimitiveInstance() {};
		__device__ __host__ inline PrimitiveInstance(uint32_t geometry_idx,
																	uint32_t transform_idx, 
																	uint32_t material_idx, 
																	float external_ior = 1.0f,
																	int outer_volume_idx = -1,
																	int inner_volume_idx = -1,
																	bool is_volume_boundary = false)
			:geometry_idx(geometry_idx), 
			transform_idx(transform_idx), 
			material_idx(material_idx), 
			external_ior(external_ior), 
			outer_volume_idx(outer_volume_idx), 
			inner_volume_idx(inner_volume_idx), treat_as_boundary(is_volume_boundary) {}
	};
};