#include "BottomBVH.h"
#include "TriangleDefines.h"

namespace YumeRT
{
	__host__ uint32_t FlattenRecursive(const std::vector<RawBottomNode>& raw_nodes, uint32_t raw_node_idx, BottomNode *bottom_nodes, uint32_t &node_count)
	{
		uint32_t i = (node_count++);
		const RawBottomNode &raw_node = raw_nodes[raw_node_idx];
		bottom_nodes[i].bbox = raw_node.bbox;
		if (raw_node.left == INVALID_UINT_32 || raw_node.right == INVALID_UINT_32)
		{
			bottom_nodes[i].leaf.offset = raw_node.offset;
			bottom_nodes[i].leaf.count = raw_node.count;
			return i;
		}

		FlattenRecursive(raw_nodes, raw_node.left, bottom_nodes, node_count);
		bottom_nodes[i].internal.left = INVALID_UINT_32;
		bottom_nodes[i].internal.right = FlattenRecursive(raw_nodes, raw_node.right, bottom_nodes, node_count);
		return i;
	}

	__host__ uint32_t Flatten(const std::vector<RawBottomNode>& raw_nodes, std::vector<BottomNode> &bottom_nodes)
	{
		uint32_t previous_size = bottom_nodes.size();
		bottom_nodes.resize(previous_size + raw_nodes.size());
		uint32_t node_count = 0;
		FlattenRecursive(raw_nodes, 0, bottom_nodes.data() + previous_size, node_count);
		bottom_nodes.resize(previous_size + node_count);
		return node_count;
	}

	__host__  std::vector<BottomNode> BottomBVHBuilder::BuildMeshBVH(float *time)
	{
		if (tri_count == 0)
		{
			*time = 0.0f;
			return std::vector<BottomNode>();
		}

		auto start_time = std::chrono::high_resolution_clock::now();

		std::vector<TriangleInfo> tri_infos(tri_count);
		concurrency::parallel_for((uint32_t)0, tri_count, [&](uint32_t i)->void
		{
			tri_infos[i].tri_idx = i;
			tri_infos[i].bbox = GetTriBound(triangles[i], positions, vidxs);
			tri_infos[i].center = GetTriCenter(triangles[i], positions, vidxs);
		});

		std::vector<bool> use_mid_split(2 * tri_count - 1, false);
		std::vector<RawBottomNode> raw_nodes(2 * tri_count - 1);
		raw_nodes[0].offset = 0;
		raw_nodes[0].count = tri_count;

		uint32_t start = 0;
		std::atomic<uint32_t> total_node_count(1);
		while (total_node_count.load() - start > 0)
		{
			uint32_t process_count = total_node_count.load() - start;
			concurrency::parallel_for(start, start + process_count, [&](uint32_t node_idx)
			{
				RawBottomNode &raw_node = raw_nodes[node_idx];

				TriangleInfo *node_tris = tri_infos.data() + raw_node.offset;
				BBox3 node_bbox;
				BBox3 center_bbox;

				for (uint32_t i = 0; i < raw_node.count; ++i)
				{
					node_bbox = BBox3Union(node_bbox, node_tris[i].bbox);
					center_bbox = BBox3Extend(center_bbox, node_tris[i].center);
				}

				raw_node.bbox = node_bbox;

				auto make_leaf = [&]()
				{
					raw_node.left = raw_node.right = INVALID_UINT_32;
				};

				auto make_mid_split = [&]()
				{
					int axis = BBox3LongestAxis(node_bbox);
					uint32_t mid_idx = FindMidElement(node_tris, raw_node.count, axis);

					uint32_t left_idx = total_node_count.fetch_add(2);
					uint32_t right_idx = left_idx + 1;

					raw_node.left = left_idx;
					raw_node.right = right_idx;

					raw_nodes[left_idx].offset = raw_node.offset;
					raw_nodes[left_idx].count = mid_idx + 1;
					use_mid_split[left_idx] = true;
					assert(raw_nodes[left_idx].count > 0);

					raw_nodes[right_idx].offset = raw_node.offset + mid_idx + 1;
					raw_nodes[right_idx].count = raw_node.count - 1 - mid_idx;
					use_mid_split[right_idx] = true;
					assert(raw_nodes[right_idx].count > 0);
				};

				if (raw_node.count <= 1)
				{
					make_leaf();
					return;
				}

				if (use_mid_split[node_idx])
				{
					make_mid_split();
					return;
				}

				const float parent_cost = raw_node.count * BBox3Area(raw_node.bbox);
				int split_axis;
				float split_pos;
				bool continue_split = EvalSAH(node_tris, raw_node.count, center_bbox, parent_cost, &split_pos, &split_axis);

				if (!continue_split)
				{
					make_mid_split();
					return;
				}

				uint32_t split_index = BVHNodePartition(node_tris, raw_node.count, split_pos, split_axis);

				if (split_index == INVALID_UINT_32 || split_index == raw_node.count - 1)
				{
					make_mid_split();
					return;
				}

				uint32_t left_idx = total_node_count.fetch_add(2);
				uint32_t right_idx = left_idx + 1;

				raw_node.left = left_idx;
				raw_node.right = right_idx;

				raw_nodes[left_idx].offset = raw_node.offset;
				raw_nodes[left_idx].count = split_index + 1;

				raw_nodes[right_idx].offset = raw_node.offset + split_index + 1;
				raw_nodes[right_idx].count = raw_node.count - 1 - split_index;
			});
			start += process_count;
		}

		raw_nodes.resize(total_node_count.load());

		std::vector<BottomNode> bottom_nodes;
		uint32_t bottom_node_count = Flatten(raw_nodes, bottom_nodes);

		std::vector<Triangle> temp_tris(tri_count);
		concurrency::parallel_for((uint32_t)0, (uint32_t)tri_count, [&](uint32_t i)
		{
			temp_tris[i] = triangles[tri_infos[i].tri_idx];
		});
		memcpy(triangles, temp_tris.data(), sizeof(Triangle) * tri_count);

		auto end_time = std::chrono::high_resolution_clock::now();
		if (time != nullptr) { *time = float(std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count()) / 1000.f; }
		return bottom_nodes;
	}


};