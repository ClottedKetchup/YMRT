#include <unordered_map>
#include <algorithm>

#include "Camera.h"
#include "Material.h"
#include "ShadingUtilities.cuh"

namespace YumeRT {
	__device__ __host__  inline bool BVHTraverse(const Scene &scene, const Ray &ray, HitRecord *hit_record);
	__host__  void Camera::InitCameraRayTransfer(const Scene& scene)
	{
		std::unordered_map<uint32_t, HitRecord> prim_map;

		Ray ray = GenerateRay(0.5f, 0.5f);
		while (true)
		{
			HitRecord hit_record;
			bool hit_surface = BVHTraverse(scene, ray, &hit_record);
			if (!hit_surface) {
				break;
			}

			glm::vec3 hit_position(0.0f), hit_position_error(0.0f), hit_geometry_normal(0.0f);
			FetchGeometryNormal(scene, hit_record, &hit_position, &hit_position_error, &hit_geometry_normal);

			auto map_iter = prim_map.find(hit_record.hit_instance_idx);
			if (map_iter == prim_map.end()) {
				prim_map[hit_record.hit_instance_idx] = hit_record;
			}
			else {
				prim_map.erase(map_iter);
			}

			ray = Ray(OffsetRayOrigin(hit_position, hit_position_error, ray.direction, hit_geometry_normal), ray.direction);
		}

		camera_ray_transfer.record_count = 0;
		for (const auto &item : prim_map) 
		{
			const HitRecord &hit_record = item.second;
			const PrimitiveInstance &prim = scene.prim_instances[hit_record.hit_instance_idx];
			const Material &mtl = scene.materials[prim.material_idx];

			uint32_t mtl_ior_priority;
			float mtl_ior = mtl.FetchIOR(&mtl_ior_priority); 

			if (!camera_ray_transfer.PushRecord(hit_record.hit_instance_idx, mtl_ior, mtl_ior_priority, prim.inner_volume_idx)) {
				break;
			}
		}
	 }
};