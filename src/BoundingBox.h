#pragma once

#include <driver_types.h>

#include "MathCommon.h"
#include "Ray.h"

namespace YumeRT
{
	struct BBox3
	{
		glm::vec3 p_max;
		glm::vec3 p_min;

		__device__ __host__ inline BBox3() : p_max(YumeRT_FLOAT_MIN), p_min(YumeRT_FLOAT_MAX) {}
		__device__ __host__ inline BBox3(const glm::vec3 &p) : p_max(p), p_min(p) {}
		__device__ __host__ inline BBox3(const glm::vec3 &p1, const glm::vec3 &p2) :
			p_max({ glm::max(p1.x, p2.x), glm::max(p1.y, p2.y), glm::max(p1.z, p2.z) }),
			p_min({ glm::min(p1.x, p2.x), glm::min(p1.y, p2.y), glm::min(p1.z, p2.z) }) {}
	};

	__device__ __host__ inline BBox3 BoundFix(const BBox3& bbox)
	{
		const float eps = 0.0005f;
		BBox3 object_bbox = bbox;
		for (int i = 0; i < 3; ++i)
		{
			if (object_bbox.p_max[i] < object_bbox.p_min[i])
			{
				Swap(object_bbox.p_max[i], object_bbox.p_min[i]);
			}
			if (fabs(object_bbox.p_max[i] - object_bbox.p_min[i]) < 2.0f * eps)
			{
				object_bbox.p_max[i] += eps;
				object_bbox.p_min[i] -= eps;
			}
		}
		return object_bbox;
	}

	__device__ __host__ inline BBox3 BBox3Extend(const BBox3 &bbox, const glm::vec3 &p)
	{
		BBox3 bbox_res;
		bbox_res.p_min.x = glm::min(bbox.p_min.x, p.x);
		bbox_res.p_min.y = glm::min(bbox.p_min.y, p.y);
		bbox_res.p_min.z = glm::min(bbox.p_min.z, p.z);
		bbox_res.p_max.x = glm::max(bbox.p_max.x, p.x);
		bbox_res.p_max.y = glm::max(bbox.p_max.y, p.y);
		bbox_res.p_max.z = glm::max(bbox.p_max.z, p.z);
		return bbox_res;
	}

	__device__ __host__ inline BBox3 BBox3Union(const BBox3 &bbox_1, const BBox3 &bbox_2)
	{
		BBox3 bbox_res;
		bbox_res.p_min.x = glm::min(bbox_1.p_min.x, bbox_2.p_min.x);
		bbox_res.p_min.y = glm::min(bbox_1.p_min.y, bbox_2.p_min.y);
		bbox_res.p_min.z = glm::min(bbox_1.p_min.z, bbox_2.p_min.z);
		bbox_res.p_max.x = glm::max(bbox_1.p_max.x, bbox_2.p_max.x);
		bbox_res.p_max.y = glm::max(bbox_1.p_max.y, bbox_2.p_max.y);
		bbox_res.p_max.z = glm::max(bbox_1.p_max.z, bbox_2.p_max.z);
		return bbox_res;
	}

	__device__ __host__ inline BBox3 BBox3Intersection(const BBox3 &bbox_1, const BBox3 &bbox_2)
	{
		BBox3 bbox_res;
		bbox_res.p_min.x = glm::max(bbox_1.p_min.x, bbox_2.p_min.x);
		bbox_res.p_min.y = glm::max(bbox_1.p_min.y, bbox_2.p_min.y);
		bbox_res.p_min.z = glm::max(bbox_1.p_min.z, bbox_2.p_min.z);
		bbox_res.p_max.x = glm::min(bbox_1.p_max.x, bbox_2.p_max.x);
		bbox_res.p_max.y = glm::min(bbox_1.p_max.y, bbox_2.p_max.y);
		bbox_res.p_max.z = glm::min(bbox_1.p_max.z, bbox_2.p_max.z);
		return bbox_res;
	}

	__device__ __host__ inline BBox3 BBox3Transform(const BBox3 &bbox, const glm::mat4 &transform)
	{
		BBox3 result_bbox;
		for (int i = 0; i < 8; ++i)
		{
			glm::vec3 corner((i & 1) ? bbox.p_max.x : bbox.p_min.x,
										(i & 2) ? bbox.p_max.y : bbox.p_min.y,
										(i & 4) ? bbox.p_max.z : bbox.p_min.z);
			result_bbox = BBox3Extend(result_bbox, glm::vec3(transform * glm::vec4(corner, 1.0f)));
		}

		return BoundFix(result_bbox);
	}

	__device__ __host__ inline bool BBox3Overlap(const BBox3 &bbox_1, const BBox3 &bbox_2)
	{
		bool x = (bbox_1.p_max.x >= bbox_2.p_min.x) && (bbox_1.p_min.x <= bbox_2.p_max.x);
		bool y = (bbox_1.p_max.y >= bbox_2.p_min.y) && (bbox_1.p_min.y <= bbox_2.p_max.y);
		bool z = (bbox_1.p_max.z >= bbox_2.p_min.z) && (bbox_1.p_min.z <= bbox_2.p_max.z);
		return x && y && z;
	}

	__device__ __host__ inline int BBox3LongestAxis(const BBox3 &bbox)
	{
		glm::vec3 diagonal = bbox.p_max - bbox.p_min;
		int max_axis = 0;
		if (diagonal[1] > diagonal[max_axis]) { max_axis = 1; }
		if (diagonal[2] > diagonal[max_axis]) { max_axis = 2; }
		return max_axis;
	}

	__device__ __host__ inline float BBox3Area(const BBox3 &bbox)
	{
		glm::vec3 extent = bbox.p_max - bbox.p_min;
		return 2.f * (extent.x * extent.y + extent.y * extent.z + extent.x * extent.z);
	}

	__device__ __host__ inline glm::vec3 BBox3Center(const BBox3 &bbox)
	{
		return (bbox.p_max + bbox.p_min) * 0.5f;
	}

	__device__ __host__ inline bool IntersectBBox3(const BBox3 &bbox, const Ray &ray, float *t = nullptr)
	{
		glm::vec3 inv_dir = glm::vec3(1.f) / ray.direction;

		float tx0 = (bbox.p_min.x - ray.origin.x) * inv_dir.x;
		float tx1 = (bbox.p_max.x - ray.origin.x) * inv_dir.x;
		if (tx0 > tx1) { Swap(tx0, tx1); }

		float ty0 = (bbox.p_min.y - ray.origin.y) * inv_dir.y;
		float ty1 = (bbox.p_max.y - ray.origin.y) * inv_dir.y;
		if (ty0 > ty1) { Swap(ty0, ty1); }

		float tz0 = (bbox.p_min.z - ray.origin.z) * inv_dir.z;
		float tz1 = (bbox.p_max.z - ray.origin.z) * inv_dir.z;
		if (tz0 > tz1) { Swap(tz0, tz1); }
		// if (isnan(tz0) || isnan(tz1)) { return false; }

		float ti = tx0;
		if (ty0 > ti) { ti = ty0; }
		if (tz0 > ti) { ti = tz0; }

		float to = tx1;
		if (ty1 < to) { to = ty1; }
		if (tz1 < to) { to = tz1; }

		bool hit = (to > ti) && (to > 0.0f) && (ti <= ray.t);
		if (t != nullptr) { *t = hit ? ti : TMAX; }
		return hit;
	}
};