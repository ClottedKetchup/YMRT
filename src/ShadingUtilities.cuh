#pragma once

#include <driver_types.h>

#include "GeometryDefines.h"
#include "SceneDefines.h"
#include "BoundingBox.h"
#include "MathCommon.h"
#include "Ray.h"
#include "BottomBVH.h"
#include "TopBVH.h"

#include "Material.h"

#include "PrimtiveInstance.h"
#include "Volume.h"

namespace YumeRT
{
#pragma pack(push, 16)
	struct HitRecord
	{
		// for sphere, it is the object space position
		glm::vec3 hit_barycentric = glm::vec3(1.0f, 1.0f, 1.0f); 
		float hit_t = TMAX;
		uint32_t hit_instance_idx = EMPTY_UINT32;

		// for mesh only
		uint32_t hit_triangle_idx = EMPTY_UINT32;
		int hit_back = 0;
		int volume_hit = 0;
		// may have to store if the ray hit the object back face
	};
#pragma pack(pop)

	struct ExtraHitInfo 
	{
		float t_hit = TMAX;
		bool hit = false;
		bool hit_back = false;
	};

	struct NearbyHit 
	{
		uint32_t hit_prim_idx = EMPTY_UINT32;
		float t_hit = TMAX;
		bool hit_back = false;

		__device__ __host__ inline NearbyHit() :hit_prim_idx(EMPTY_UINT32), t_hit(TMAX), hit_back(false) {}
		__device__ __host__ inline NearbyHit(uint32_t hit_prim_idx, float t_hit, bool hit_back) : hit_prim_idx(hit_prim_idx), t_hit(t_hit), hit_back(hit_back) {}
	};

