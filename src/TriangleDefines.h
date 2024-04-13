#pragma once

#include <driver_types.h>

#include "MathCommon.h"

namespace YumeRT 
{
	struct Triangle
	{
		uint32_t id0, id1, id2;
	};

	__device__ __host__ inline  BBox3 GetTriBound(const Triangle &triangle, const glm::vec3 *mesh_positions, const uint32_t *mesh_vidxs)
	{
		BBox3 bbox;
		uint32_t vid0 = mesh_vidxs[triangle.id0];
		uint32_t vid1 = mesh_vidxs[triangle.id1];
		uint32_t vid2 = mesh_vidxs[triangle.id2];

		const glm::vec3 &p0 = mesh_positions[vid0];
		const glm::vec3 &p1 = mesh_positions[vid1];
		const glm::vec3 &p2 = mesh_positions[vid2];

		bbox = BBox3Extend(bbox, p0);
		bbox = BBox3Extend(bbox, p1);
		bbox = BBox3Extend(bbox, p2);

		bbox = BoundFix(bbox);
		return bbox;
	}

	__device__ __host__ inline glm::vec3 GetTriCenter(const Triangle &triangle, const glm::vec3 *mesh_positions, const uint32_t *mesh_vidxs)
	{
		uint32_t vid0 = mesh_vidxs[triangle.id0];
		uint32_t vid1 = mesh_vidxs[triangle.id1];
		uint32_t vid2 = mesh_vidxs[triangle.id2];

		const glm::vec3 &p0 = mesh_positions[vid0];
		const glm::vec3 &p1 = mesh_positions[vid1];
		const glm::vec3 &p2 = mesh_positions[vid2];
		return (p0 + p1 + p2) / 3.f;
	}
};