#pragma once

#include <driver_types.h>

#include "SceneDefines.h"
#include "Ray.h"
#include "Volume.h"
#include "Texture.h"

#include "RandomSampler.cuh"
#include "ShadingUtilities.cuh"
#include "TextureEval.cuh"

namespace YumeRT {

#define MAX_FREE_PATH_LENGTH 2048

	__device__ __host__ inline bool SampleTrHomogeneous(const Scene& scene,
		const ImageTileCache& image_tile_cache,
		const Volume &vol,
		const Ray& ray,
		const float t_max,
		glm::vec3 *tr_weight,
		float *sampled_distance,
		RandomSampler &sampler)
	{
		*tr_weight = glm::vec3(1.0f);
		auto random = [&]()->float {return sampler.Random1D(); };

		glm::vec3 sigma_t = vol.sigma_a + vol.sigma_s;
		float channel_sum = sigma_t.x + sigma_t.y + sigma_t.z;
		glm::vec3 channel_weight = channel_sum > 0.0f ? (sigma_t / channel_sum) : glm::vec3(1.0f / 3.0f);

		const float u0 = random();
		const float u1 = random();
		int channel = 0;
		if (u0 < channel_weight[0]) { channel = 0; }
		else if (u0 < channel_weight[0] + channel_weight[1]) { channel = 1; }
		else { channel = 2; }

		float free_path_length = sigma_t[channel] > 0.0f ? (-glm::log(1.0f - u1) / sigma_t[channel]) : 1E36f;
		bool volume_scatter = free_path_length < t_max;
		glm::vec3 tr = glm::exp(-sigma_t * glm::min(t_max, free_path_length));
		float pdf = volume_scatter ?
			(channel_weight.x * sigma_t.x * tr.x + channel_weight.y * sigma_t.y * tr.y + channel_weight.z * sigma_t.z * tr.z) :
			(channel_weight.x * tr.x + channel_weight.y * tr.y + channel_weight.z * tr.z);

		if (glm::abs(pdf) == 0.0f) { return false; }

		// don't forget to multiply the sigma_s
		*tr_weight = volume_scatter ? (tr * vol.sigma_s / pdf) : (tr / pdf);
		*sampled_distance = free_path_length;
		return volume_scatter;
	}

	__device__ __host__ inline bool SampleTrHeterogeneous(const Scene& scene,
		const ImageTileCache& image_tile_cache,
		const Volume &vol,
		const Ray& ray,
		const float t_max,
		glm::vec3 *tr_weight,
		float *sampled_distance,
		RandomSampler &sampler)
	{
		*tr_weight = glm::vec3(1.0f);
		*sampled_distance = YumeRT_FLOAT_MAX;

		auto random = [&]()->float {return sampler.Random1D(); };

		const glm::vec3 sigma_t = vol.sigma_a + vol.sigma_s;
		const float channel_sum = sigma_t.x + sigma_t.y + sigma_t.z;
		const glm::vec3 channel_weight = channel_sum > 0.0f ? (sigma_t / channel_sum) : glm::vec3(1.0f / 3.0f);

		const float u0 = random();
		int channel = 0;
		if (u0 < channel_weight[0]) { channel = 0; }
		else if (u0 < channel_weight[0] + channel_weight[1]) { channel = 1; }
		else { channel = 2; }

		const glm::mat4 &i_transform = scene.i_transforms[vol.transform_idx];
		const glm::vec3 ray_origin = glm::vec3(i_transform * glm::vec4(ray.origin, 1.0f));
		const glm::vec3 ray_direction = glm::vec3(i_transform * glm::vec4(ray.direction, 0.0f));

		// begin delta tracking
		const Texture& density_texture = scene.textures[vol.density_texture_idx];
		// TODO: you should add a texture function to calculate this density_upper_bound
		const float density_upper_bound = density_texture.GetUpperBound(ray_origin, ray_direction, t_max);
		const glm::vec3 sigma_m = sigma_t * density_upper_bound;
		const glm::vec3 sigma_m_rcp = glm::vec3(SafeRcp(sigma_m.x), SafeRcp(sigma_m.y), SafeRcp(sigma_m.z));
		if (!(sigma_m[channel] > 0.0f)) { return false; }

		glm::vec3 tr(1.0f);
		glm::vec3 pdf(1.0f);
		float accumulate_distance = 0.0f;
		int iteration = 0;
		while (true) 
		{
			// we reach the limit, sample fail
			if (iteration >= MAX_FREE_PATH_LENGTH)
			{ 
				printf("Max iteration reached at SampleTrHeterogeneous!\n");
				break; 
			}

			float free_path_length = -glm::log(1.0f - random()) / sigma_m[channel];
			if (accumulate_distance + free_path_length > t_max)
			{
				// hit surface, we don't need to multiply the sigma_t
				const glm::vec3 path_tr = glm::exp(-sigma_m * (t_max - accumulate_distance));
				tr *= path_tr * sigma_m_rcp;
				pdf *= path_tr * sigma_m_rcp;
				accumulate_distance += free_path_length;
				break;
			}

			accumulate_distance += free_path_length;
			// TODO: consider transform the ray to volume space
			// fetch real density from texture
			const TextureCoordinate texture_coordinate(glm::vec2(0.0f), 
				ray.origin + ray.direction * accumulate_distance, 
				ray_origin + ray_direction * accumulate_distance);
			float density = glm::max(TextureEval(density_texture, scene.textures, image_tile_cache, texture_coordinate).x, 0.0f);
			glm::vec3 real_prob_channels;
			real_prob_channels.x = glm::min(sigma_m.x > 0.0f ? sigma_t.x * density / sigma_m.x : 0.0f, 1.0f);
			real_prob_channels.y = glm::min(sigma_m.y > 0.0f ? sigma_t.y * density / sigma_m.y : 0.0f, 1.0f);
			real_prob_channels.z = glm::min(sigma_m.z > 0.0f ? sigma_t.z * density / sigma_m.z : 0.0f, 1.0f);

			const glm::vec3 path_tr = glm::exp(-sigma_m * free_path_length);
			if (random() < real_prob_channels[channel]) 
			{
				// note: sigma_m in pdf conceal
				tr *= path_tr * vol.sigma_s * glm::min(density, density_upper_bound)  * sigma_m_rcp;
				pdf *= path_tr * real_prob_channels;
				break;
			}
			else 
			{
				// note: sigma_m in pdf and tr conceal
				tr *= path_tr * (glm::vec3(1.0f) - real_prob_channels);
				pdf *= path_tr * (glm::vec3(1.0f) - real_prob_channels);
			}
			++iteration;
		}

		*tr_weight = tr * SafeRcp(glm::dot(channel_weight, pdf));
		*sampled_distance = accumulate_distance;
		// if stop due to hit surface or go out the max iteration, sample fail
		return accumulate_distance > t_max || iteration >= MAX_FREE_PATH_LENGTH ? false : true;
	}

