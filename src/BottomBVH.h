#pragma once

#include <vector>
#include <chrono>
#include <ppl.h>
#include <atomic>

#include "SceneDefines.h"
#include "MathCommon.h"
#include "BoundingBox.h"
#include "GeometryDefines.h"
#include "PrimtiveInstance.h"

namespace YumeRT 
{
	struct RawBottomNode
	{
		BBox3 bbox;
		uint32_t left, right;
		uint32_t offset, count;
	};
	struct BottomNode
	{
		BBox3 bbox;
		union
		{
			struct { uint32_t left, right; } internal;
			struct { uint32_t offset, count; } leaf;
		};
	};
	struct TriangleInfo
	{
		BBox3 bbox;
		glm::vec3 center;
		uint32_t tri_idx;

		inline glm::vec3 GetCenter() { return center; }
		inline BBox3 GetBBox() { return bbox; }
	};

	template<typename T>
	__host__  inline  uint32_t BVHNodePartition(T *types /* current node's */, uint32_t count, float pivot, int axis)
	{
		assert(count > 0);
		uint32_t left = 0, right = count - 1;
		while (left <= right && right != INVALID_UINT_32)
		{
			if (types[left].GetCenter()[axis] <= pivot) { ++left; }
			else { Swap(types[left], types[right--]); }
		}
		return right;
	}

	template<typename T>
	__host__ inline  bool EvalSAH(T *prims, uint32_t count, const BBox3 &center_bbox, const float parent_cost, float *split_pos, int *split_axis)
	{
		assert(count > 0);

		int best_axis = 0;
		float best_pos = prims[0].GetCenter()[best_axis];
		float best_cost = 1E33f;
		glm::vec3 extent = center_bbox.p_max - center_bbox.p_min;

		struct Bin { uint32_t count = 0; BBox3 bbox; };
		const int bin_count = 16;
		int axis = BBox3LongestAxis(center_bbox);

		Bin bins[bin_count];
		const float step = extent[axis] / float(bin_count);
		const float inv_len = 1.f / extent[axis];
		for (uint32_t idx = 0; idx < count; ++idx)
		{
			float prim_ndc = (prims[idx].GetCenter()[axis] - center_bbox.p_min[axis]) * inv_len;
			int bin_idx = glm::max(glm::min((int)(prim_ndc * bin_count), bin_count - 1), 0);
			bins[bin_idx].count++;
			bins[bin_idx].bbox = BBox3Union(bins[bin_idx].bbox, prims[idx].GetBBox());
		}

		float left_areas[bin_count - 1], right_areas[bin_count - 1];
		uint32_t left_counts[bin_count - 1], right_counts[bin_count - 1];

		BBox3 left_bbox, right_bbox;
		uint32_t left_count = 0, right_count = 0;
		for (uint32_t i = 0; i < bin_count - 1; ++i)
		{
			left_count += bins[i].count;
			left_bbox = BBox3Union(left_bbox, bins[i].bbox);
			left_counts[i] = left_count;
			left_areas[i] = BBox3Area(left_bbox);

			right_count += bins[bin_count - 1 - i].count;
			right_bbox = BBox3Union(right_bbox, bins[bin_count - 1 - i].bbox);
			right_counts[bin_count - 2 - i] = right_count;
			right_areas[bin_count - 2 - i] = BBox3Area(right_bbox);
		}

		for (int i = 0; i < bin_count - 1; ++i)
		{
			float cost = left_counts[i] * left_areas[i] + right_counts[i] * right_areas[i];
			if (cost <= 0.0f) { continue; }
			if (cost < best_cost)
			{
				best_cost = cost;
				best_axis = axis;
				best_pos = center_bbox.p_min[axis] + float(i + 1) * step;
			}
		}

		if (best_cost >= parent_cost)
		{
			return false;
		}

		*split_axis = best_axis;
		*split_pos = best_pos;
		return true;
	}

	template<typename T>
	__host__ inline void HeapDown(T *types, uint32_t i, uint32_t right, int axis)
	{
		uint32_t left_child = (2 * i) + 1, right_child = (2 * i) + 2;
		if (!(left_child <= right)) { return; }
		uint32_t swap_target = left_child;
		if (right_child <= right && types[right_child].GetCenter()[axis] > types[left_child].GetCenter()[axis]) { swap_target = right_child; }
		if (types[i].GetCenter()[axis] > types[swap_target].GetCenter()[axis]) { return; }
		Swap(types[i], types[swap_target]);
		HeapDown(types, swap_target, right, axis);
	}

