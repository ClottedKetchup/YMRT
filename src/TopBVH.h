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

namespace YMRT 
{
	struct RawTopNode
	{
		BBox3 bbox;
		uint32_t left, right;
		uint32_t offset, count;
	};
	struct TopNode
	{
		// TODO: can we use some thing to store if this treelet contain volume?
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
		__host__ inline TopBVHBuilder(PrimitiveInstance *prim_instances,
			uint32_t prim_instance_count,
			glm::mat4 *transforms,
			GeometryData *geometries)
			:prim_instances(prim_instances), prim_instance_count(prim_instance_count), transforms(transforms), geometries(geometries){}

		__host__  uint32_t BuildSceneBVH(std::vector<TopNode>& top_nodes, float* time, uint32_t* highlight_prim_idx = nullptr, std::unordered_map<uint32_t, uint32_t>* dst_indices_map = nullptr);
	private:
		PrimitiveInstance *prim_instances;
		uint32_t prim_instance_count;
		glm::mat4 *transforms;
		GeometryData *geometries;
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
		__host__ inline ACBVHBuilder(PrimitiveInstance *prim_instances,
			uint32_t prim_instance_count,
			glm::mat4 *transforms,
			GeometryData *geometries)
			:prim_instances(prim_instances), prim_instance_count(prim_instance_count), transforms(transforms), geometries(geometries){}

		__host__  uint32_t BuildSceneBVH(std::vector<TopNode>& top_nodes, float* time, uint32_t* highlight_prim_idx = nullptr, std::unordered_map<uint32_t, uint32_t>* dst_indices_map = nullptr);
	private:
		PrimitiveInstance *prim_instances;
		uint32_t prim_instance_count;
		glm::mat4 *transforms;
		GeometryData *geometries;

		__host__ uint32_t FindBestMatch(const std::vector<ACCluster> &clusters,
			const std::vector<RawTopNode> &raw_nodes,
			uint32_t idx,
			uint32_t cluster_count);
	};
};