	// give sampled distance
	__device__ __host__ inline bool SampleVolumeScattering(const Scene& scene,
		const ImageTileCache& image_tile_cache,
		const Ray& ray,
		const float t_max,
		glm::vec3 *tr_weight,
		float *sampled_distance,
		RandomSampler &sampler)
	{
		if (scene.volumes == nullptr || scene.volume_count == 0) { return false; }
		if (ray.volume_idx < 0 || ray.volume_idx > scene.volume_count - 1) { return false; }

		const Volume &vol = scene.volumes[ray.volume_idx];
		if (vol.density_texture_idx >= 0 && vol.density_texture_idx < scene.texture_count) 
		{
			//TODO: delta tracking
			return SampleTrHeterogeneous(scene, image_tile_cache, vol, ray, t_max, tr_weight, sampled_distance, sampler);
		}
		else 
		{
			return SampleTrHomogeneous(scene, image_tile_cache, vol, ray, t_max, tr_weight, sampled_distance, sampler);
		}
	}

	__device__ __host__ inline glm::vec3 EvalTrHomogeneous(const Scene& scene,
		const ImageTileCache& image_tile_cache,
		const Volume &vol,
		const Ray& ray,
		const float t_max,
		RandomSampler &sampler)
	{
		return glm::exp(-(vol.sigma_a + vol.sigma_s) * t_max);
	}

