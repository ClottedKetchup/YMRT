#pragma once

#include <driver_types.h>

#include "GeometryDefines.h"
#include "SceneDefines.h"
#include "BoundingBox.h"
#include "MathCommon.h"
#include "Ray.h"
#include "BottomBVH.h"
#include "TopBVH.h"

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
		uint32_t hit_instance_idx = INVALID_UINT_32;

		// for mesh only
		uint32_t hit_triangle_idx = INVALID_UINT_32;
		int hit_back = 0;
		int volume_hit = 0;
		// may have to store if the ray hit the object back face
	};
#pragma pack(pop)

	// This is for Mesh intersection
	__device__ __host__  inline bool IntersectTri(const Triangle &triangle, const glm::vec3 *mesh_positions, const uint32_t *mesh_vidxs, const Ray &ray, HitRecord *hit_record)
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
		if (t < TMIN || t > ray.t)
		{
			return false;
		}

		ray.t = t;

		hit_record->hit_t = t;
		hit_record->hit_barycentric = glm::vec3(e0, e1, e2) * inv_denominator;
		hit_record->hit_back = glm::dot(geometry_normal, ray.direction) > 0.0f;

		return true;
	}

	__device__ __host__  inline bool IntersectMeshObject(const GeometryData &geometry, const Scene &scene, const Ray &ray, HitRecord *hit_record, bool test_any_hit)
	{
		if (geometry.tri_mesh.bottom_node_count == 0 || scene.bottom_nodes == nullptr)
		{
			return false;
		}

		const uint32_t stack_size = 32;
		uint32_t root_idx = 0;

		uint32_t stack[stack_size];
		int p_top = -1;

		const uint32_t *vidxs = scene.vidxs + geometry.tri_mesh.vidx_offset;
		const glm::vec3 *positions = scene.positions + geometry.tri_mesh.position_offset;
		const Triangle *triangles = scene.triangles + geometry.tri_mesh.triangle_offset;
		const BottomNode *bottom_nodes = scene.bottom_nodes + geometry.tri_mesh.bottom_node_offset;

		float t = 0.0f;
		bool hit = false;

		stack[++p_top] = root_idx;
		while (p_top != -1)
		{
			uint32_t node_idx = stack[p_top--];
			const BottomNode &node = bottom_nodes[node_idx];

			if (node.internal.left != INVALID_UINT_32)
			{
				const Triangle *tris = triangles + node.leaf.offset;
				for (uint32_t i = 0; i < node.leaf.count; ++i)
				{
					if (IntersectTri(tris[i], positions, vidxs, ray, hit_record))
					{
						hit_record->hit_triangle_idx = node.leaf.offset + i;
						hit = hit || true;
					}
					if (test_any_hit && hit) { return hit; }
				}
				continue;
			}

			uint32_t n_node = node_idx + 1;
			uint32_t f_node = node.internal.right;

			float tn, tf;
			bool bn = IntersectBBox3(bottom_nodes[n_node].bbox, ray, &tn);
			bool bf = IntersectBBox3(bottom_nodes[f_node].bbox, ray, &tf);
			if (tf < tn)
			{
				Swap(tf, tn);
				Swap(n_node, f_node);
				Swap(bn, bf);
			}

			if (bf)
			{
				stack[++p_top] = f_node;
				assert(p_top < stack_size);
			}
			if (bn)
			{
				stack[++p_top] = n_node;
				assert(p_top < stack_size);
			}
		}

		return hit;
	}

	__device__ __host__  inline bool IntersectSphereObject(const GeometryData &geometry, const Scene &scene, const Ray &ray, HitRecord *hit_record, bool test_any_hit)
	{
		float radius = geometry.sphere.radius;

		const glm::vec3 &ray_o = ray.origin;
		const glm::vec3 &ray_d = ray.direction;

		float a = ray_d.x * ray_d.x + ray_d.y * ray_d.y + ray_d.z * ray_d.z;
		float b = 2.0f * (ray_d.x * ray_o.x + ray_d.y * ray_o.y + ray_d.z * ray_o.z);
		float c = ray_o.x * ray_o.x + ray_o.y * ray_o.y + ray_o.z * ray_o.z - radius * radius;
		float discriminator = b * b - 4.0f * a * c;
		float t = 0.f;

		if (discriminator < 0)
		{
			return false;
		}

		float sqrt_d = glm::sqrt(discriminator);
		float t1 = (-b - sqrt_d) / (2.0 * a);
		float t2 = (-b + sqrt_d) / (2.0 * a);

		if (t1 > ray.t || t2 < TMIN)
		{
			return false;
		}

		t = t1;
		if (t < TMIN)
		{
			t = t2;
			if (t > ray.t)
			{
				return false;
			}
		}

		ray.t = t;

		glm::vec3 position_object = ray_o + ray_d * t;
		float distance = length(position_object);
		position_object = distance > 1E-8f ?
			position_object * radius / distance : glm::vec3(0.0f);

		hit_record->hit_t = t;
		hit_record->hit_barycentric = position_object;
		hit_record->hit_back = glm::dot(ray_d, position_object / radius) > 0.0f;
		return true;
	}

	__device__ __host__  inline bool IntersectGeometry(const GeometryData &geometry, const Scene &scene, const Ray &ray, HitRecord *hit_record, bool test_any_hit)
	{
		uint32_t geo_type = geometry.geometry_type;
		if (geo_type == GEOMETRY_TYPE::TRIANGLE_MESH)
		{
			return IntersectMeshObject(geometry, scene, ray, hit_record, test_any_hit);
		}
		else if (geo_type == GEOMETRY_TYPE::SPHERE)
		{
			return IntersectSphereObject(geometry, scene, ray, hit_record, test_any_hit);
		}
		else
		{
			return false;
		}
	}

	__device__ __host__  inline bool BVHTraverse(const Scene &scene, const Ray &ray, HitRecord *hit_record)
	{
		if (scene.top_node_count == 0 || scene.top_nodes == nullptr) { return false; }

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

			if (node.internal.left != INVALID_UINT_32)
			{
				PrimitiveInstance *prim_instances = scene.prim_instances + node.leaf.offset;
				for (uint32_t i = 0; i < node.leaf.count; ++i)
				{
					const PrimitiveInstance &prim = prim_instances[i];
					const glm::mat4 &i_transform = scene.i_transforms[prim.transform_idx];
					Ray object_ray = TransformRay(ray, i_transform);
					const GeometryData &geometry = scene.geometries[prim.geometry_idx];
					if (IntersectGeometry(geometry, scene, object_ray, hit_record, false))
					{
						hit_record->hit_instance_idx = node.leaf.offset + i;
						hit = hit || true;
						ray.t = object_ray.t;
					}
				}
				continue;
			}

			uint32_t n_node = node_idx + 1;
			uint32_t f_node = node.internal.right;

			float tn, tf;
			bool bn = IntersectBBox3(scene.top_nodes[n_node].bbox, ray, &tn);
			bool bf = IntersectBBox3(scene.top_nodes[f_node].bbox, ray, &tf);
			if (tf < tn)
			{
				Swap(tf, tn);
				Swap(n_node, f_node);
				Swap(bn, bf);
			}

			if (bf)
			{
				stack[++p_top] = f_node;
				assert(p_top < stack_size);
			}
			if (bn)
			{
				stack[++p_top] = n_node;
				assert(p_top < stack_size);
			}
		}

		return hit;
	}

	// TODO: you should construst a separate BVH for volume traverse!
	__device__ __host__  inline bool ClosestVolume(const Scene &scene, const Ray &ray, HitRecord *hit_record)
	{
		if (scene.top_node_count == 0 || scene.top_nodes == nullptr || scene.volume_count == 0) { return false; }

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

			if (node.internal.left != INVALID_UINT_32)
			{
				PrimitiveInstance *prim_instances = scene.prim_instances + node.leaf.offset;
				for (uint32_t i = 0; i < node.leaf.count; ++i)
				{
					if (!prim_instances[i].is_volume_boundary) { continue; }

					const PrimitiveInstance &prim = prim_instances[i];
					const glm::mat4 &i_transform = scene.i_transforms[prim.transform_idx];
					Ray object_ray = TransformRay(ray, i_transform);
					const GeometryData &geometry = scene.geometries[prim.geometry_idx];
					if (IntersectGeometry(geometry, scene, object_ray, hit_record, false))
					{
						hit_record->hit_instance_idx = node.leaf.offset + i;
						hit = hit || true;
						ray.t = object_ray.t;
					}
				}
				continue;
			}

			uint32_t n_node = node_idx + 1;
			uint32_t f_node = node.internal.right;

			float tn, tf;
			bool bn = IntersectBBox3(scene.top_nodes[n_node].bbox, ray, &tn);
			bool bf = IntersectBBox3(scene.top_nodes[f_node].bbox, ray, &tf);
			if (tf < tn)
			{
				Swap(tf, tn);
				Swap(n_node, f_node);
				Swap(bn, bf);
			}

			if (bf)
			{
				stack[++p_top] = f_node;
				assert(p_top < stack_size);
			}
			if (bn)
			{
				stack[++p_top] = n_node;
				assert(p_top < stack_size);
			}
		}

		return hit;
	}

	__device__ __host__  inline bool BVHTraverseShadow(const Scene &scene, const Ray &ray, HitRecord *hit_record)
	{
		if (scene.top_node_count == 0 || scene.top_nodes == nullptr) { return false; }

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

			if (node.internal.left != INVALID_UINT_32)
			{
				PrimitiveInstance *prim_instances = scene.prim_instances + node.leaf.offset;
				for (uint32_t i = 0; i < node.leaf.count; ++i)
				{
					// skip the test of volume boundary
					if (prim_instances[i].is_volume_boundary) { continue; }

					const glm::mat4 &i_transform = scene.i_transforms[prim_instances[i].transform_idx];
					Ray object_ray = TransformRay(ray, i_transform);
					const GeometryData &geometry = scene.geometries[prim_instances[i].geometry_idx];
					if (IntersectGeometry(geometry, scene, object_ray, hit_record, true /* test any hit */))
					{
						hit_record->hit_instance_idx = node.leaf.offset + i;
						hit = hit || true;
						ray.t = object_ray.t;
						return hit;
					}
				}
				continue;
			}

			uint32_t n_node = node_idx + 1;
			uint32_t f_node = node.internal.right;

			float tn, tf;
			bool bn = IntersectBBox3(scene.top_nodes[n_node].bbox, ray, &tn);
			bool bf = IntersectBBox3(scene.top_nodes[f_node].bbox, ray, &tf);
			if (tf < tn)
			{
				Swap(tf, tn);
				Swap(n_node, f_node);
				Swap(bn, bf);
			}

			if (bf)
			{
				stack[++p_top] = f_node;
				assert(p_top < stack_size);
			}
			if (bn)
			{
				stack[++p_top] = n_node;
				assert(p_top < stack_size);
			}
		}

		return hit;
	}



	__device__ __host__ inline void FetchSphereShadingData(const Scene &scene,
																					 const HitRecord &hit_record,
																					 const PrimitiveInstance &hit_instance,
																					 glm::vec3 *hit_position,
																					 glm::vec3 *hit_shading_normal,
																					 glm::vec3 *hit_geometry_normal,
																					 glm::vec2 *hit_uv,
																					 glm::vec3 *hit_dpdu,
																					 glm::vec3 *hit_dpdv)
	{
		const GeometryData &hit_sphere = scene.geometries[hit_instance.geometry_idx];

		float radius = glm::max(hit_sphere.sphere.radius, 1E-8f);

		glm::vec3 position_object = hit_record.hit_barycentric;
		glm::vec3 normal_object = hit_record.hit_barycentric / radius;

		*hit_position = position_object;
		*hit_shading_normal = normal_object;
		*hit_geometry_normal = normal_object;

		float theta = glm::acos(normal_object.y);
		float phi = glm::atan(normal_object.z, normal_object.x);
		if (phi < 0.0f) { phi += TWO_PI; }
		*hit_uv = glm::vec2(0.5f * phi * INV_PI, theta * INV_PI);

		float sin_theta = 0.0f;
		if (1.0f - normal_object.y * normal_object.y > 0.0f)
		{
			sin_theta = glm::sqrt(1.0f - normal_object.y * normal_object.y);
		}

		float cos_phi = 0.0f, sin_phi = 0.0f;
		float r_xz = sin_theta;
		if (r_xz > 0.0f)
		{
			cos_phi = normal_object.x / r_xz;
			sin_phi = normal_object.z / r_xz;
		}
		else 
		{
			cos_phi = 1.0f, sin_phi = 0.0f;
		}

		*hit_dpdu = glm::vec3(-position_object.z, 0.0f, position_object.x) * TWO_PI;
		*hit_dpdv = glm::vec3(position_object.y * cos_phi, -radius * sin_theta, sin_phi * position_object.y) * ONE_PI;
	}

	__device__ __host__ inline void FetchMeshShadingData(const Scene &scene,
																				   const HitRecord &hit_record,
																				   const PrimitiveInstance &hit_instance,
																				   glm::vec3 *hit_position,
																				   glm::vec3 *hit_shading_normal,
																				   glm::vec3 *hit_geometry_normal,
																				   glm::vec2 *hit_uv,
																				   glm::vec3 *hit_dpdu,
																				   glm::vec3 *hit_dpdv)
	{
		const GeometryData &hit_mesh = scene.geometries[hit_instance.geometry_idx];
		
		const glm::vec3 *mesh_positions = scene.positions + hit_mesh.tri_mesh.position_offset;
		const glm::vec3 *mesh_normals = scene.normals + hit_mesh.tri_mesh.normal_offset;
		const uint32_t *mesh_vidxs = scene.vidxs + hit_mesh.tri_mesh.vidx_offset;
		const uint32_t *mesh_nidxs = scene.nidxs + hit_mesh.tri_mesh.nidx_offset;

		const Triangle *mesh_triangles = scene.triangles + hit_mesh.tri_mesh.triangle_offset;
		const Triangle &triangle = mesh_triangles[hit_record.hit_triangle_idx];
		
		uint32_t vid0 = mesh_vidxs[triangle.id0];
		uint32_t vid1 = mesh_vidxs[triangle.id1];
		uint32_t vid2 = mesh_vidxs[triangle.id2];

		const glm::vec3 &p0 = mesh_positions[vid0];
		const glm::vec3 &p1 = mesh_positions[vid1];
		const glm::vec3 &p2 = mesh_positions[vid2];

		*hit_position = p0 * hit_record.hit_barycentric.x
							 + p1 * hit_record.hit_barycentric.y
							 + p2 * hit_record.hit_barycentric.z;

		uint32_t nid0 = mesh_nidxs[triangle.id0];
		uint32_t nid1 = mesh_nidxs[triangle.id1];
		uint32_t nid2 = mesh_nidxs[triangle.id2];

		const glm::vec3 &n0 = mesh_normals[nid0];
		const glm::vec3 &n1 = mesh_normals[nid1];
		const glm::vec3 &n2 = mesh_normals[nid2];

		*hit_shading_normal = n0 * hit_record.hit_barycentric.x
																   + n1 * hit_record.hit_barycentric.y
																   + n2 * hit_record.hit_barycentric.z;
		*hit_geometry_normal = glm::cross(p1 - p0, p2 - p0);

		bool has_custom_uv = (hit_mesh.tri_mesh.texcoord_count > 0);

		const uint32_t *mesh_uvidxs = has_custom_uv?
			(scene.uvidxs + hit_mesh.tri_mesh.uvidx_offset) : nullptr;
		const glm::vec2 *mesh_texcoords = has_custom_uv ?
			(scene.texcoords + hit_mesh.tri_mesh.texcoord_offset) : nullptr;
		
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
		float delta_u12 = uv2.x - uv1.x, delta_v12 = uv2.y - uv1.y;
		float determinant = delta_u01 * delta_v12 - delta_v01 * delta_u12;
		if (glm::abs(determinant) < 1E-7f)
		{
			delta_u01 = 1.0f, delta_v01 = 0.0f;
			delta_u12 = 0.0f, delta_v12 = 1.0f;
			determinant = 1.0f;
		}

		glm::vec3 delta_p01 = p1 - p0, delta_p12 = p2 - p1;
		float i_det = 1.0f / determinant;
		*hit_dpdu = (delta_v12 * delta_p01 - delta_v01 * delta_p12) * i_det;
		*hit_dpdv = (-delta_u12 * delta_p01 + delta_u01 * delta_p12) * i_det;
	}

	__device__ __host__ inline void FetchShadingData(const Scene &scene,
																					const HitRecord &hit_record,
																					glm::vec3 *hit_position,
																					glm::vec3 *hit_position_object_space,
																					glm::vec3 *hit_shading_normal,
																					glm::vec3 *hit_geometry_normal,
																					glm::vec2 *hit_uv,
																					glm::vec3 *hit_dpdu,
																					glm::vec3 *hit_dpdv)
	{
		if (hit_record.hit_instance_idx == INVALID_UINT_32) { return; }
		const PrimitiveInstance &hit_instance = scene.prim_instances[hit_record.hit_instance_idx];
		const GeometryData &hit_geometry = scene.geometries[hit_instance.geometry_idx];
		uint32_t geometry_type = hit_geometry.geometry_type;
		if (geometry_type == GEOMETRY_TYPE::TRIANGLE_MESH)
		{
			FetchMeshShadingData(scene,
													hit_record,
													hit_instance,
													hit_position,
													hit_shading_normal,
													hit_geometry_normal,
													hit_uv,
													hit_dpdu,
													hit_dpdv);
		}
		else if (geometry_type == GEOMETRY_TYPE::SPHERE)
		{
			FetchSphereShadingData(scene,
													  hit_record,
													  hit_instance,
													  hit_position,
													  hit_shading_normal,
													  hit_geometry_normal,
													  hit_uv,
													  hit_dpdu,
													  hit_dpdv);
		}
		else 
		{
			// nothing to do
		}

		// transform geometry data
		const glm::mat4 &transform = scene.transforms[hit_instance.transform_idx];
		const glm::mat4 &i_transform = scene.i_transforms[hit_instance.transform_idx];

		auto otw_position = [&](const glm::vec3 &pos)->glm::vec3 
		{
			return glm::vec3(transform * glm::vec4(pos, 1.0f));
		};
		auto otw_vector = [&](const glm::vec3 &vec)->glm::vec3
		{
			return glm::vec3(transform * glm::vec4(vec, 0.0f));
		};
		auto otw_normal = [&](const glm::vec3 &n)->glm::vec3
		{
			return glm::vec3(glm::transpose(i_transform) * glm::vec4(n, 0.0f));
		};

		*hit_position_object_space = *hit_position;
		*hit_position = otw_position(*hit_position);
		*hit_shading_normal = glm::normalize(otw_normal(*hit_shading_normal));
		*hit_geometry_normal = glm::normalize(otw_normal(*hit_geometry_normal));
		*hit_dpdu = otw_vector(*hit_dpdu);
		*hit_dpdv = otw_vector(*hit_dpdv);

		// TODO: maybe flipped the shading normal to match geometry normal
	}

	__device__ __host__ inline void FetchSphereGeometryNormal(const Scene &scene,
																								const HitRecord &hit_record, 
																								const PrimitiveInstance &hit_instance, 
																								glm::vec3 *hit_position, 
																								glm::vec3 *hit_geometry_normal)
	{
		const GeometryData &hit_sphere = scene.geometries[hit_instance.geometry_idx];

		float radius = glm::max(hit_sphere.sphere.radius, 1E-8f);

		*hit_position = hit_record.hit_barycentric;

		*hit_geometry_normal = hit_record.hit_barycentric / radius;
	}

	__device__ __host__ inline void FetchMeshGeometryNormal(const Scene &scene,
																								const HitRecord &hit_record, 
																								const PrimitiveInstance &hit_instance,
																								glm::vec3 *hit_position,
																								glm::vec3 *hit_geometry_normal)
	{
		const GeometryData &hit_mesh = scene.geometries[hit_instance.geometry_idx];

		const glm::vec3 *mesh_positions = scene.positions + hit_mesh.tri_mesh.position_offset;
		const uint32_t *mesh_vidxs = scene.vidxs + hit_mesh.tri_mesh.vidx_offset;

		const Triangle *mesh_triangles = scene.triangles + hit_mesh.tri_mesh.triangle_offset;
		const Triangle &triangle = mesh_triangles[hit_record.hit_triangle_idx];

		uint32_t vid0 = mesh_vidxs[triangle.id0];
		uint32_t vid1 = mesh_vidxs[triangle.id1];
		uint32_t vid2 = mesh_vidxs[triangle.id2];

		const glm::vec3 &p0 = mesh_positions[vid0];
		const glm::vec3 &p1 = mesh_positions[vid1];
		const glm::vec3 &p2 = mesh_positions[vid2];

		*hit_position = p0 * hit_record.hit_barycentric.x
							+ p1 * hit_record.hit_barycentric.y
							+ p2 * hit_record.hit_barycentric.z;

		*hit_geometry_normal = glm::cross(p1 - p0, p2 - p0);
	}

	__device__ __host__ inline void FetchGeometryNormal(const Scene &scene, 
																						const HitRecord &hit_record, 
																						glm::vec3 *hit_position,
																						glm::vec3 *hit_geometry_normal)
	{
		if (hit_record.hit_instance_idx == INVALID_UINT_32) { return; }
		const PrimitiveInstance &hit_instance = scene.prim_instances[hit_record.hit_instance_idx];
		const GeometryData &hit_geometry = scene.geometries[hit_instance.geometry_idx];
		uint32_t geometry_type = hit_geometry.geometry_type;

		if (geometry_type == GEOMETRY_TYPE::TRIANGLE_MESH)
		{
			FetchMeshGeometryNormal(scene, hit_record, hit_instance, hit_position, hit_geometry_normal);
		}
		else if (geometry_type == GEOMETRY_TYPE::SPHERE)
		{
			FetchSphereGeometryNormal(scene, hit_record, hit_instance, hit_position, hit_geometry_normal);
		}
		else
		{
			// do nothing
		}

		const glm::mat4 &i_transform = scene.i_transforms[hit_instance.transform_idx];
		const glm::mat4 &transform = scene.transforms[hit_instance.transform_idx];

		*hit_position = glm::vec3(transform * glm::vec4((*hit_position), 1.0f));

		*hit_geometry_normal = glm::normalize(glm::vec3(glm::transpose(i_transform) * glm::vec4((*hit_geometry_normal), 0.0f)));
	}

	__device__  __host__ inline glm::vec3 OffsetRayOrigin(const glm::vec3& p, const glm::vec3 &n)
	{
		constexpr float origin = 1.0f / 32.0f; 
		constexpr float float_scale = 1.0f / 65536.0f;
		constexpr float int_scale = 256.0f;

		int of_ix = int_scale * n.x;
		int of_iy = int_scale * n.y;
		int of_iz = int_scale * n.z;
	
		glm::vec3 p_i;
		p_i.x = IntToFloat(FloatToInt(p.x) + ((p.x < 0) ? -of_ix : of_ix));
		p_i.y = IntToFloat(FloatToInt(p.y) + ((p.y < 0) ? -of_iy : of_iy));
		p_i.z = IntToFloat(FloatToInt(p.z) + ((p.z < 0) ? -of_iz : of_iz));
	
		return glm::vec3(glm::abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x,
								   glm::abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y,
								   glm::abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z);
	}
};