	template<typename T>
	__host__ inline uint32_t FindMidElement(T *types, uint32_t count, int axis) /* [0..n_th] [n_th + 1..count - 1] */
	{
		uint32_t n_th = (count - 1) / 2;
		uint32_t n_pass = count - n_th;
		for (uint32_t i = count / 2 - 1; i >= 0 && i != INVALID_UINT_32; --i)
		{
			HeapDown(types, i, count - 1, axis);
		}
		for (uint32_t i = count - 1; i > 0 && n_pass > 0; --i, --n_pass)
		{
			Swap(types[0], types[i]);
			HeapDown(types, 0, i - 1, axis);
		}
		return n_th;
	}

	__host__ uint32_t FlattenRecursive(const std::vector<RawBottomNode>& raw_nodes, uint32_t raw_node_idx, BottomNode *bottom_nodes, uint32_t &node_count);

	__host__ uint32_t Flatten(const std::vector<RawBottomNode>& raw_nodes, std::vector<BottomNode> &bottom_nodes);

	// TODO: should use center bbox to perform split
	class BottomBVHBuilder
	{
	public:
		__host__ BottomBVHBuilder(GeometryData *tri_mesh,
													Triangle *triangles,
													uint32_t tri_count,
													glm::vec3 *positions,
													uint32_t *vidxs)
			:tri_mesh(tri_mesh), triangles(triangles), tri_count(tri_count), vidxs(vidxs), positions(positions){}

		__host__  uint32_t BuildMeshBVH(std::vector<BottomNode> &bottom_nodes, float *time);
	private:
		GeometryData *tri_mesh;
		Triangle *triangles;
		uint32_t tri_count;
		glm::vec3 *positions;
		uint32_t *vidxs;
	};

	struct RawTopNode
	{
		BBox3 bbox;
		uint32_t left, right;
		uint32_t offset, count;
	};
	struct TopNode
	{
		BBox3 bbox;
		union
		{
			struct { uint32_t left, right; } internal;
			struct { uint32_t offset, count; } leaf;
		};
	};
	struct PrimInstanceInfo
	{
		BBox3 bbox;
		uint32_t idx;

		inline glm::vec3 GetCenter() { return BBox3Center(bbox); }
		inline BBox3 GetBBox() { return bbox; }
	};

	__host__ uint32_t FlattenRecursive(const std::vector<RawTopNode>& raw_nodes, uint32_t raw_node_idx, TopNode *top_nodes, uint32_t &node_count);

	__host__ uint32_t Flatten(const std::vector<RawTopNode>& raw_nodes, std::vector<TopNode> &top_nodes);

	class TopBVHBuilder
	{
	public:
		__host__ TopBVHBuilder(PrimitiveInstance *prim_instances,
												uint32_t prim_instance_count,
												glm::mat4 *transforms,
												GeometryData *geometries,
												BottomNode *bottom_nodes)
			:prim_instances(prim_instances), prim_instance_count(prim_instance_count), transforms(transforms),geometries(geometries), bottom_nodes(bottom_nodes){}

		__host__  uint32_t BuildSceneBVH(std::vector<TopNode> &top_nodes, float *time);
	private:
		PrimitiveInstance *prim_instances;
		uint32_t prim_instance_count;
		glm::mat4 *transforms;
		GeometryData *geometries;
		BottomNode *bottom_nodes;
	};

	struct ACCluster 
	{
		uint32_t node_idx;
	};

	__host__ uint32_t FlattenACRecursive(const std::vector<RawTopNode>& raw_nodes,
																uint32_t root_idx,
																TopNode *top_nodes,
																uint32_t &node_count,
																uint32_t *dst_prims,
																uint32_t &prim_count);

	__host__ uint32_t FlattenAC(const std::vector<RawTopNode>& raw_nodes,
												uint32_t root_idx,
												std::vector<TopNode> &top_nodes,
												uint32_t *dst_prims);

	class ACBVHBuilder
	{
	public:
		__host__ ACBVHBuilder(PrimitiveInstance *prim_instances,
											  uint32_t prim_instance_count,
											  glm::mat4 *transforms,
											  GeometryData *geometries,
											  BottomNode *bottom_nodes)
			:prim_instances(prim_instances), prim_instance_count(prim_instance_count), transforms(transforms), geometries(geometries), bottom_nodes(bottom_nodes) {}

		__host__  uint32_t BuildSceneBVH(std::vector<TopNode> &top_nodes, float *time, uint32_t *highlight_prim_idx = nullptr);
	private:
		PrimitiveInstance *prim_instances;
		uint32_t prim_instance_count;
		glm::mat4 *transforms;
		GeometryData *geometries;
		BottomNode *bottom_nodes;

		__host__ uint32_t FindBestMatch(const std::vector<ACCluster> &clusters,
															const std::vector<RawTopNode> &raw_nodes,
															uint32_t idx,
															uint32_t cluster_count);
	};
};