	__device__ __host__ inline glm::vec3 EvalTrHeterogeneous(const Scene& scene,
		const ImageTileCache& image_tile_cache,
		const Volume &vol,
		const Ray& ray,
		const float t_max,
		RandomSampler &sampler)
	{
		auto random = [&]()->float {return sampler.Random1D(); };

		const glm::vec3 sigma_t = vol.sigma_a + vol.sigma_s;
		const float channel_sum = sigma_t.x + sigma_t.y + sigma_t.z;
		const glm::vec3 channel_weight = channel_sum > 0.0f ? (sigma_t / channel_sum) : glm::vec3(1.0f / 3.0f);

		const float u0 = random();
		int channel = 0;
		if (u0 < channel_weight[0]) { channel = 0; }
		else if (u0 < channel_weight[0] + channel_weight[1]) { channel = 1; }
		else { channel = 2; }

		const glm::mat4 &i_transform = scene.i_transforms[vol.transform_idx];
		const glm::vec3 ray_origin = glm::vec3(i_transform * glm::vec4(ray.origin, 1.0f));
		const glm::vec3 ray_direction = glm::vec3(i_transform * glm::vec4(ray.direction, 0.0f));

		// begin ratio tracking
		const Texture& density_texture = scene.textures[vol.density_texture_idx];
		// TODO: you should add a texture function to calculate this density_upper_bound
		const float density_upper_bound = density_texture.GetUpperBound(ray_origin, ray_direction, t_max);
		const glm::vec3 sigma_m = sigma_t * density_upper_bound;
		const glm::vec3 sigma_m_rcp = glm::vec3(SafeRcp(sigma_m.x), SafeRcp(sigma_m.y), SafeRcp(sigma_m.z));
		if (!(sigma_m[channel] > 0.0f)) { return glm::vec3(1.0f); }

		constexpr int max_trace_time = 32;
		glm::vec3 tr(1.0f);
		glm::vec3 pdf(1.0f);
		float accumulate_distance = 0.0f;
		int iteration = 0;
		while(true)
		{
			if (iteration >= MAX_FREE_PATH_LENGTH)
			{
				printf("Max iteration reached at EvalTrHeterogeneous!\n");
				break;
			}

			float free_path_length = -glm::log(1.0f - random()) / sigma_m[channel];
			if (accumulate_distance + free_path_length > t_max) 
			{
				const glm::vec3 path_tr = glm::exp(-sigma_m * (t_max - accumulate_distance));
				tr *= path_tr * sigma_m_rcp;
				pdf *= path_tr * sigma_m_rcp;
				break;
			}

			accumulate_distance += free_path_length;
			// TODO: consider transform the ray to volume space
			// fetch real density from texture
			const TextureCoordinate texture_coordinate(glm::vec2(0.0f), 
				ray.origin + ray.direction * accumulate_distance,
				ray_origin + ray_direction * accumulate_distance);
			float density = glm::max(TextureEval(density_texture, scene.textures, image_tile_cache, texture_coordinate).x, 0.0f);
			glm::vec3 real_prob_channels;
			real_prob_channels.x = glm::min(sigma_m.x > 0.0f ? sigma_t.x * density / sigma_m.x : 0.0f, 1.0f);
			real_prob_channels.y = glm::min(sigma_m.y > 0.0f ? sigma_t.y * density / sigma_m.y : 0.0f, 1.0f);
			real_prob_channels.z = glm::min(sigma_m.z > 0.0f ? sigma_t.z * density / sigma_m.z : 0.0f, 1.0f);

			const glm::vec3 path_tr = glm::exp(-sigma_m * free_path_length);
			// note: sigma_m in pdf and tr conceal
			tr *= path_tr * (glm::vec3(1.0f) - real_prob_channels);
			pdf *= path_tr;
			if (iteration + 1 > max_trace_time) 
			{
				float rr_factor = glm::max(tr.x, glm::max(tr.y, tr.z));
				if (random() < rr_factor) 
				{
					tr *= SafeRcp(rr_factor);
				}
				else 
				{
					break;
				}
			}
			++iteration;
		} 
		return tr * SafeRcp(glm::dot(channel_weight, pdf));
	}

	__device__ __host__ inline glm::vec3 EvalTr(const Scene& scene,
		const ImageTileCache& image_tile_cache,
		const Ray& ray,
		const float t_max,
		RandomSampler &sampler)
	{
		if (scene.volumes == nullptr || scene.volume_count == 0) { return glm::vec3(1.0f); }
		if (ray.volume_idx < 0 || ray.volume_idx > (int)scene.volume_count - 1) { return glm::vec3(1.0f); }

		const Volume &vol = scene.volumes[ray.volume_idx];
		if (vol.density_texture_idx >= 0 && vol.density_texture_idx < scene.texture_count) 
		{
			// TODO: ratio tracking
			return EvalTrHeterogeneous(scene, image_tile_cache, vol, ray, t_max, sampler); 
		}
		else 
		{
			return EvalTrHomogeneous(scene, image_tile_cache, vol, ray, t_max, sampler);
		}
	}

	__device__ __host__ inline glm::vec3 TraceTr(const Scene& scene,
		const ImageTileCache& image_tile_cache,
		const Ray& ray,
		RandomSampler &sampler)
	{
		if (scene.volumes == nullptr || scene.volume_count == 0) { return glm::vec3(1.0f); }

		glm::vec3 tr(1.0f);
		// distance from position to light
		Ray tr_ray = ray;
		const float max_trace_distance = ray.t; // shadow ray length
		float traced_distance = TMIN;
		while (traced_distance < max_trace_distance)
		{
			HitRecord hit_record;
			bool hit_boundary = ClosestVolume(scene, tr_ray, &hit_record);

			if (!hit_boundary) // nothing hit, reach the light
			{
				tr *= EvalTr(scene, image_tile_cache, tr_ray, glm::max(max_trace_distance - traced_distance, 0.0f), sampler);
				break;
			}
			else
			{
				tr *= EvalTr(scene, image_tile_cache, tr_ray, hit_record.hit_t, sampler);

				traced_distance += hit_record.hit_t;

				// trace forward the ray
				const PrimitiveInstance &prim = scene.prim_instances[hit_record.hit_instance_idx];
				glm::vec3 hit_position, hit_geometry_normal;
				FetchGeometryNormal(scene, hit_record, &hit_position, &hit_geometry_normal);

				bool front_side_bounce = glm::dot(hit_geometry_normal, tr_ray.direction) >= 0.0f;
				tr_ray = Ray(OffsetRayOrigin(hit_position, front_side_bounce ? hit_geometry_normal : -hit_geometry_normal),
									tr_ray.direction,
									tr_ray.ray_ior,
									front_side_bounce ? prim.outer_volume_idx : prim.inner_volume_idx);
			}
		}
		return tr;
	}

};