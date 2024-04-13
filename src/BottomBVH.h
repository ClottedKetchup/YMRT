#pragma once

#include <vector>
#include <chrono>
#include <ppl.h>
#include <atomic>

#include "SceneDefines.h"
#include "MathCommon.h"
#include "BoundingBox.h"
#include "PrimtiveInstance.h"

namespace YumeRT 
{
	struct Triangle;
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
		if (BBox3Area(center_bbox) <= 0.0f) { return false; }

		// check all axis
		int best_axis = -1;
		float best_pos = 0.0f;
		float best_cost = 1E36f;

		struct Bin { uint32_t count = 0; BBox3 bbox; };
		const int bin_count = 16;

		Bin bins[bin_count][3];
		const glm::vec3 i_extent = glm::vec3(1.0f) / (center_bbox.p_max - center_bbox.p_min);
		const glm::vec3 bin_length = (center_bbox.p_max - center_bbox.p_min) / float(bin_count);
		for (uint32_t idx = 0; idx < count; ++idx)
		{
			const BBox3 prim_bbox = prims[idx].GetBBox();
			const glm::vec3 prim_ndc = (prims[idx].GetCenter() - center_bbox.p_min) * i_extent;

			int bin_idx_x = glm::max(glm::min((int)(prim_ndc.x * bin_count), bin_count - 1), 0);
			bins[bin_idx_x][0].count++;
			bins[bin_idx_x][0].bbox = BBox3Union(bins[bin_idx_x][0].bbox, prim_bbox);

			int bin_idx_y = glm::max(glm::min((int)(prim_ndc.y * bin_count), bin_count - 1), 0);
			bins[bin_idx_y][1].count++;
			bins[bin_idx_y][1].bbox = BBox3Union(bins[bin_idx_y][1].bbox, prim_bbox);

			int bin_idx_z = glm::max(glm::min((int)(prim_ndc.z * bin_count), bin_count - 1), 0);
			bins[bin_idx_z][2].count++;
			bins[bin_idx_z][2].bbox = BBox3Union(bins[bin_idx_z][2].bbox, prim_bbox);
		}

		float left_areas[bin_count - 1][3], right_areas[bin_count - 1][3];
		uint32_t left_counts[bin_count - 1][3], right_counts[bin_count - 1][3];
		for (int axis = 0; axis < 3; ++axis)
		{
			if ((center_bbox.p_max[axis] - center_bbox.p_min[axis]) < 1E-8f) { continue; }

			BBox3 left_bbox, right_bbox;
			uint32_t left_count = 0, right_count = 0;

			for (uint32_t i = 0; i < bin_count - 1; ++i)
			{
				left_count += bins[i][axis].count;
				left_bbox = BBox3Union(left_bbox, bins[i][axis].bbox);
				left_counts[i][axis] = left_count;
				left_areas[i][axis] = BBox3Area(left_bbox);

				right_count += bins[bin_count - 1 - i][axis].count;
				right_bbox = BBox3Union(right_bbox, bins[bin_count - 1 - i][axis].bbox);
				right_counts[bin_count - 2 - i][axis] = right_count;
				right_areas[bin_count - 2 - i][axis] = BBox3Area(right_bbox);
			}

			for (int i = 0; i < bin_count - 1; ++i)
			{
				float cost = left_counts[i][axis] * left_areas[i][axis] + right_counts[i][axis] * right_areas[i][axis];
				if (cost <= 0.0f) { continue; }
				if (cost < best_cost)
				{
					best_cost = cost;
					best_axis = axis;
					best_pos = center_bbox.p_min[axis] + float(i + 1) * bin_length[axis];
				}
			}
		}

		if (best_axis == -1 || best_cost >= parent_cost) { return false; }

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
		__host__ inline BottomBVHBuilder(Triangle *triangles,
			uint32_t tri_count,
			glm::vec3 *positions,
			uint32_t *vidxs)
			:triangles(triangles), tri_count(tri_count), vidxs(vidxs), positions(positions) {}

		__host__  std::vector<BottomNode> BuildMeshBVH(float *time = nullptr);
	private:
		Triangle *triangles;
		uint32_t tri_count;
		glm::vec3 *positions;
		uint32_t *vidxs;
	};
};