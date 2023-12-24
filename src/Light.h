#pragma once

#include <driver_types.h>

#include "MathCommon.h"
#include "PrimtiveInstance.h"
#include "GeometryDefines.h"
#include "GeometryUtilities.h"
#include "SampleUtilities.h"

namespace YumeRT
{

	__device__ __host__ inline bool RayIntersectTriangle(const glm::vec3 &ray_origin,
																					    const glm::vec3 &ray_direction, 
																						const glm::vec3 &p0, 
																						const glm::vec3 &p1, 
																						const glm::vec3 &p2,
																						glm::vec3 *barycentric)
	{
		const glm::vec3 p20 = p0 - p2;
		const glm::vec3 p21 = p1 - p2;
		
		glm::mat3 m;
		m[0][0] = p20.x;
		m[0][1] = p20.y;
		m[0][2] = p20.z;
		m[1][0] = p21.x;
		m[1][1] = p21.y;
		m[1][2] = p21.z;
		m[2][0] = -ray_direction.x;
		m[2][1] = -ray_direction.y;
		m[2][2] = -ray_direction.z;

		glm::vec3 res = glm::inverse(m) * (ray_origin - p2);

		float u = res.x;
		float v = res.y;
		float t = res.z;
		float w = 1.0f - u - v;

		if (t < TMIN) 
		{ 
			return false;
		}

		*barycentric = glm::vec3(u, v, w);

		return (u > 0.0f && u < 1.0f) && (v > 0.0f && v < 1.0f) && (w > 0.0f && w < 1.0f);
	}

	__device__ __host__ inline bool RayIntersectSphere(const glm::vec3 &ray_origin,
																					  const glm::vec3 &ray_direction,
																					  const glm::vec3 &sphere_center,
																					  float sphere_radius,
																					  float *t0,
																					  float *t1)
	{
		float a = ray_direction.x * ray_direction.x + ray_direction.y * ray_direction.y + ray_direction.z * ray_direction.z;
		float b = 2.0f * (ray_direction.x * ray_origin.x + ray_direction.y * ray_origin.y + ray_direction.z * ray_origin.z);
		float c = ray_origin.x * ray_origin.x + ray_origin.y * ray_origin.y + ray_origin.z * ray_origin.z - sphere_radius * sphere_radius;
		float discriminator = b * b - 4.0f * a * c;
		float t = 0.f;

		if (discriminator < 0)
		{
			return false;
		}

		float sqrt_discriminator = glm::sqrt(discriminator);
		*t0 = (-b - sqrt_discriminator) / (2.0 * a);
		*t1 = (-b + sqrt_discriminator) / (2.0 * a);
		if ((*t1) < TMIN) { return false; }
		return true;
	}


	struct ShapeLight
	{
		uint32_t instance_idx;
		uint32_t triangle_idx;
		float power;
		float area;

		__device__ __host__ ShapeLight() {}

		__device__ __host__ inline float PDF(const Scene &scene, const glm::vec3 &position, const glm::vec3 &wi) const
		{
			return 0.0f;
		}

