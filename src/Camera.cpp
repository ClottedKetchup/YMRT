#include <unordered_map>
#include <algorithm>

#include "Camera.h"
#include "Material.h"
#include "ShadingUtilities.cuh"

namespace YumeRT {
	__device__ __host__  inline bool TraceRay(const Scene &scene,
		const Ray &ray,
		HitRecord *hit_record,
		int *nearby_hit_count,
		NearbyHit nearby_hits[]);

	__host__  void Camera::InitCameraRayTransfer(const Scene& scene)
	{
		std::unordered_map<uint32_t, HitRecord> prim_map;

		Ray ray = GenerateRay(0.5f, 0.5f);
		while (true)
		{
			HitRecord hit_record;
			int nearby_hit_count = 0;
			NearbyHit nearby_hits[MAX_BOUNDARY_RECORD + 2];
			bool hit_surface = TraceRay(scene, ray, &hit_record, &nearby_hit_count, nearby_hits);
			if (!hit_surface) {
				break;
			}

			glm::vec3 hit_position(0.0f), hit_position_error(0.0f), hit_geometry_normal(0.0f);
			FetchGeometryNormal(scene, hit_record, &hit_position, &hit_position_error, &hit_geometry_normal);

			auto map_iter = prim_map.find(hit_record.hit_instance_idx);
			if (map_iter == prim_map.end()) {
				prim_map[hit_record.hit_instance_idx] = hit_record;
			}

			ray = Ray(OffsetRayOrigin(hit_position, hit_position_error, ray.direction, hit_geometry_normal), ray.direction);
		}

		camera_ray_transfer.record_count = 0;
		for (const auto &item : prim_map) 
		{
			const PrimitiveInstance &prim = scene.prim_instances[item.second.hit_instance_idx];
			const glm::mat4 &i_transform = scene.i_transforms[prim.transform_idx];
			const Material &mtl = scene.materials[prim.material_idx];
			
			Ray test_ray = GenerateRay(0.5f, 0.5f);
			Ray positive_object_ray = TransformRay(test_ray, i_transform);
			Ray negative_object_ray;

			negative_object_ray.origin = positive_object_ray.origin;
			negative_object_ray.direction = -positive_object_ray.direction;
			negative_object_ray.t = positive_object_ray.t;
			negative_object_ray.time = positive_object_ray.time;

			const GeometryData &geometry = scene.geometries[prim.geometry_idx];

			HitRecord hit_record_positive;
			HitRecord hit_record_negative;
			if (IntersectGeometry(geometry, scene, positive_object_ray, &hit_record_positive, true) && 
				IntersectGeometry(geometry, scene, negative_object_ray, &hit_record_negative, true))
			{
				uint32_t mtl_ior_priority;
				float mtl_ior = mtl.FetchIOR(&mtl_ior_priority);
				if (!camera_ray_transfer.PushRecord(item.second.hit_instance_idx, mtl_ior, mtl_ior_priority, prim.inner_volume_idx)) {
					break;
				}
			}
		}
	 }
};