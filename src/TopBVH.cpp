#include "TopBVH.h"
#include "GeometryUtilities.h"

namespace YumeRT
{
	__host__ uint32_t FlattenRecursive(const std::vector<RawTopNode>& raw_nodes, uint32_t raw_node_idx, TopNode *top_nodes, uint32_t &node_count)
	{
		uint32_t i = (node_count++);
		const RawTopNode &raw_node = raw_nodes[raw_node_idx];
		top_nodes[i].bbox = raw_node.bbox;
		if (raw_node.left == EMPTY_UINT32 || raw_node.right == EMPTY_UINT32)
		{
			top_nodes[i].leaf.offset = raw_node.offset;
			top_nodes[i].leaf.count = raw_node.count;
			return i;
		}

		FlattenRecursive(raw_nodes, raw_node.left, top_nodes, node_count);
		top_nodes[i].internal.left = EMPTY_UINT32;
		top_nodes[i].internal.right = FlattenRecursive(raw_nodes, raw_node.right, top_nodes, node_count);
		return i;
	}

	__host__ uint32_t Flatten(const std::vector<RawTopNode>& raw_nodes, std::vector<TopNode> &top_nodes)
	{
		uint32_t previous_size = top_nodes.size();
		top_nodes.resize(previous_size + raw_nodes.size());
		uint32_t node_count = 0;
		FlattenRecursive(raw_nodes, 0, top_nodes.data() + previous_size, node_count);
		top_nodes.resize(previous_size + node_count);
		return node_count;
	}