		__device__ __host__ inline glm::vec3 EvalLi(const Scene &scene, 
			const glm::vec3 &position,
			const glm::vec3 &wi, 
			glm::vec3 *light_sample_pos, 
			glm::vec3 *light_sample_geo_normal,
			float *pdf) const
		{
			const PrimitiveInstance &prim = scene.prim_instances[instance_idx];
			const Material &light_mtl = scene.materials[prim.material_idx];
			const GeometryData &geometry = scene.geometries[prim.geometry_idx];

			const glm::mat4 &otw = scene.transforms[prim.transform_idx];
			const glm::mat4 &wto = scene.i_transforms[prim.transform_idx];

		
			if (geometry.geometry_type == TRIANGLE_MESH)
			{
				const glm::vec3 *mesh_positions = scene.positions + geometry.tri_mesh.position_offset;
				const uint32_t *mesh_vidxs = scene.vidxs + geometry.tri_mesh.vidx_offset;
				const Triangle *mesh_triangles = scene.triangles + geometry.tri_mesh.triangle_offset;

				const Triangle &triangle = mesh_triangles[triangle_idx];

				uint32_t vid0 = mesh_vidxs[triangle.id0];
				uint32_t vid1 = mesh_vidxs[triangle.id1];
				uint32_t vid2 = mesh_vidxs[triangle.id2];

				const glm::vec3 &p0 = mesh_positions[vid0];
				const glm::vec3 &p1 = mesh_positions[vid1];
				const glm::vec3 &p2 = mesh_positions[vid2];
				const glm::vec3 geo_normal = glm::cross(p1 - p0, p2 - p0);

				const glm::vec3 object_space_wi = glm::vec3(wto * glm::vec4(wi, 0.0f));
				const glm::vec3 object_space_position = glm::vec3(wto * glm::vec4(position, 1.0f));

				glm::vec3 barycentric(0.0f);
				bool hit_light = RayIntersectTriangle(object_space_position, object_space_wi, p0, p1, p2, &barycentric);
				if (!hit_light)
				{
					*pdf = 0.0f;
					return glm::vec3(0.0f);
				}

				const glm::vec3 *mesh_normals = scene.normals + geometry.tri_mesh.normal_offset;
				const uint32_t *mesh_nidxs = scene.nidxs + geometry.tri_mesh.nidx_offset;

				uint32_t nid0 = mesh_nidxs[triangle.id0];
				uint32_t nid1 = mesh_nidxs[triangle.id1];
				uint32_t nid2 = mesh_nidxs[triangle.id2];

				const glm::vec3 &n0 = mesh_normals[nid0];
				const glm::vec3 &n1 = mesh_normals[nid1];
				const glm::vec3 &n2 = mesh_normals[nid2];

				glm::vec3 sample_position = p0 * barycentric.x + p1 * barycentric.y + p2 * barycentric.z;
				glm::vec3 sample_normal = n0 * barycentric.x + n1 * barycentric.y + n2 * barycentric.z;
				glm::vec3 world_space_normal = glm::normalize(glm::vec3(glm::transpose(wto) * glm::vec4(sample_normal, 0.0f)));
				if (-glm::dot(world_space_normal, wi) <= 0.0f)
				{
					*pdf = 0.0f;
					return glm::vec3(0.0f);
				}

				*light_sample_pos = glm::vec3(otw * glm::vec4(sample_position, 1.0f));
				*light_sample_geo_normal = glm::normalize(glm::vec3(glm::transpose(wto) * glm::vec4(geo_normal, 0.0f)));
				*pdf = glm::dot((*light_sample_pos - position), (*light_sample_pos - position)) / (glm::max(-glm::dot(world_space_normal, wi) * area, 1E-8f));
				return light_mtl.light_material.light_color * light_mtl.light_material.intensity;
			}
			else if(geometry.geometry_type == SPHERE)
			{
				const glm::vec3 object_space_wi = glm::vec3(wto * glm::vec4(wi, 0.0f));
				const glm::vec3 object_space_position = glm::vec3(wto * glm::vec4(position, 1.0f));
				
				float length_po = glm::length(object_space_position);
				float radius = geometry.sphere.radius;
				if (length_po < radius)
				{
					*pdf = 0.0f;
					return glm::vec3(0.0f);
				}

				float t0, t1;
				bool hit_light = RayIntersectSphere(object_space_position, object_space_wi, glm::vec3(0.0f), radius, &t0, &t1);
				if (!hit_light) 
				{
					*pdf = 0.0f;
					return glm::vec3(0.0f);
				}

				float cos_theta_max = glm::sqrt(glm::max(Sqr(length_po) - Sqr(radius), 0.0f)) / length_po;
				float cone_solid_angle = 2.0f * ONE_PI * (1.0f - cos_theta_max);
				float cos_theta = glm::dot(glm::normalize(object_space_wi), -object_space_position / length_po);
				if (cos_theta < cos_theta_max)
				{
					*pdf = 0.0f;
					return glm::vec3(0.0f);
				}

				glm::vec3 intersect_pos = object_space_position + object_space_wi * t0;
				float length_io = glm::length(intersect_pos);
				intersect_pos = length_io > 1E-8f ? (intersect_pos * radius / length_io) : glm::vec3(0.0f);

				*light_sample_pos = glm::vec3(otw * glm::vec4(intersect_pos, 1.0f));
				*light_sample_geo_normal = ((*light_sample_pos) - glm::vec3(otw[3][0], otw[3][1], otw[3][2])) / radius;
				*pdf = 1.0f / cone_solid_angle;
				return light_mtl.light_material.light_color * light_mtl.light_material.intensity;
			}
			else 
			{
				return glm::vec3(0.0f);
			}
		}

