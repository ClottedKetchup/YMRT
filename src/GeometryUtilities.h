#pragma once

#include <driver_types.h>

#include "GeometryDefines.h"
#include "SceneDefines.h"
#include "BoundingBox.h"
#include "MathCommon.h"
#include "BottomBVH.h"

namespace YMRT 
{
	__host__ inline  BBox3 GetMeshObjectBound(const TriangleMesh &triangle_mesh)
	{
		BBox3 *p_bbox = triangle_mesh.GetBoundingBoxHost();
		return p_bbox != nullptr ? *p_bbox : BBox3();
	}

	__host__ inline  BBox3 GetSphereObjectBound(const Sphere &sphere)
	{
		BBox3 result_bbox;
		result_bbox.p_min = glm::vec3(-sphere.radius);
		result_bbox.p_max = glm::vec3(sphere.radius);
		return BoundFix(result_bbox);
	}

	__host__ inline  BBox3 GetGeometryBound(const GeometryData &geometry)
	{
		uint32_t geo_type = geometry.geometry_type;
		BBox3 object_bbox;
		if (geo_type == GEOMETRY_TYPE::TRIANGLE_MESH)
		{
			object_bbox = GetMeshObjectBound(geometry.triangle_mesh);
		}
		else if (geo_type == GEOMETRY_TYPE::SPHERE)
		{
			object_bbox = GetSphereObjectBound(geometry.sphere);
		}
		else 
		{
			object_bbox = BBox3();
		}

		return object_bbox;
	}

	__host__ inline  glm::vec3 GetMeshObjectCenter(const TriangleMesh &triangle_mesh)
	{
		BBox3 *p_bbox = triangle_mesh.GetBoundingBoxHost();
		return BBox3Center(p_bbox != nullptr ? *p_bbox : BBox3());
	}

	__host__ inline  glm::vec3 GetSphereObjectCenter(const Sphere &sphere)
	{
		return glm::vec3(0.f);
	}

	__host__ inline  glm::vec3 GetGeometryCenter(const GeometryData &geometry)
	{
		uint32_t geo_type = geometry.geometry_type;
		if (geo_type == GEOMETRY_TYPE::TRIANGLE_MESH)
		{
			return GetMeshObjectCenter(geometry.triangle_mesh);
		}
		else if (geo_type == GEOMETRY_TYPE::SPHERE)
		{
			return GetSphereObjectCenter(geometry.sphere);
		}
		else 
		{
			return glm::vec3(0.0f);
		}
	}
};