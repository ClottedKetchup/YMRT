#pragma once

#include <driver_types.h>

#include "MathCommon.h"

namespace YumeRT
{
	struct PrimitiveInstance
	{
		uint32_t geometry_idx ;
		uint32_t transform_idx;
		uint32_t transform_count;
		uint32_t material_idx;
		int inner_volume_idx;
		int treat_as_boundary;
		
		__device__ __host__ inline PrimitiveInstance() : geometry_idx(EMPTY_UINT32), transform_idx(EMPTY_UINT32), transform_count(1), material_idx(EMPTY_UINT32), inner_volume_idx(-1), treat_as_boundary(0) {}
		__device__ __host__ inline PrimitiveInstance(uint32_t geometry_idx, uint32_t transform_idx, uint32_t material_idx, int inner_volume_idx = -1, int treat_as_boundary = 0)
			: geometry_idx(geometry_idx), transform_idx(transform_idx), transform_count(1), material_idx(material_idx), 
			inner_volume_idx(inner_volume_idx), treat_as_boundary(treat_as_boundary) {}
	};
};