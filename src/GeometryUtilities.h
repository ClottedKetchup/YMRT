#pragma once

#include <driver_types.h>

#include "GeometryDefines.h"
#include "SceneDefines.h"
#include "BoundingBox.h"
#include "MathCommon.h"
#include "Ray.h"
#include "BottomBVH.h"

namespace YumeRT 
{
	__device__ __host__ inline  BBox3 GetMeshObjectBound(const GeometryData &geometry, const BottomNode *bottom_nodes)
	{
		assert(bottom_nodes != nullptr);
		return (bottom_nodes + geometry.tri_mesh.bottom_node_offset)[0].bbox;
	}

	__device__ __host__ inline  BBox3 GetSphereObjectBound(const GeometryData &geometry, const BottomNode *bottom_nodes)
	{
		float radius = geometry.sphere.radius;
		BBox3 result_bbox;
		result_bbox.p_min = glm::vec3(-radius);
		result_bbox.p_max = glm::vec3(radius);
		return result_bbox;
	}

	__device__ __host__ inline  BBox3 GetGeometryBound(const GeometryData &geometry, const BottomNode *bottom_nodes)
	{
		uint32_t geo_type = geometry.geometry_type;
		if (geo_type == GEOMETRY_TYPE::TRIANGLE_MESH)
		{
			return GetMeshObjectBound(geometry, bottom_nodes);
		}
		else if (geo_type == GEOMETRY_TYPE::SPHERE)
		{
			return GetSphereObjectBound(geometry, bottom_nodes);
		}
		else 
		{
			return BBox3();
		}
	}

	__device__ __host__ inline  glm::vec3 GetMeshObjectCenter(const GeometryData &geometry, const BottomNode *bottom_nodes)
	{
		assert(bottom_nodes != nullptr);
		return BBox3Center((bottom_nodes + geometry.tri_mesh.bottom_node_offset)[0].bbox);
	}

	__device__ __host__ inline  glm::vec3 GetSphereObjectCenter(const GeometryData &geometry, const BottomNode *bottom_nodes)
	{
		return glm::vec3(0.f);
	}

	__device__ __host__ inline  glm::vec3 GetGeometryCenter(const GeometryData &geometry, const BottomNode *bottom_nodes /* global data buffer */)
	{
		uint32_t geo_type = geometry.geometry_type;
		if (geo_type == GEOMETRY_TYPE::TRIANGLE_MESH)
		{
			return GetMeshObjectCenter(geometry, bottom_nodes);
		}
		else if (geo_type == GEOMETRY_TYPE::SPHERE)
		{
			return GetSphereObjectCenter(geometry, bottom_nodes);
		}
		else 
		{
			return glm::vec3(0.0f);
		}
	}

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

		constexpr float bbox_min_extent = 1E-3f;
		if (bbox.p_max.x <= bbox.p_min.x) { bbox.p_max.x = bbox.p_min.x + bbox_min_extent; }
		if (bbox.p_max.y <= bbox.p_min.y) { bbox.p_max.y = bbox.p_min.y + bbox_min_extent; }
		if (bbox.p_max.z <= bbox.p_min.z) { bbox.p_max.z = bbox.p_min.z + bbox_min_extent; }
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