	__host__  uint32_t TopBVHBuilder::BuildSceneBVH(std::vector<TopNode>& top_nodes, float* time, uint32_t* highlight_prim_idx, std::unordered_map<uint32_t, uint32_t>* dst_indices_map)
	{
		if (prim_instance_count == 0)
		{
			if (time != nullptr) {
				*time = 0.0f;
			}
			if (highlight_prim_idx != nullptr) {
				*highlight_prim_idx = EMPTY_UINT32;
			}
			return 0;
		}

		auto start_time = std::chrono::high_resolution_clock::now();

		std::vector<PrimInstanceInfo> prim_instance_infos(prim_instance_count);
		concurrency::parallel_for((uint32_t)0, prim_instance_count, [&](uint32_t i)->void
			{
				prim_instance_infos[i].idx = i;
				const glm::mat4 &transform = transforms[prim_instances[i].transform_idx];
				const GeometryData &geometry = geometries[prim_instances[i].geometry_idx];
				prim_instance_infos[i].bbox = BBox3Transform(GetGeometryBound(geometry), transform);
			});

		std::vector<bool> use_mid_split(2 * prim_instance_count - 1, false);
		std::vector<RawTopNode> raw_nodes(2 * prim_instance_count - 1);
		raw_nodes[0].offset = 0;
		raw_nodes[0].count = prim_instance_count;

		const int single_thread_max_count = 256;
		uint32_t start = 0;
		std::atomic<uint32_t> total_node_count(1);
		while (total_node_count.load() - start > 0)
		{
			uint32_t process_count = total_node_count.load() - start;
			auto process_function = [&](uint32_t node_idx)
			{
				RawTopNode& raw_node = raw_nodes[node_idx];

				PrimInstanceInfo* node_prims = prim_instance_infos.data() + raw_node.offset;
				BBox3 node_bbox;
				BBox3 center_bbox;
				for (uint32_t i = 0; i < raw_node.count; ++i)
				{
					node_bbox = BBox3Union(node_bbox, node_prims[i].bbox);
					center_bbox = BBox3Extend(center_bbox, node_prims[i].GetCenter());
				}

				raw_node.bbox = node_bbox;

				auto make_leaf = [&]()
				{
					raw_node.left = raw_node.right = EMPTY_UINT32;
				};

				auto make_mid_split = [&]()
				{
					int axis = BBox3LongestAxis(node_bbox);
					uint32_t mid_idx = FindMidElement(node_prims, raw_node.count, axis);

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
				bool continue_split = EvalSAH(node_prims, raw_node.count, center_bbox, parent_cost, &split_pos, &split_axis);

				if (!continue_split)
				{
					make_mid_split();
					return;
				}

				uint32_t split_index = BVHNodePartition(node_prims, raw_node.count, split_pos, split_axis);

				if (split_index == EMPTY_UINT32 || split_index == raw_node.count - 1)
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
			};
			
			if (process_count < single_thread_max_count) {
				for (uint32_t node_idx = start; node_idx < start + process_count; ++node_idx) {
					process_function(node_idx);
				}
			}
			else {
				concurrency::parallel_for(start, start + process_count, process_function);
			}

			start += process_count;
		}

		raw_nodes.resize(total_node_count.load());

		uint32_t top_node_count = Flatten(raw_nodes, top_nodes);
		
		if (dst_indices_map != nullptr) {
			auto& index_to_index_map = *dst_indices_map;
			for (uint32_t index = 0; index < prim_instance_count; ++index) {
				index_to_index_map[prim_instance_infos[index].idx] = index;
			}
		}

		std::vector<PrimitiveInstance> temp_prims(prim_instance_count);
		const int single_thread_copy_limit = 1024;
		auto copy_function = [&](uint32_t index) {
			temp_prims[index] = prim_instances[prim_instance_infos[index].idx];
		};
		if (prim_instance_count < single_thread_copy_limit) {
			for (uint32_t index = 0; index < prim_instance_count; ++index) {
				copy_function(index);
			}
		}
		else {
			concurrency::parallel_for((uint32_t)0, (uint32_t)prim_instance_count, copy_function);
		}
		memcpy(prim_instances, temp_prims.data(), sizeof(PrimitiveInstance) * prim_instance_count);

		auto end_time = std::chrono::high_resolution_clock::now();
		if (time != nullptr) {
			*time = float(std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count()) / 1000.f;
		}
		return 0;
	}

	__host__ uint32_t FlattenACRecursive(const std::vector<RawTopNode>& raw_nodes,
		uint32_t root_idx,
		TopNode *top_nodes,
		uint32_t &node_count,
		uint32_t *dst_prims,
		uint32_t &prim_count)
	{
		uint32_t node_idx = (node_count++);
		const RawTopNode &raw_node = raw_nodes[root_idx];
		top_nodes[node_idx].bbox = raw_node.bbox;
		if (raw_node.left == EMPTY_UINT32 || raw_node.right == EMPTY_UINT32)
		{
			assert(raw_node.count == 1);
			top_nodes[node_idx].leaf.count = raw_node.count;
			top_nodes[node_idx].leaf.offset = prim_count;
			dst_prims[raw_node.offset] = prim_count++;
			return node_idx;
		}

		top_nodes[node_idx].internal.left = FlattenACRecursive(raw_nodes, raw_node.left, top_nodes, node_count, dst_prims, prim_count);
		top_nodes[node_idx].internal.right = FlattenACRecursive(raw_nodes, raw_node.right, top_nodes, node_count, dst_prims, prim_count);
		top_nodes[node_idx].internal.left = EMPTY_UINT32;
		return node_idx;
	}

	__host__ uint32_t FlattenAC(const std::vector<RawTopNode>& raw_nodes,
		uint32_t root_idx,
		std::vector<TopNode> &top_nodes,
		uint32_t *dst_prims)
	{
		uint32_t previous_size = top_nodes.size();
		top_nodes.resize(previous_size + raw_nodes.size());
		uint32_t node_count = 0;
		uint32_t prim_count = 0;

		assert(dst_prims != nullptr);
		FlattenACRecursive(raw_nodes, root_idx, top_nodes.data() + previous_size, node_count, dst_prims, prim_count);

		top_nodes.resize(previous_size + node_count);
		return node_count;
	}

	__host__ uint32_t ACBVHBuilder::BuildSceneBVH(std::vector<TopNode> &top_nodes, float *time, uint32_t *highlight_prim_idx, std::unordered_map<uint32_t, uint32_t>* dst_indices_map)
	{
		// consider when prim_instance_count == 0...
		if (prim_instance_count == 0)
		{
			if (time != nullptr) { 
				*time = 0.0f; 
			}
			if (highlight_prim_idx != nullptr) {
				*highlight_prim_idx = EMPTY_UINT32;
			}
			return 0;
		}

		auto start_time = std::chrono::high_resolution_clock::now();

		std::vector<ACCluster> clusters(prim_instance_count);
		std::vector<RawTopNode> raw_nodes(2 * prim_instance_count - 1);
		concurrency::parallel_for((uint32_t)0, prim_instance_count, [&](uint32_t i)
			{
				const glm::mat4 &transform = transforms[prim_instances[i].transform_idx];
				const GeometryData &geometry = geometries[prim_instances[i].geometry_idx];

				raw_nodes[i].bbox = BBox3Transform(GetGeometryBound(geometry), transform);
				raw_nodes[i].left = raw_nodes[i].right = EMPTY_UINT32;
				raw_nodes[i].offset = i;
				raw_nodes[i].count = 1;

				clusters[i].node_idx = i;
			});

		uint32_t total_node_count = prim_instance_count;
		uint32_t cluster_count = prim_instance_count;

		uint32_t a = 0;
		uint32_t b = FindBestMatch(clusters, raw_nodes, a, cluster_count);
		while (cluster_count > 1)
		{
			uint32_t c = FindBestMatch(clusters, raw_nodes, b, cluster_count);
			if (a == c)
			{
				raw_nodes[total_node_count].bbox = BBox3Union(raw_nodes[clusters[a].node_idx].bbox, raw_nodes[clusters[b].node_idx].bbox);
				raw_nodes[total_node_count].left = clusters[a].node_idx;
				raw_nodes[total_node_count].right = clusters[b].node_idx;

				clusters[a].node_idx = total_node_count++;
				clusters[b].node_idx = clusters[cluster_count - 1].node_idx;
				--cluster_count;

				b = FindBestMatch(clusters, raw_nodes, a, cluster_count);
			}
			else
			{
				a = b;
				b = c;
			}
		}

		std::vector<uint32_t> dst_prims_interleave(prim_instance_count);
		uint32_t node_count = FlattenAC(raw_nodes, total_node_count - 1, top_nodes, dst_prims_interleave.data());
		assert(node_count == total_node_count);

		if (dst_indices_map != nullptr) {
			auto& index_to_index_map = *dst_indices_map;
			for (uint32_t index = 0; index < prim_instance_count; ++index) {
				index_to_index_map[index] = dst_prims_interleave[index];
			}
		}

		std::vector<PrimitiveInstance> temp_prims(prim_instance_count);
		concurrency::parallel_for((uint32_t)0, prim_instance_count, [&](uint32_t i)
			{
				temp_prims[dst_prims_interleave[i]] = prim_instances[i];
			});
		memcpy(prim_instances, temp_prims.data(), sizeof(PrimitiveInstance) * prim_instance_count);

		if (highlight_prim_idx != nullptr && (*highlight_prim_idx) != EMPTY_UINT32)
		{
			assert((*highlight_prim_idx) >= 0 && (*highlight_prim_idx) < prim_instance_count);
			*highlight_prim_idx = dst_prims_interleave[*highlight_prim_idx];
		}

		auto end_time = std::chrono::high_resolution_clock::now();
		if (time != nullptr) {
			*time = float(std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count()) / 1000.f;
		}
		return 0;
	}

	__host__ uint32_t ACBVHBuilder::FindBestMatch(const std::vector<ACCluster> &clusters,
		const std::vector<RawTopNode> &raw_nodes,
		uint32_t idx,
		uint32_t cluster_count)
	{
		auto cost_function = [&](uint32_t a, uint32_t b)->float
		{
			const BBox3 &bbox_a = raw_nodes[clusters[a].node_idx].bbox;
			const BBox3 &bbox_b = raw_nodes[clusters[b].node_idx].bbox;
			return BBox3Area(BBox3Union(bbox_a, bbox_b));
		};
		float min_cost = YumeRT_FLOAT_MAX;
		uint32_t candidate = EMPTY_UINT32;
		for (uint32_t i = 0; i < cluster_count; ++i)
		{
			if (i == idx) { continue; }
			float cost = cost_function(idx, i);
			if (cost < min_cost)
			{
				min_cost = cost;
				candidate = i;
			}
		}
		return candidate;
	}
};