		__device__ __host__ inline glm::vec3 SampleLi(const Scene &scene,
																				const glm::vec3& position,
																				const glm::vec3 &normal,
																				float u0,
																				float u1,
																				glm::vec3 *wi,
																				glm::vec3 *light_sample_pos,
																				glm::vec3 *light_sample_geo_normal,
																				float *pdf) const
		{
			const PrimitiveInstance &prim = scene.prim_instances[instance_idx];
			const Material &light_mtl = scene.materials[prim.material_idx];
			const GeometryData &geometry = scene.geometries[prim.geometry_idx];

			const glm::mat4 &otw = scene.transforms[prim.transform_idx];
			const glm::mat4 &wto = scene.i_transforms[prim.transform_idx];

			if (geometry.geometry_type == TRIANGLE_MESH)
			{
				// TODO: fix bug
				const glm::vec3 *mesh_positions = scene.positions + geometry.tri_mesh.position_offset;
				const uint32_t *mesh_vidxs = scene.vidxs + geometry.tri_mesh.vidx_offset;
				const Triangle *mesh_triangles = scene.triangles + geometry.tri_mesh.triangle_offset;

				const Triangle &triangle = mesh_triangles[triangle_idx];
				
				uint32_t vid0 = mesh_vidxs[triangle.id0];
				uint32_t vid1 = mesh_vidxs[triangle.id1];
				uint32_t vid2 = mesh_vidxs[triangle.id2];

				const glm::vec3 &p0 = mesh_positions[vid0];
				const glm::vec3 &p1 = mesh_positions[vid1];
				const glm::vec3 &p2 = mesh_positions[vid2];
				const glm::vec3 geo_normal = glm::cross(p1 - p0, p2 - p0);

				const glm::vec3 *mesh_normals = scene.normals + geometry.tri_mesh.normal_offset;
				const uint32_t *mesh_nidxs = scene.nidxs + geometry.tri_mesh.nidx_offset;

				uint32_t nid0 = mesh_nidxs[triangle.id0];
				uint32_t nid1 = mesh_nidxs[triangle.id1];
				uint32_t nid2 = mesh_nidxs[triangle.id2];

				const glm::vec3 &n0 = mesh_normals[nid0];
				const glm::vec3 &n1 = mesh_normals[nid1];
				const glm::vec3 &n2 = mesh_normals[nid2];

				glm::vec3 barycentric = SampleTriangle(u0, u1);
				glm::vec3 sample_position = p0 * barycentric.x + p1 * barycentric.y + p2 * barycentric.z;
				glm::vec3 sample_normal = n0 * barycentric.x + n1 * barycentric.y + n2 * barycentric.z;
				glm::vec3 world_space_normal = glm::normalize(glm::vec3(glm::transpose(wto) * glm::vec4(sample_normal, 0.0f)));

				*light_sample_pos = glm::vec3(otw * glm::vec4(sample_position, 1.0f));
				*light_sample_geo_normal = glm::normalize(glm::vec3(glm::transpose(wto) * glm::vec4(geo_normal, 0.0f)));
				*wi = glm::normalize(*light_sample_pos - position);
				*pdf = glm::dot((*light_sample_pos - position), (*light_sample_pos - position)) / (glm::max(-area * glm::dot(world_space_normal, *wi), 1E-8f));

				return  -glm::dot(world_space_normal, *wi) > 0.0f? light_mtl.light_material.light_color * light_mtl.light_material.intensity / (*pdf) : glm::vec3(0.0f);
			}
			else if (geometry.geometry_type == SPHERE)
			{
				glm::vec3 object_space_position = glm::vec3(wto * glm::vec4(position, 1.0f));
				float length_po = glm::length(object_space_position);

				float radius = geometry.sphere.radius;
				if (length_po < radius) 
				{
					*pdf = 0.0f;
					return glm::vec3(0.0f);
				}

				float cos_theta_max = glm::sqrt(glm::max(Sqr(length_po) - Sqr(radius), 0.0f)) / length_po;
				float cone_solid_angle = 2.0f * ONE_PI * (1.0f - cos_theta_max);

				glm::vec3 cone_sample = SampleCone(u0, u1, cos_theta_max);
				glm::vec3 w = -object_space_position, u, v;
				OrthogonalBasis(w, u, v);
				cone_sample = cone_sample.x * u + cone_sample.y * v + cone_sample.z * w;

				float t0, t1;
				RayIntersectSphere(object_space_position, cone_sample, glm::vec3(0.0f), radius, &t0, &t1);
				glm::vec3 intersect_pos = object_space_position + cone_sample * t0;
				float length_io = glm::length(intersect_pos);
				intersect_pos = length_io > 1E-8f ? (intersect_pos * radius / length_io) : glm::vec3(0.0f);

				*light_sample_pos = glm::vec3(otw * glm::vec4(intersect_pos, 1.0f));
				*light_sample_geo_normal = ((*light_sample_pos) - glm::vec3(otw[3][0], otw[3][1], otw[3][2])) / radius;
				*wi = glm::normalize(glm::vec3(otw * glm::vec4(cone_sample, 0.0f)));
				*pdf = 1.0f / cone_solid_angle;
				return light_mtl.light_material.light_color * light_mtl.light_material.intensity * cone_solid_angle;
			}
			else
			{
				return glm::vec3(0.0f);
			}
		}
	};
	// TODO: re design DistantLight's info and make it interactable
	struct DistantLight
	{
		glm::vec3 light_color;
		uint32_t transform_idx;
		float intensity;
		float cos_theta_max;
		float theta_max;
		float padding;

