#pragma once

#include <driver_types.h>

#include "MathCommon.h"

namespace YumeRT
{
#define	TMAX 1E36f
#define	TMIN  1E-7f
#define    DEFAULT_IOR 1.0f
#define	MAX_BOUNDARY_RECORD 4

	struct Ray 
	{
		//TODO: ior
		glm::vec3 origin;
		glm::vec3 direction;
		mutable float t;
		int volume_idx;
		
		float ray_ior;

		__device__ __host__ inline Ray() : t(TMAX), volume_idx(-1), origin(0.0f), direction(0.0f), ray_ior(1.0f){};

		__device__ __host__ inline Ray(const glm::vec3 &origin, const glm::vec3 &direction, float ray_ior = 1.0f, int volume_idx = -1)
			:origin(origin), direction(glm::normalize(direction)), t(TMAX), ray_ior(ray_ior), volume_idx(volume_idx) {}

		__device__ __host__ inline glm::vec3 PositionAtT(float time)
		{
			return origin + direction * time;
		}
	};

	__device__ __host__ inline Ray TransformRay(const Ray &ray, const glm::mat4 &transform)
	{
		Ray transformed_ray;
		transformed_ray.origin = glm::vec3(transform * glm::vec4(ray.origin, 1.0f));
		transformed_ray.direction = glm::vec3(transform * glm::vec4(ray.direction, 0.0f));
		transformed_ray.t = ray.t;
		transformed_ray.volume_idx = ray.volume_idx;
		transformed_ray.ray_ior = ray.ray_ior;
		return transformed_ray;
	}

	struct RayDifferential 
	{
		glm::vec3 origin_x;
		glm::vec3 direction_x;
		glm::vec3 origin_y;
		glm::vec3 direction_y;

		float scale_x;
		float scale_y;

		__device__ __host__ inline RayDifferential(): scale_x(1.0f), scale_y(1.0f) {}
		__device__ __host__ inline void SetInvalid() 
		{
			auto set_bits = [](float *fp)->void {(*(uint32_t*)(fp)) = EMPTY_UINT32; };
			set_bits(&scale_x);
			set_bits(&scale_y);
		}
		__device__ __host__ bool inline IsValid() const
		{
			auto get_bits = [](const float *fp)->uint32_t {return (*(uint32_t*)(fp)); };
			return get_bits(&scale_x) == EMPTY_UINT32 &&
				get_bits(&scale_y) == EMPTY_UINT32;
		}
	};


	struct BoundaryRecord 
	{
		uint32_t prim_idx;
		float ior;
		uint32_t ior_priority;
		int vol_idx;

		__device__ __host__ inline BoundaryRecord() :prim_idx(EMPTY_UINT32), ior(1.0f), ior_priority(0), vol_idx(-1) {}
	};

	struct RayTransfer 
	{
		int max_priority_record_index = -1;
		int record_count = 0;

		BoundaryRecord boundary_records[MAX_BOUNDARY_RECORD];
		
		__device__ __host__ inline RayTransfer() : max_priority_record_index(-1), record_count(0) {}

		__device__ __host__ inline const BoundaryRecord& GetMaxPriorityRecord()const
		{
			return max_priority_record_index != -1? boundary_records[max_priority_record_index] : BoundaryRecord();
		}
		__device__ __host__ inline bool empty() const 
		{ 
			return record_count == 0; 
		}
		__device__ __host__ inline int FindRecordWithPrim(uint32_t prim_idx) const 
		{
			for (int record_idx = record_count - 1; record_idx >= 0; --record_idx) {
				if (boundary_records[record_idx].prim_idx == prim_idx) {
					return record_idx;
				}
			}
			return -1;
		}
		__device__ __host__ inline int FindMaxPriorityRecord() const
		{
			if (record_count == 0) {
				return -1;
			}
			uint32_t max_priority = boundary_records[0].ior_priority;
			int max_priority_record_idx = 0;
			for (int record_idx = 1; record_idx < record_count; ++record_idx)
			{
				if (boundary_records[record_idx].ior_priority >= max_priority) {
					max_priority = boundary_records[record_idx].ior_priority;
					max_priority_record_idx = record_idx;
				}
			}
			return max_priority_record_idx;
		}
		__device__ __host__ inline bool PushRecord(uint32_t prim_idx, float ior, uint32_t ior_priority, int vol_idx)
		{
			if (record_count == MAX_BOUNDARY_RECORD) {
				printf("Reached the max boundary record.\n");
				return false;
			}
			boundary_records[record_count].prim_idx = prim_idx;
			boundary_records[record_count].ior = ior;
			boundary_records[record_count].ior_priority = ior_priority;
			boundary_records[record_count].vol_idx = vol_idx;
			++record_count;
			
			if (record_count == 1) {
				max_priority_record_index = 0;
				return true;
			}

			if (ior_priority >= boundary_records[max_priority_record_index].ior_priority) {
				max_priority_record_index = record_count - 1;
				return true;
			}
		}
		__device__ __host__ inline bool PopRecord(uint32_t hit_prim_idx)
		{
			if (record_count == 0) {
				return false;
			}
			const int record_idx = FindRecordWithPrim(hit_prim_idx);
			if ( record_idx == -1) {
				return false;
			}
			for (int idx = record_idx + 1; idx < record_count; ++idx) {
				boundary_records[idx - 1] = boundary_records[idx];
			}
			--record_count;
			max_priority_record_index = FindMaxPriorityRecord();
			return true;
		}
		__device__ __host__ inline float GetRayIOR() const
		{
			return record_count > 0 ? boundary_records[max_priority_record_index].ior : DEFAULT_IOR;
		}
		__device__ __host__ inline float GetExIOR(uint32_t hit_prim_idx) const
		{
			// some differences: find the max priority ior that prim_idx != hit_prim_idx
			uint32_t max_priority = 0;
			int max_priority_record_idx = -1;
			for (int record_idx = 0; record_idx < record_count; ++record_idx) {
				if (boundary_records[record_idx].prim_idx == hit_prim_idx) {
					continue;
				}
				if (boundary_records[record_idx].ior_priority >= max_priority) {
					max_priority = boundary_records[record_idx].ior_priority;
					max_priority_record_idx = record_idx;
				}
			}
			return max_priority_record_idx == -1 ? DEFAULT_IOR : boundary_records[max_priority_record_idx].ior;
		}
	};
};