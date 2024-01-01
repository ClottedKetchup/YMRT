#pragma once

#include <driver_types.h>

#include "MathCommon.h"

namespace YumeRT
{
	struct PrimitiveInstance
	{
		uint32_t geometry_idx = INVALID_UINT_32;
		uint32_t transform_idx = INVALID_UINT_32;
		uint32_t material_idx = INVALID_UINT_32;
		int outer_volume_idx = -1;
		int inner_volume_idx = -1;
		float external_ior = 1.0f;

		__device__ __host__ PrimitiveInstance() {};
		__device__ __host__ PrimitiveInstance(uint32_t geometry_idx, 
																	uint32_t transform_idx, 
																	uint32_t material_idx, 
																	float external_ior = 1.0f,
																	int outer_volume_idx = -1,
																	int inner_volume_idx = -1)
			:geometry_idx(geometry_idx), transform_idx(transform_idx), material_idx(material_idx), external_ior(external_ior), outer_volume_idx(outer_volume_idx), inner_volume_idx(inner_volume_idx) {}
	};
};