		// dir always toward the light, wi!
		__device__ __host__ DistantLight() {}
		__device__ __host__ inline float PDF(const Scene &scene, const glm::vec3 &wi) const
		{
			return 0.0f;
		}
		__device__ __host__ inline glm::vec3 EvalLi(const Scene &scene, const glm::vec3 &wi, float *pdf) const
		{
			const glm::mat3 i_transform = glm::mat3(scene.i_transforms[transform_idx]);
			const glm::vec3 local_wi = i_transform * wi;

			float cos_theta = -local_wi.y;
			if (cos_theta < cos_theta_max) 
			{
				*pdf = 0.0f;
				return glm::vec3(0.0f);
			}
			float cone_solid_angle = 2.0f * ONE_PI * (1.0f - cos_theta_max);
			*pdf = 1.0f / cone_solid_angle;
			return light_color * intensity * (*pdf);
		}

		// return light's energy
		__device__ __host__ inline glm::vec3 SampleLi(const Scene &scene,
																				 const glm::vec3& position, 
																				 const glm::vec3 &normal, 
																				 float u0, 
																				 float u1,
																				 glm::vec3 *wi, 
																				 glm::vec3 *light_sample_pos, 
																				 float *pdf) const
		{
			const glm::mat4 &transform = scene.transforms[transform_idx];

			float cone_solid_angle = 2.0f * ONE_PI * (1.0f - cos_theta_max);
			
			*pdf = 1.0f / cone_solid_angle;
			
			glm::vec3 cone_sample = SampleCone(u0, u1, cos_theta_max);
			
			cone_sample = glm::vec3(cone_sample.x, cone_sample.z, cone_sample.y);
			
			*wi = -glm::normalize(glm::mat3(transform) * cone_sample);

			return light_color * intensity;
		}

	};

	__device__ __host__ inline int SampleLightUniform(const Scene &scene, float u0, float *pdf)
	{
		int light_idx = (int)(u0 * scene.shape_light_count);
		light_idx = glm::clamp(light_idx, 0, (int)scene.shape_light_count - 1);
		*pdf = 1.0f / float(scene.shape_light_count);
		return light_idx;
	}

	__device__ __host__ inline int SampleLightPower(const Scene &scene, float u0, float *pdf)
	{
		auto search = [&](float *sample_table, int count, float u)->int 
		{
			int left = 0, right = count;
			while (left < right)
			{
				int mid = (left + right) / 2;
				if (sample_table[mid] <= u) { left = mid + 1; }
				else { right = mid; }
			}
			return left;
		};
		int idx = search(scene.shape_light_sample_table, scene.shape_light_count, u0);
		*pdf = idx == 0 ? scene.shape_light_sample_table[idx] : (scene.shape_light_sample_table[idx] - scene.shape_light_sample_table[idx - 1]);
		return glm::min(idx, (int)scene.shape_light_count - 1);
	}
};