	// This is for Mesh intersection
	__device__ __host__  inline bool IntersectTri(const Triangle &triangle, 
		const glm::vec3 *mesh_positions, 
		const uint32_t *mesh_vidxs, 
		const Ray &ray, 
		HitRecord *hit_record,
		ExtraHitInfo *extra_hit_info = nullptr)
	{
		const uint32_t vid0 = mesh_vidxs[triangle.id0];
		const uint32_t vid1 = mesh_vidxs[triangle.id1];
		const uint32_t vid2 = mesh_vidxs[triangle.id2];

		glm::vec3  p0 = mesh_positions[vid0];
		glm::vec3  p1 = mesh_positions[vid1];
		glm::vec3  p2 = mesh_positions[vid2];

		const glm::vec3 geometry_normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));

		p0 -= ray.origin;
		p1 -= ray.origin;
		p2 -= ray.origin;

		int kz = MaxDimension(glm::abs(ray.direction));
		int kx = kz + 1; if (kx == 3) { kx = 0; }
		int ky = kx + 1; if (ky == 3) { ky = 0; }
		glm::vec3 d = Permute(ray.direction, kx, ky, kz);

		p0 = Permute(p0, kx, ky, kz);
		p1 = Permute(p1, kx, ky, kz);
		p2 = Permute(p2, kx, ky, kz);

		float scale_x = -d.x / d.z;
		float scale_y = -d.y / d.z;
		float scale_z = 1.f / d.z;

		p0.x += scale_x * p0.z;
		p0.y += scale_y * p0.z;
		p1.x += scale_x * p1.z;
		p1.y += scale_y * p1.z;
		p2.x += scale_x * p2.z;
		p2.y += scale_y * p2.z;

		float e0 = p1.x * p2.y - p1.y * p2.x;
		float e1 = p2.x * p0.y - p2.y * p0.x;
		float e2 = p0.x * p1.y - p0.y * p1.x;

		if ((e0 < 0 || e1 < 0 || e2 < 0) && (e0 > 0 || e1 > 0 || e2 > 0))
		{
			return false;
		}
		float denominator = e0 + e1 + e2;
		if (denominator == 0)
		{
			return false;
		}

		p0.z *= scale_z;
		p1.z *= scale_z;
		p2.z *= scale_z;

		float inv_denominator = 1.0f / denominator;

		float t = e0 * p0.z + e1 * p1.z + e2 * p2.z;
		t *= inv_denominator;
		if (extra_hit_info != nullptr && t >= TMIN) {
			extra_hit_info->hit = true;
			extra_hit_info->t_hit = glm::min(extra_hit_info->t_hit, t);
			extra_hit_info->hit_back = glm::dot(geometry_normal, ray.direction) > 0.0f;
		}
		
		if (t < TMIN || t > ray.t) {
			return false;
		}

		ray.t = t;

		hit_record->hit_t = t;
		hit_record->hit_barycentric = glm::vec3(e0, e1, e2) * inv_denominator;
		hit_record->hit_back = glm::dot(geometry_normal, ray.direction) > 0.0f;

		return true;
	}

	__device__ __host__  inline bool IntersectMeshObject(const TriangleMesh &triangle_mesh, 
		const Ray &ray, 
		HitRecord *hit_record, 
		bool test_any_hit, 
		ExtraHitInfo *extra_hit_info = nullptr)
	{
		if (triangle_mesh.GetNodesDevice() == nullptr) { 
			return false; 
		}

		const uint32_t stack_size = 32;
		uint32_t root_idx = 0;

		uint32_t stack[stack_size];
		int p_top = -1;

		const Triangle *triangles = triangle_mesh.GetTrianglesDevice();
		const uint32_t *vidxs = triangle_mesh.GetPositionIndicesDevice();
		const glm::vec3 *positions = triangle_mesh.GetPositionsDevice();
		const BottomNode *bottom_nodes = triangle_mesh.GetNodesDevice();
		if (triangles == nullptr || vidxs == nullptr || positions == nullptr || bottom_nodes == nullptr) { 
			return false; 
		}

		float t = 0.0f;
		bool hit = false;

		stack[++p_top] = root_idx;
		while (p_top != -1)
		{
			uint32_t node_idx = stack[p_top--];
			const BottomNode &node = bottom_nodes[node_idx];

			if (node.internal.left != EMPTY_UINT32)
			{
				const Triangle *tris = triangles + node.leaf.offset;
				for (uint32_t leaf_prim_idx = 0; leaf_prim_idx < node.leaf.count; ++leaf_prim_idx)
				{
					if (IntersectTri(tris[leaf_prim_idx], positions, vidxs, ray, hit_record, extra_hit_info))
					{
						hit_record->hit_triangle_idx = node.leaf.offset + leaf_prim_idx;
						hit = hit || true;
					}
					if (test_any_hit && hit) { 
						return hit; 
					}
				}
				continue;
			}

			uint32_t near_node = node_idx + 1;
			uint32_t far_node = node.internal.right;

			float t_near, t_far;
			bool intersect_near = IntersectBBox3(bottom_nodes[near_node].bbox, ray, &t_near);
			bool intersect_far = IntersectBBox3(bottom_nodes[far_node].bbox, ray, &t_far);
			if (t_far < t_near)
			{
				Swap(t_far, t_near);
				Swap(near_node, far_node);
				Swap(intersect_near, intersect_far);
			}

			if (intersect_far)
			{
				stack[++p_top] = far_node;
				assert(p_top < stack_size);
			}
			if (intersect_near)
			{
				stack[++p_top] = near_node;
				assert(p_top < stack_size);
			}
		}

		return hit;
	}

	__device__ __host__  inline bool IntersectSphereObject(const Sphere &sphere, 
		const Ray &ray, 
		HitRecord *hit_record, 
		bool test_any_hit, 
		ExtraHitInfo *extra_hit_info = nullptr)
	{
		float radius = sphere.radius;

		const glm::vec3 &ray_o = ray.origin;
		const glm::vec3 &ray_d = ray.direction;

		float a = ray_d.x * ray_d.x + ray_d.y * ray_d.y + ray_d.z * ray_d.z;
		float b = 2.0f * (ray_d.x * ray_o.x + ray_d.y * ray_o.y + ray_d.z * ray_o.z);
		float c = ray_o.x * ray_o.x + ray_o.y * ray_o.y + ray_o.z * ray_o.z - radius * radius;
		float discriminator = b * b - 4.0f * a * c;
		float t = 0.f;

		if (discriminator < 0) {
			return false;
		}

		float sqrt_d = glm::sqrt(discriminator);
		float t1 = (-b - sqrt_d) / (2.0 * a);
		float t2 = (-b + sqrt_d) / (2.0 * a);
		if (extra_hit_info != nullptr) 
		{
			extra_hit_info->hit = true;
			extra_hit_info->t_hit = t1 >= TMIN ? t1 : t2;
			extra_hit_info->hit_back = glm::dot(ray_d, glm::normalize(ray_o + ray_d * (extra_hit_info->t_hit))) > 0.0f;
		}

		if (t1 > ray.t || t2 < TMIN) {
			return false;
		}

		t = t1;
		if (t < TMIN)
		{
			t = t2;
			if (t > ray.t) {
				return false;
			}
		}

		ray.t = t;

		glm::vec3 position_object = ray_o + ray_d * t;
		float distance = length(position_object);
		position_object = distance > (float)FLOAT_EPSILON ?
			position_object * radius / distance : glm::vec3(0.0f);

		hit_record->hit_t = t;
		hit_record->hit_barycentric = position_object;
		hit_record->hit_back = glm::dot(ray_d, position_object / radius) > 0.0f;
		return true;
	}

	__device__ __host__  inline bool IntersectGeometry(const GeometryData &geometry, 
		const Scene &scene, 
		const Ray &ray, 
		HitRecord *hit_record, 
		bool test_any_hit, 
		ExtraHitInfo *extra_hit_info = nullptr)
	{
		uint32_t geo_type = geometry.geometry_type;
		if (geo_type == GEOMETRY_TYPE::TRIANGLE_MESH)
		{
			return IntersectMeshObject(geometry.triangle_mesh, 
				ray, 
				hit_record, 
				test_any_hit, 
				extra_hit_info);
		}
		else if (geo_type == GEOMETRY_TYPE::SPHERE)
		{
			return IntersectSphereObject(geometry.sphere, 
				ray, 
				hit_record, 
				test_any_hit, 
				extra_hit_info);
		}
		else
		{
			return false;
		}
	}

	__device__ __host__  inline bool TraceRay(const Scene &scene, 
		const Ray &ray, 
		HitRecord *hit_record, 
		int *nearby_hit_count, 
		NearbyHit nearby_hits[])
	{
		if (scene.top_node_count == 0 || scene.top_nodes == nullptr) { 
			return false; 
		}

		int nearby_prim_count = 0;
		float t_upper_bound = 0.0f;
		float t_lower_bound = 0.0f;
		NearbyHit nearby_hit_prims[MAX_BOUNDARY_RECORD + 2];
		auto push_nearby_hits = [&](const NearbyHit& ray_hit) {
			if (nearby_prim_count == MAX_BOUNDARY_RECORD + 2) {
				return;
			}
			nearby_hit_prims[nearby_prim_count++] = ray_hit;
		};

		const uint32_t stack_size = 32;
		const uint32_t root_idx = 0;
		uint32_t stack[stack_size];
		int p_top = -1;

		float t = 0.0f;
		bool hit = false;

		stack[++p_top] = root_idx;
		while (p_top != -1)
		{
			uint32_t node_idx = stack[p_top--];
			TopNode &node = scene.top_nodes[node_idx];

			if (node.internal.left != EMPTY_UINT32)
			{
				const PrimitiveInstance *prim_instances = scene.primitive_instances + node.leaf.offset;
				for (uint32_t leaf_prim_idx = 0; leaf_prim_idx < node.leaf.count; ++leaf_prim_idx)
				{
					const PrimitiveInstance &prim = prim_instances[leaf_prim_idx];

					const glm::mat4 &i_transform = scene.i_transforms[prim.transform_idx];
					Ray object_ray = TransformRay(ray, i_transform);

					const GeometryData &geometry = scene.geometries[prim.geometry_idx];

					HitRecord prim_hit_record;
					ExtraHitInfo prim_extra_hit_info;
					if (IntersectGeometry(geometry, scene, object_ray, &prim_hit_record, false, &prim_extra_hit_info))
					{
						assert(prim_extra_hit_info.hit);
						hit_record->hit_back = prim_hit_record.hit_back;
						hit_record->hit_barycentric = prim_hit_record.hit_barycentric;
						hit_record->hit_t = prim_hit_record.hit_t;
						hit_record->hit_triangle_idx = prim_hit_record.hit_triangle_idx;
						hit_record->hit_instance_idx = node.leaf.offset + leaf_prim_idx;
						hit_record->volume_hit = prim_hit_record.volume_hit;

						hit = hit || true;
						ray.t = object_ray.t;

						// TODO: update upper bound and lower bound
						t_upper_bound = NextFloatUp(NextFloatUp(ray.t));
						t_lower_bound = NextFloatDown(NextFloatDown(ray.t));

						// TODO: update the nearby object list
						int head = 0, tail = nearby_prim_count - 1;
						while (head <= tail) {
							if (nearby_hit_prims[head].t_hit > t_lower_bound && nearby_hit_prims[head].t_hit < t_upper_bound) {
								++head;
							}
							else {
								Swap(nearby_hit_prims[head], nearby_hit_prims[tail--]);
							}
						}
						nearby_prim_count = head;
						
						push_nearby_hits(NearbyHit(hit_record->hit_instance_idx, hit_record->hit_t, hit_record->hit_back));
					}
					else {
						// TODO:
						if (hit && prim_extra_hit_info.hit &&
							(prim_extra_hit_info.t_hit > t_lower_bound && prim_extra_hit_info.t_hit < t_upper_bound)) {
							push_nearby_hits(NearbyHit(node.leaf.offset + leaf_prim_idx, prim_extra_hit_info.t_hit, prim_extra_hit_info.hit_back));
						}
					}
				}
				continue;
			}

			uint32_t near_node = node_idx + 1;
			uint32_t far_node = node.internal.right;

			float t_near, t_far;
			bool intersect_near = IntersectBBox3(scene.top_nodes[near_node].bbox, ray, &t_near);
			bool intersect_far = IntersectBBox3(scene.top_nodes[far_node].bbox, ray, &t_far);
			if (t_far < t_near)
			{
				Swap(t_far, t_near);
				Swap(near_node, far_node);
				Swap(intersect_near, intersect_far);
			}

			if (intersect_far)
			{
				stack[++p_top] = far_node;
				assert(p_top < stack_size);
			}
			if (intersect_near)
			{
				stack[++p_top] = near_node;
				assert(p_top < stack_size);
			}
		}

		assert(nearby_prim_count <= MAX_BOUNDARY_RECORD + 2);
		for (int idx = 0; idx < nearby_prim_count; ++idx) {
			nearby_hits[idx] = nearby_hit_prims[idx];
		}
		*nearby_hit_count = nearby_prim_count;
		return hit;
	}

	// TODO: you should construst a separate BVH for volume traverse!
	__device__ __host__  inline bool TraceVolume(const Scene &scene, 
		const Ray &ray, 
		HitRecord *hit_record,
		int *nearby_hit_count,
		NearbyHit nearby_hits[])
	{
		if (scene.top_node_count == 0 || scene.top_nodes == nullptr || scene.volume_count == 0) { 
			return false; 
		}

		int nearby_prim_count = 0;
		float t_upper_bound = 0.0f;
		float t_lower_bound = 0.0f;
		NearbyHit nearby_hit_prims[MAX_BOUNDARY_RECORD + 2];
		auto push_nearby_hits = [&](const NearbyHit& ray_hit) {
			if (nearby_prim_count == MAX_BOUNDARY_RECORD + 2) {
				return;
			}
			nearby_hit_prims[nearby_prim_count++] = ray_hit;
		};

		const uint32_t stack_size = 32;
		const uint32_t root_idx = 0;
		uint32_t stack[stack_size];
		int p_top = -1;

		float t = 0.0f;
		bool hit = false;

		stack[++p_top] = root_idx;
		while (p_top != -1)
		{
			uint32_t node_idx = stack[p_top--];
			TopNode &node = scene.top_nodes[node_idx];

			if (node.internal.left != EMPTY_UINT32)
			{
				const PrimitiveInstance *prim_instances = scene.primitive_instances + node.leaf.offset;
				for (uint32_t leaf_prim_idx = 0; leaf_prim_idx < node.leaf.count; ++leaf_prim_idx)
				{
					if (!prim_instances[leaf_prim_idx].treat_as_boundary) { 
						continue; 
					}

					const PrimitiveInstance &prim = prim_instances[leaf_prim_idx];

					const glm::mat4 &i_transform = scene.i_transforms[prim.transform_idx];
					Ray object_ray = TransformRay(ray, i_transform);

					const GeometryData &geometry = scene.geometries[prim.geometry_idx];
					
					HitRecord prim_hit_record;
					ExtraHitInfo prim_extra_hit_info;
					if (IntersectGeometry(geometry, scene, object_ray, &prim_hit_record, false, &prim_extra_hit_info))
					{
						assert(prim_extra_hit_info.hit);
						hit_record->hit_back = prim_hit_record.hit_back;
						hit_record->hit_barycentric = prim_hit_record.hit_barycentric;
						hit_record->hit_t = prim_hit_record.hit_t;
						hit_record->hit_triangle_idx = prim_hit_record.hit_triangle_idx;
						hit_record->hit_instance_idx = node.leaf.offset + leaf_prim_idx;
						hit_record->volume_hit = prim_hit_record.volume_hit;

						hit = hit || true;
						ray.t = object_ray.t;

						// update upper bound and lower bound
						t_upper_bound = NextFloatUp(NextFloatUp(ray.t));
						t_lower_bound = NextFloatDown(NextFloatDown(ray.t));

						// update the nearby object list
						int head = 0, tail = nearby_prim_count - 1;
						while (head <= tail) {
							if (nearby_hit_prims[head].t_hit > t_lower_bound && nearby_hit_prims[head].t_hit < t_upper_bound) {
								++head;
							}
							else {
								Swap(nearby_hit_prims[head], nearby_hit_prims[tail--]);
							}
						}
						nearby_prim_count = head;

						// push current hit into the nearby hits
						push_nearby_hits(NearbyHit(hit_record->hit_instance_idx, hit_record->hit_t, hit_record->hit_back));
					}
					else 
					{
						if (hit && prim_extra_hit_info.hit &&
							(prim_extra_hit_info.t_hit > t_lower_bound && prim_extra_hit_info.t_hit < t_upper_bound)) {
							push_nearby_hits(NearbyHit(node.leaf.offset + leaf_prim_idx, prim_extra_hit_info.t_hit, prim_extra_hit_info.hit_back));
						}
					}
				}
				continue;
			}

			uint32_t near_node = node_idx + 1;
			uint32_t far_node = node.internal.right;

			float t_near, t_far;
			bool intersect_near = IntersectBBox3(scene.top_nodes[near_node].bbox, ray, &t_near);
			bool intersect_far = IntersectBBox3(scene.top_nodes[far_node].bbox, ray, &t_far);
			if (t_far < t_near)
			{
				Swap(t_far, t_near);
				Swap(near_node, far_node);
				Swap(intersect_near, intersect_far);
			}

			if (intersect_far)
			{
				stack[++p_top] = far_node;
				assert(p_top < stack_size);
			}
			if (intersect_near)
			{
				stack[++p_top] = near_node;
				assert(p_top < stack_size);
			}
		}

		assert(nearby_prim_count <= MAX_BOUNDARY_RECORD + 2);
		for (int idx = 0; idx < nearby_prim_count; ++idx) {
			nearby_hits[idx] = nearby_hit_prims[idx];
		}
		*nearby_hit_count = nearby_prim_count;
		return hit;
	}

	__device__ __host__  inline bool BVHTraverseShadow(const Scene &scene, const Ray &ray, HitRecord *hit_record)
	{
		if (scene.top_node_count == 0 || scene.top_nodes == nullptr) { 
			return false; 
		}

		const uint32_t stack_size = 32;
		uint32_t root_idx = 0;
		uint32_t stack[stack_size];
		int p_top = -1;

		float t = 0.0f;
		bool hit = false;

		stack[++p_top] = root_idx;
		while (p_top != -1)
		{
			uint32_t node_idx = stack[p_top--];
			TopNode &node = scene.top_nodes[node_idx];

			if (node.internal.left != EMPTY_UINT32)
			{
				const PrimitiveInstance *prim_instances = scene.primitive_instances + node.leaf.offset;
				for (uint32_t leaf_prim_idx = 0; leaf_prim_idx < node.leaf.count; ++leaf_prim_idx)
				{
					// skip the test of volume boundary
					if (prim_instances[leaf_prim_idx].treat_as_boundary) { 
						continue; 
					}

					const glm::mat4 &i_transform = scene.i_transforms[prim_instances[leaf_prim_idx].transform_idx];

					Ray object_ray = TransformRay(ray, i_transform);
					const GeometryData &geometry = scene.geometries[prim_instances[leaf_prim_idx].geometry_idx];

					if (IntersectGeometry(geometry, scene, object_ray, hit_record, true))  /* test any hit */
					{
						hit_record->hit_instance_idx = node.leaf.offset + leaf_prim_idx;
						hit = hit || true;
						ray.t = object_ray.t;
						return hit; // any hit return
					}
				}
				continue;
			}

			uint32_t near_node = node_idx + 1;
			uint32_t far_node = node.internal.right;

			float t_near, t_far;
			bool intersect_near = IntersectBBox3(scene.top_nodes[near_node].bbox, ray, &t_near);
			bool intersect_far = IntersectBBox3(scene.top_nodes[far_node].bbox, ray, &t_far);
			if (t_far < t_near)
			{
				Swap(t_far, t_near);
				Swap(near_node, far_node);
				Swap(intersect_near, intersect_far);
			}

			if (intersect_far)
			{
				stack[++p_top] = far_node;
				assert(p_top < stack_size);
			}
			if (intersect_near)
			{
				stack[++p_top] = near_node;
				assert(p_top < stack_size);
			}
		}

		return hit;
	}



	__device__ __host__ inline void FetchSphereShadingData(const Scene &scene,
																					const HitRecord &hit_record,
																					const PrimitiveInstance &hit_instance,
																					glm::vec3 *hit_position,
																					glm::vec3 *hit_position_error,
																					glm::vec3 *hit_shading_normal,
																					glm::vec3 *hit_geometry_normal,
																					glm::vec2 *hit_uv,
																					glm::vec3 *hit_dpdu,
																					glm::vec3 *hit_dpdv, 
																					glm::vec3 *hit_dndu = nullptr,
																					glm::vec3 *hit_dndv = nullptr)
	{
		const Sphere &hit_sphere = scene.geometries[hit_instance.geometry_idx].sphere;

		float radius = glm::max(hit_sphere.radius, (float)FLOAT_EPSILON);

		const glm::vec3 position_object = hit_record.hit_barycentric;
		const glm::vec3 normal_object = hit_record.hit_barycentric / radius;

		*hit_position = position_object;
		*hit_position_error = ErrorGamma(5) * glm::abs(position_object);
		*hit_shading_normal = normal_object;
		*hit_geometry_normal = normal_object;

		float theta = glm::acos(normal_object.y);
		float phi = glm::atan(normal_object.z, normal_object.x);
		if (phi < 0.0f) { 
			phi += TWO_PI; 
		}
		*hit_uv = glm::vec2(0.5f * phi * INV_PI, theta * INV_PI);

		float sin_theta = 0.0f;
		if (1.0f - normal_object.y * normal_object.y > 0.0f){
			sin_theta = glm::sqrt(1.0f - normal_object.y * normal_object.y);
		}

		float cos_phi = 0.0f, sin_phi = 0.0f;
		float r_xz = sin_theta;
		if (r_xz > 0.0f){
			cos_phi = normal_object.x / r_xz;
			sin_phi = normal_object.z / r_xz;
		}
		else {
			cos_phi = 1.0f, sin_phi = 0.0f;
		}

		// pbrt: sphere
		glm::vec3 dpdu = glm::vec3(-position_object.z, 0.0f, position_object.x) * TWO_PI;
		glm::vec3 dpdv = glm::vec3(position_object.y * cos_phi, -radius * sin_theta, sin_phi * position_object.y) * ONE_PI;
		*hit_dpdu = dpdu;
		*hit_dpdv = dpdv;

		if (hit_dndu != nullptr || hit_dndv != nullptr) 
		{
			glm::vec3 dp_duu = -Sqr(TWO_PI) * glm::vec3(position_object.x, 0.0f, position_object.z);
			glm::vec3 dp_duv = TWO_PI * ONE_PI * glm::vec3(-position_object.y * sin_phi, 0.0f, position_object.y * cos_phi);
			glm::vec3 dp_dvv = -Sqr(ONE_PI) * glm::vec3(position_object.x, position_object.y, position_object.z);
			float E = glm::dot(dpdu, dpdu);
			float F = glm::dot(dpdu, dpdv);
			float G = glm::dot(dpdv, dpdv);
			float e = glm::dot(normal_object, dp_duu);
			float f = glm::dot(normal_object, dp_duv);
			float g = glm::dot(normal_object, dp_dvv);
			float inv_EGF2 = 1.0f * SafeRcp(E * G - F * F);
			if (hit_dndu != nullptr) { 
				*hit_dndu = glm::vec3((f * F - e * G) * inv_EGF2 * dpdu + (e * F - f * E) * inv_EGF2 * dpdv); 
			}
			if (hit_dndv != nullptr) {
				*hit_dndv = glm::vec3((g * F - f * G) * inv_EGF2 * dpdu + (f * F - g * E) * inv_EGF2 * dpdv);
			}
		}
	}

	__device__ __host__ inline void FetchMeshShadingData(const Scene &scene,
																				const HitRecord &hit_record,
																				const PrimitiveInstance &hit_instance,
																				glm::vec3 *hit_position,
																				glm::vec3 *hit_position_error,
																				glm::vec3 *hit_shading_normal,
																				glm::vec3 *hit_geometry_normal,
																				glm::vec2 *hit_uv,
																				glm::vec3 *hit_dpdu,
																				glm::vec3 *hit_dpdv,
																				glm::vec3 *hit_dndu = nullptr,
																				glm::vec3 *hit_dndv = nullptr)
	{
		const TriangleMesh &hit_mesh = scene.geometries[hit_instance.geometry_idx].triangle_mesh;
		
		const glm::vec3 *mesh_positions = hit_mesh.GetPositionsDevice();
		const glm::vec3 *mesh_normals = hit_mesh.GetNormalsDevice();
		const uint32_t *mesh_vidxs = hit_mesh.GetPositionIndicesDevice();
		const uint32_t *mesh_nidxs = hit_mesh.GetNormalIndicesDevice();

		const Triangle *mesh_triangles = hit_mesh.GetTrianglesDevice();
		const Triangle &triangle = mesh_triangles[hit_record.hit_triangle_idx];
		
		uint32_t vid0 = mesh_vidxs[triangle.id0];
		uint32_t vid1 = mesh_vidxs[triangle.id1];
		uint32_t vid2 = mesh_vidxs[triangle.id2];

		const glm::vec3 &p0 = mesh_positions[vid0];
		const glm::vec3 &p1 = mesh_positions[vid1];
		const glm::vec3 &p2 = mesh_positions[vid2];

		const float x_abs_sum = glm::abs(p0.x * hit_record.hit_barycentric.x) + glm::abs(p1.x * hit_record.hit_barycentric.y) + glm::abs(p2.x * hit_record.hit_barycentric.z);
		const float y_abs_sum = glm::abs(p0.y * hit_record.hit_barycentric.x) + glm::abs(p1.y * hit_record.hit_barycentric.y) + glm::abs(p2.y * hit_record.hit_barycentric.z);
		const float z_abs_sum = glm::abs(p0.z * hit_record.hit_barycentric.x) + glm::abs(p1.z * hit_record.hit_barycentric.y) + glm::abs(p2.z * hit_record.hit_barycentric.z);
		*hit_position = p0 * hit_record.hit_barycentric.x + p1 * hit_record.hit_barycentric.y + p2 * hit_record.hit_barycentric.z;
		*hit_position_error = ErrorGamma(7) * glm::vec3(x_abs_sum, y_abs_sum, z_abs_sum);

		uint32_t nid0 = mesh_nidxs[triangle.id0];
		uint32_t nid1 = mesh_nidxs[triangle.id1];
		uint32_t nid2 = mesh_nidxs[triangle.id2];

		const glm::vec3 &n0 = mesh_normals[nid0];
		const glm::vec3 &n1 = mesh_normals[nid1];
		const glm::vec3 &n2 = mesh_normals[nid2];

		*hit_shading_normal = n0 * hit_record.hit_barycentric.x + n1 * hit_record.hit_barycentric.y + n2 * hit_record.hit_barycentric.z;
		*hit_geometry_normal = glm::cross(p1 - p0, p2 - p0);

		bool has_custom_uv = (hit_mesh.GetTexcoordsDevice() != nullptr && hit_mesh.GetTexcoordIndicesDevice() != nullptr);

		const uint32_t *mesh_uvidxs = has_custom_uv? hit_mesh.GetTexcoordIndicesDevice() : nullptr;
		const glm::vec2 *mesh_texcoords = has_custom_uv ? hit_mesh.GetTexcoordsDevice() : nullptr;
		
		uint32_t uvid0 = has_custom_uv ? mesh_uvidxs[triangle.id0] : vid0;
		uint32_t uvid1 = has_custom_uv ? mesh_uvidxs[triangle.id1] : vid1;
		uint32_t uvid2 = has_custom_uv ? mesh_uvidxs[triangle.id2] : vid2;

		glm::vec2 uv0 = has_custom_uv ? mesh_texcoords[uvid0] : glm::vec2(0.0f, 0.0f);
		glm::vec2 uv1 = has_custom_uv ? mesh_texcoords[uvid1] : glm::vec2(1.0f, 0.0f);
		glm::vec2 uv2 = has_custom_uv ? mesh_texcoords[uvid2] : glm::vec2(1.0f, 1.0f);

		*hit_uv = uv0 * hit_record.hit_barycentric.x
					+ uv1 * hit_record.hit_barycentric.y
					+ uv2 * hit_record.hit_barycentric.z;

		float delta_u01 = uv1.x - uv0.x, delta_v01 = uv1.y - uv0.y;
		float delta_u02 = uv2.x - uv0.x, delta_v02 = uv2.y - uv0.y;
		float determinant = delta_u01 * delta_v02 - delta_v01 * delta_u02;
		if (glm::abs(determinant) < (float)FLOAT_EPSILON)
		{
			delta_u01 = 1.0f, delta_v01 = 0.0f;
			delta_u02 = 0.0f, delta_v02 = 1.0f;
			determinant = 1.0f;
		}

		float i_det = 1.0f / determinant;
		glm::vec3 delta_p01 = p1 - p0, delta_p02 = p2 - p0;
		*hit_dpdu = (delta_v02 * delta_p01 - delta_v01 * delta_p02) * i_det;
		*hit_dpdv = (-delta_u02 * delta_p01 + delta_u01 * delta_p02) * i_det;

		if (hit_dndu != nullptr || hit_dndv != nullptr)
		{
			glm::vec3 delta_n01 = n1 - n0, delta_n02 = n2 - n0;
			if (hit_dndu != nullptr) { 
				*hit_dndu = (delta_v02 * delta_n01 - delta_v01 * delta_n02) * i_det;
			}
			if (hit_dndv != nullptr) {
				*hit_dndv = (-delta_u02 * delta_n01 + delta_u01 * delta_n02) * i_det;
			}
		}
	}

	__device__ __host__ inline void FetchShadingData(const Scene &scene,
																			const HitRecord &hit_record,
																			glm::vec3 *hit_position,
																			glm::vec3 *hit_position_error,
																			glm::vec3 *hit_position_object_space,
																			glm::vec3 *hit_shading_normal,
																			glm::vec3 *hit_geometry_normal,
																			glm::vec2 *hit_uv,
																			glm::vec3 *hit_dpdu,
																			glm::vec3 *hit_dpdv, 
																			glm::vec3 *hit_dndu = nullptr, 
																			glm::vec3 *hit_dndv = nullptr)
	{
		if (hit_record.hit_instance_idx == EMPTY_UINT32) { return; }
		const PrimitiveInstance &hit_instance = scene.primitive_instances[hit_record.hit_instance_idx];
		const GeometryData &hit_geometry = scene.geometries[hit_instance.geometry_idx];
		uint32_t geometry_type = hit_geometry.geometry_type;
		if (geometry_type == GEOMETRY_TYPE::TRIANGLE_MESH)
		{
			FetchMeshShadingData(scene,
											hit_record,
											hit_instance,
											hit_position,
											hit_position_error,
											hit_shading_normal,
											hit_geometry_normal,
											hit_uv,
											hit_dpdu,
											hit_dpdv, 
											hit_dndu, 
											hit_dndv);
		}
		else if (geometry_type == GEOMETRY_TYPE::SPHERE)
		{
			FetchSphereShadingData(scene,
												hit_record,
												hit_instance,
												hit_position,
												hit_position_error,
												hit_shading_normal,
												hit_geometry_normal,
												hit_uv,
												hit_dpdu,
												hit_dpdv, 
												hit_dndu,
												hit_dndv);
		}
		else 
		{
			// nothing to do
		}

		// transform geometry data
		const glm::mat4 &otw = scene.transforms[hit_instance.transform_idx];
		const glm::mat4 &wto = scene.i_transforms[hit_instance.transform_idx];

		*hit_position_object_space = *hit_position;
		*hit_position = TransformPosition(otw, *hit_position, hit_position_error);
		*hit_shading_normal = glm::normalize(TransposeTransformVector(wto, *hit_shading_normal));
		*hit_geometry_normal = glm::normalize(TransposeTransformVector(wto, *hit_geometry_normal));
		*hit_dpdu = TransformVector(otw, *hit_dpdu);
		*hit_dpdv = TransformVector(otw, *hit_dpdv);
		if (hit_dndu != nullptr) {
			*hit_dndu = TransformVector(otw, *hit_dndu);
		}
		if (hit_dndv != nullptr) {
			*hit_dndv = TransformVector(otw, *hit_dndv);
		}

		// TODO: maybe flipped the shading normal to match geometry normal
	}

	__device__ __host__ inline void FetchSphereGeometryNormal(const Scene &scene,
																								const HitRecord &hit_record, 
																								const PrimitiveInstance &hit_instance, 
																								glm::vec3 *hit_position, 
																								glm::vec3 *hit_position_error, 
																								glm::vec3 *hit_geometry_normal)
	{
		const Sphere &hit_sphere = scene.geometries[hit_instance.geometry_idx].sphere;
		float radius = glm::max(hit_sphere.radius, (float)FLOAT_EPSILON);

		*hit_position = hit_record.hit_barycentric;
		*hit_position_error = ErrorGamma(5) * glm::abs(hit_record.hit_barycentric);
		*hit_geometry_normal = hit_record.hit_barycentric / radius;
	}

	__device__ __host__ inline void FetchMeshGeometryNormal(const Scene &scene,
																								const HitRecord &hit_record, 
																								const PrimitiveInstance &hit_instance,
																								glm::vec3 *hit_position,
																								glm::vec3 *hit_position_error,
																								glm::vec3 *hit_geometry_normal)
	{
		const TriangleMesh &hit_mesh = scene.geometries[hit_instance.geometry_idx].triangle_mesh;

		const glm::vec3 *mesh_positions = hit_mesh.GetPositionsDevice();
		const uint32_t *mesh_vidxs = hit_mesh.GetPositionIndicesDevice();

		const Triangle *mesh_triangles = hit_mesh.GetTrianglesDevice();
		const Triangle &triangle = mesh_triangles[hit_record.hit_triangle_idx];

		uint32_t vid0 = mesh_vidxs[triangle.id0];
		uint32_t vid1 = mesh_vidxs[triangle.id1];
		uint32_t vid2 = mesh_vidxs[triangle.id2];

		const glm::vec3 &p0 = mesh_positions[vid0];
		const glm::vec3 &p1 = mesh_positions[vid1];
		const glm::vec3 &p2 = mesh_positions[vid2];

		const float x_abs_sum = glm::abs(p0.x * hit_record.hit_barycentric.x) + glm::abs(p1.x * hit_record.hit_barycentric.y) + glm::abs(p2.x * hit_record.hit_barycentric.z);
		const float y_abs_sum = glm::abs(p0.y * hit_record.hit_barycentric.x) + glm::abs(p1.y * hit_record.hit_barycentric.y) + glm::abs(p2.y * hit_record.hit_barycentric.z);
		const float z_abs_sum = glm::abs(p0.z * hit_record.hit_barycentric.x) + glm::abs(p1.z * hit_record.hit_barycentric.y) + glm::abs(p2.z * hit_record.hit_barycentric.z);
		*hit_position = p0 * hit_record.hit_barycentric.x + p1 * hit_record.hit_barycentric.y + p2 * hit_record.hit_barycentric.z;
		*hit_position_error = ErrorGamma(7) * glm::vec3(x_abs_sum, y_abs_sum, z_abs_sum);
		*hit_geometry_normal = glm::cross(p1 - p0, p2 - p0);
	}

	__device__ __host__ inline void FetchGeometryNormal(const Scene &scene, 
																						const HitRecord &hit_record, 
																						glm::vec3 *hit_position,
																						glm::vec3 *hit_position_error,
																						glm::vec3 *hit_geometry_normal)
	{
		if (hit_record.hit_instance_idx == EMPTY_UINT32) { return; }
		const PrimitiveInstance &hit_instance = scene.primitive_instances[hit_record.hit_instance_idx];
		const GeometryData &hit_geometry = scene.geometries[hit_instance.geometry_idx];
		uint32_t geometry_type = hit_geometry.geometry_type;

		if (geometry_type == GEOMETRY_TYPE::TRIANGLE_MESH)
		{
			FetchMeshGeometryNormal(scene, hit_record, hit_instance, hit_position, hit_position_error, hit_geometry_normal);
		}
		else if (geometry_type == GEOMETRY_TYPE::SPHERE)
		{
			FetchSphereGeometryNormal(scene, hit_record, hit_instance, hit_position, hit_position_error, hit_geometry_normal);
		}
		else
		{
			// do nothing
		}

		const glm::mat4 &otw = scene.transforms[hit_instance.transform_idx];
		const glm::mat4 &wto = scene.i_transforms[hit_instance.transform_idx];
		
		*hit_position = TransformPosition(otw, *hit_position, hit_position_error);
		*hit_geometry_normal = glm::normalize(TransposeTransformVector(wto, *hit_geometry_normal));
	}

	__device__ __host__ inline bool BoundaryTransitionBunch(RayTransfer &ray_transfer,
		const PrimitiveInstance *prim_instances,
		const Material *materials,
		const NearbyHit nearby_hit_prims[],
		const int nearby_prim_count)
	{
		for (int nearby_hit_idx = 0; nearby_hit_idx < nearby_prim_count; ++nearby_hit_idx)
		{
			// note: process push first, later pop
			if (nearby_hit_prims[nearby_hit_idx].hit_back) {
				continue;
			}
			const PrimitiveInstance &hitted_prim = prim_instances[nearby_hit_prims[nearby_hit_idx].hit_prim_idx];
			const Material &mtl = materials[hitted_prim.material_idx];

			uint32_t mtl_ior_priority;
			const float mtl_ior = mtl.FetchIOR(&mtl_ior_priority);

			const int record_idx = ray_transfer.FindRecordWithPrim(nearby_hit_prims[nearby_hit_idx].hit_prim_idx);
			if (record_idx == -1) {
				ray_transfer.PushRecord(nearby_hit_prims[nearby_hit_idx].hit_prim_idx, mtl_ior, mtl_ior_priority, hitted_prim.inner_volume_idx);
			}
		}

		for (int nearby_hit_idx = 0; nearby_hit_idx < nearby_prim_count; ++nearby_hit_idx)
		{
			if (!nearby_hit_prims[nearby_hit_idx].hit_back) {
				continue;
			}
			const int record_idx = ray_transfer.FindRecordWithPrim(nearby_hit_prims[nearby_hit_idx].hit_prim_idx);
			if (record_idx != -1) {
				ray_transfer.PopRecord(record_idx);
			}
		}

		return true;
	}

	__device__  __host__ inline glm::vec3 OffsetRayOrigin(const glm::vec3 &p, const glm::vec3 &p_error, const glm::vec3 &new_dir, const glm::vec3 &geometry_normal)
	{
		const float distance = glm::dot(glm::abs(geometry_normal), p_error);
		glm::vec3 offset = distance * geometry_normal;
		if (glm::dot(new_dir, geometry_normal) < 0.0f) {
			offset = -offset;
		}

		glm::vec3 po = p + offset;
		for (int axis = 0; axis <= 2; ++axis) {
			if (offset[axis] > 0.0f) {
				po[axis] = NextFloatUp(po[axis]);
			}
			else if (offset[axis] < 0.0f) {
				po[axis] = NextFloatDown(po[axis]);
			}
		}
		return po;
	}
};