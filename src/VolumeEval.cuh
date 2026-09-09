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
		const int volume_count,
		const int volume_indices[],
		const Ray& ray,
		const float t_max,
		glm::vec3 *tr_weight,
		float *sampled_distance,
		float volume_weights[],
		RandomSampler &sampler)
	{
		*tr_weight = glm::vec3(1.0f);
		*sampled_distance = YumeRT_FLOAT_MAX;
		auto random = [&]()->float {return sampler.Random1D(); };

		glm::vec3 mixed_sigma_t(0.0f);
		glm::vec3 mixed_sigma_s(0.0f);
		for (int idx = 0; idx < volume_count; ++idx) {
			const Volume &vol = scene.volumes[volume_indices[idx]];
			mixed_sigma_t += vol.sigma_t;
			mixed_sigma_s += vol.sigma_t * vol.albedo;
		}

		float channel_sum = mixed_sigma_t.x + mixed_sigma_t.y + mixed_sigma_t.z;
		glm::vec3 channel_weight = channel_sum > 0.0f ? (mixed_sigma_t / channel_sum) : glm::vec3(1.0f / 3.0f);
		float u0 = random();
		int channel = 0;
		if (u0 < channel_weight[0]) { 
			channel = 0; 
			u0 = u0 * SafeRcp(channel_weight[0]);
		}
		else if (u0 < channel_weight[0] + channel_weight[1]) { 
			channel = 1; 
			u0 = (u0 - channel_weight[0]) * SafeRcp(channel_weight[1]);
		}
		else { 
			channel = 2; 
			u0 = (u0 - channel_weight[0] - channel_weight[1]) * SafeRcp(channel_weight[2]);
		}

		const float free_path_length = mixed_sigma_t[channel] > 0.0f ? (-glm::log(1.0f - u0) / mixed_sigma_t[channel]) : YumeRT_FLOAT_MAX;
		const bool volume_scatter = free_path_length < t_max;
		glm::vec3 tr = glm::exp(-mixed_sigma_t * glm::min(t_max, free_path_length));
		float pdf = volume_scatter ?
			(channel_weight.x * mixed_sigma_t.x * tr.x + channel_weight.y * mixed_sigma_t.y * tr.y + channel_weight.z * mixed_sigma_t.z * tr.z) :
			(channel_weight.x * tr.x + channel_weight.y * tr.y + channel_weight.z * tr.z);

		if (!(glm::abs(pdf) > 0.0f)) { 
			return false; 
		}

		// currently just use uniform sampling for selecting g
		const float vol_weight = 1.0f / float(volume_count);
		for (int idx = 0; idx < volume_count; ++idx) {
			volume_weights[idx] = vol_weight;
		}

		*tr_weight = volume_scatter ? (tr * mixed_sigma_s / pdf) : (tr / pdf); // don't forget to multiply the sigma_s
		*sampled_distance = free_path_length;
		return volume_scatter;
	}

	__device__ __host__ inline bool SampleTrHeterogeneous(const Scene &scene,
		const ImageTileCache &image_tile_cache,
		const int volume_count,
		const int volume_indices[],
		const Ray &ray,
		const float t_max,
		glm::vec3 *tr_weight,
		float *sampled_distance,
		float volume_weights[],
		RandomSampler &sampler)
	{
		*tr_weight = glm::vec3(1.0f);
		*sampled_distance = YumeRT_FLOAT_MAX;
		auto random = [&]()->float {
			return sampler.Random1D(); 
		};

		glm::vec3 ray_origins[MAX_BOUNDARY_RECORD];
		glm::vec3 ray_directions[MAX_BOUNDARY_RECORD];
		glm::vec3 mixed_sigma_m(0.0f);
		for (int idx = 0; idx < volume_count; ++idx)
		{
			const Volume &vol = scene.volumes[volume_indices[idx]];

			float density_upper_bound = 1.0f;
			if (vol.density_texture_idx >= 0 && vol.density_texture_idx < scene.texture_count) {

				const glm::mat4 &i_transform = scene.i_transforms[vol.transform_idx];
				ray_origins[idx] = glm::vec3(i_transform * glm::vec4(ray.origin, 1.0f));
				ray_directions[idx] = glm::vec3(i_transform * glm::vec4(ray.direction, 0.0f));

				density_upper_bound = scene.textures[vol.density_texture_idx].GetUpperBound(ray_origins[idx], ray_directions[idx], t_max);
			}
			mixed_sigma_m += vol.sigma_t * density_upper_bound;
		}

		const float channel_sum = mixed_sigma_m.x + mixed_sigma_m.y + mixed_sigma_m.z;
		const glm::vec3 channel_weight = channel_sum > 0.0f ? (mixed_sigma_m / channel_sum) : glm::vec3(1.0f / 3.0f);
		const float u0 = random();
		int channel = 0;
		if (u0 < channel_weight[0]) {
			channel = 0;
		}
		else if (u0 < channel_weight[0] + channel_weight[1]) {
			channel = 1;
		}
		else {
			channel = 2;
		}

		if (!(mixed_sigma_m[channel] > 0.0f)) {
			return false;
		}

		const glm::vec3 normalize_factor = glm::vec3(SafeRcp(mixed_sigma_m.x + 1.0f), SafeRcp(mixed_sigma_m.y + 1.0f), SafeRcp(mixed_sigma_m.z + 1.0f));
		const glm::vec3 mixed_sigma_m_rcp = glm::vec3(SafeRcp(mixed_sigma_m.x), SafeRcp(mixed_sigma_m.y), SafeRcp(mixed_sigma_m.z));

		glm::vec3 tr(1.0f);
		glm::vec3 pdf(1.0f);
		float accumulate_distance = 0.0f;
		int iteration = 0;
		while (true) 
		{
			// reach the limit, sample fail
			if (iteration >= MAX_FREE_PATH_LENGTH) {
				printf("Max iteration reached at SampleTrHeterogeneous!\n");
				break;
			}

			float free_path_length = -glm::log(1.0f - random()) / mixed_sigma_m[channel];
			if (accumulate_distance + free_path_length > t_max)
			{
				// hit surface, we don't need to multiply the sigma_t
				const glm::vec3 path_tr = glm::exp(-mixed_sigma_m * (t_max - accumulate_distance));
				tr *= path_tr * normalize_factor;
				pdf *= path_tr * normalize_factor;
				accumulate_distance += free_path_length;
				break;
			}

			accumulate_distance += free_path_length;
			// TODO: consider transform the ray to volume space
			// fetch real density from texture
			glm::vec3 mixed_sigma_t(0.0f);
			glm::vec3 mixed_sigma_s(0.0f);
			for (int idx = 0; idx < volume_count; ++idx)
			{
				const Volume &vol = scene.volumes[volume_indices[idx]];
				float density = 1.0f;
				if (vol.density_texture_idx >= 0 && vol.density_texture_idx < scene.texture_count)
				{
					const Texture &density_texture = scene.textures[vol.density_texture_idx];
					const TextureCoordinate texture_coordinate(glm::vec2(0.0f),
						ray.origin + ray.direction * accumulate_distance,
						ray_origins[idx] + ray_directions[idx] * accumulate_distance);
					density = glm::max(TextureEval(density_texture, scene.textures, image_tile_cache, texture_coordinate).x, 0.0f);
				}
				mixed_sigma_t += vol.sigma_t * density;
				mixed_sigma_s += vol.sigma_t * vol.albedo * density;
			}

			glm::vec3 real_prob_channels;
			real_prob_channels.x = glm::min(mixed_sigma_m.x > 0.0f ? mixed_sigma_t.x * mixed_sigma_m_rcp.x : 0.0f, 1.0f);
			real_prob_channels.y = glm::min(mixed_sigma_m.y > 0.0f ? mixed_sigma_t.y * mixed_sigma_m_rcp.y : 0.0f, 1.0f);
			real_prob_channels.z = glm::min(mixed_sigma_m.z > 0.0f ? mixed_sigma_t.z * mixed_sigma_m_rcp.z : 0.0f, 1.0f);
			glm::vec3 fake_prob_channels = glm::vec3(1.0f) - real_prob_channels;

			const glm::vec3 path_tr = glm::exp(-mixed_sigma_m * free_path_length);
			if (random() < real_prob_channels[channel]) 
			{
				// note: sigma_m in pdf conceal
				tr *= path_tr * mixed_sigma_s * normalize_factor;
				pdf *= path_tr * mixed_sigma_m * real_prob_channels * normalize_factor;
				break;
			}
			else 
			{
				// note: sigma_m in pdf and tr conceal
				// note: tr = path_tr x sigma_null = path_tr x sigma_major * fake_prob
				// note: pdf = path_tr x sigma_major * fake_prob
				tr *= path_tr * mixed_sigma_m * fake_prob_channels * normalize_factor; 
				pdf *= path_tr * mixed_sigma_m * fake_prob_channels * normalize_factor;
			}
			++iteration;
		}

		const float vol_weight = 1.0f / float(volume_count);
		for (int idx = 0; idx < volume_count; ++idx) {
			volume_weights[idx] = vol_weight;
		}

		*tr_weight = tr * SafeRcp(glm::dot(channel_weight, pdf));
		*sampled_distance = accumulate_distance;
		return accumulate_distance > t_max || iteration >= MAX_FREE_PATH_LENGTH ? false : true; // if stop due to hit surface or go out the max iteration, sample fail
	}

	// give sampled distance
	__device__ __host__ inline bool SampleVolumeScattering(const Scene& scene,
		const ImageTileCache& image_tile_cache,
		const Ray& ray,
		const int overlapped_volume_count,
		const int volume_indices[],
		const float t_max,
		glm::vec3 *tr_weight,
		float *sampled_distance,
		float volume_weights[],
		RandomSampler &sampler)
	{
		if (scene.volumes == nullptr || scene.volume_count == 0 || overlapped_volume_count == 0) {
			return false; 
		}
		
		bool heterogeneous = false;
		for (int idx = 0; idx < overlapped_volume_count; ++idx) {
			const Volume &vol = scene.volumes[volume_indices[idx]];
			if (vol.density_texture_idx >= 0 && vol.density_texture_idx < scene.texture_count) {
				heterogeneous = true;
				break;
			}
		}

		if (heterogeneous) 
		{
			return SampleTrHeterogeneous(scene, image_tile_cache, overlapped_volume_count, volume_indices, ray, t_max, 
				tr_weight, sampled_distance, volume_weights, sampler);
		}
		else 
		{
			return SampleTrHomogeneous(scene, image_tile_cache, overlapped_volume_count, volume_indices, ray, t_max, 
				tr_weight, sampled_distance, volume_weights, sampler);
		}
	}

	__device__ __host__ inline glm::vec3 EvalTrHomogeneous(const Scene& scene,
		const ImageTileCache& image_tile_cache,
		const int volume_count,
		const int volume_indices[],
		const Ray& ray,
		const float t_max,
		RandomSampler &sampler)
	{
		glm::vec3 mixed_sigma_t(0.0f);
		for (int idx = 0; idx < volume_count; ++idx) {
			const Volume &vol = scene.volumes[volume_indices[idx]];
			mixed_sigma_t += vol.sigma_t;
		}
		return glm::exp(-mixed_sigma_t * t_max);
	}

	__device__ __host__ inline glm::vec3 EvalTrHeterogeneous(const Scene& scene,
		const ImageTileCache& image_tile_cache,
		const int volume_count,
		const int volume_indices[],
		const Ray& ray,
		const float t_max,
		RandomSampler &sampler)
	{
		auto random = [&]()->float {
			return sampler.Random1D(); 
		};

		glm::vec3 ray_origins[MAX_BOUNDARY_RECORD];
		glm::vec3 ray_directions[MAX_BOUNDARY_RECORD];
		glm::vec3 mixed_sigma_m(0.0f);
		for (int idx = 0; idx < volume_count; ++idx)
		{
			const Volume &vol = scene.volumes[volume_indices[idx]];
			
			float density_upper_bound = 1.0f;
			if (vol.density_texture_idx >= 0 && vol.density_texture_idx < scene.texture_count) {

				const glm::mat4 &i_transform = scene.i_transforms[vol.transform_idx];
				ray_origins[idx] = glm::vec3(i_transform * glm::vec4(ray.origin, 1.0f));
				ray_directions[idx] = glm::vec3(i_transform * glm::vec4(ray.direction, 0.0f));

				density_upper_bound = scene.textures[vol.density_texture_idx].GetUpperBound(ray_origins[idx], ray_directions[idx], t_max);
			}
			mixed_sigma_m += vol.sigma_t * density_upper_bound;
		}

		const float channel_sum = mixed_sigma_m.x + mixed_sigma_m.y + mixed_sigma_m.z;
		const glm::vec3 channel_weight = channel_sum > 0.0f ? (mixed_sigma_m / channel_sum) : glm::vec3(1.0f / 3.0f);
		const float u0 = random();
		int channel = 0;
		if (u0 < channel_weight[0]) {
			channel = 0;
		}
		else if (u0 < channel_weight[0] + channel_weight[1]) {
			channel = 1;
		}
		else {
			channel = 2;
		}

		if (!(mixed_sigma_m[channel] > 0.0f)) {
			return glm::vec3(1.0f);
		}

		const glm::vec3 normalize_factor = glm::vec3(SafeRcp(mixed_sigma_m.x + 1.0f), SafeRcp(mixed_sigma_m.y + 1.0f), SafeRcp(mixed_sigma_m.z + 1.0f));
		const glm::vec3 mixed_sigma_m_rcp = glm::vec3(SafeRcp(mixed_sigma_m.x), SafeRcp(mixed_sigma_m.y), SafeRcp(mixed_sigma_m.z));

		constexpr int max_trace_time = 32;
		glm::vec3 tr(1.0f);
		glm::vec3 pdf(1.0f);
		float accumulate_distance = 0.0f;
		int iteration = 0;
		while(true)
		{
			if (iteration >= MAX_FREE_PATH_LENGTH) {
				printf("Max iteration reached at EvalTrHeterogeneous!\n");
				break;
			}

			float free_path_length = -glm::log(1.0f - random()) / mixed_sigma_m[channel];
			if (accumulate_distance + free_path_length > t_max) 
			{
				const glm::vec3 path_tr = glm::exp(-mixed_sigma_m * (t_max - accumulate_distance));
				tr *= path_tr * normalize_factor;
				pdf *= path_tr * normalize_factor;
				break;
			}

			accumulate_distance += free_path_length;

			// TODO: consider transform the ray to volume space
			// fetch real density from texture
			glm::vec3 mixed_sigma_t(0.0f);
			for (int idx = 0; idx < volume_count; ++idx)
			{
				const Volume &vol = scene.volumes[volume_indices[idx]];
				float density = 1.0f;
				if (vol.density_texture_idx >= 0 && vol.density_texture_idx < scene.texture_count)
				{
					const Texture &density_texture = scene.textures[vol.density_texture_idx];
					const TextureCoordinate texture_coordinate(glm::vec2(0.0f), 
						ray.origin + ray.direction * accumulate_distance,
						ray_origins[idx] + ray_directions[idx] * accumulate_distance);
					density = glm::max(TextureEval(density_texture, scene.textures, image_tile_cache, texture_coordinate).x, 0.0f);
				}
				mixed_sigma_t += vol.sigma_t * density;
			}
			glm::vec3 real_prob_channels;
			real_prob_channels.x = glm::min(mixed_sigma_m.x > 0.0f ? mixed_sigma_t.x * mixed_sigma_m_rcp.x : 0.0f, 1.0f);
			real_prob_channels.y = glm::min(mixed_sigma_m.y > 0.0f ? mixed_sigma_t.y * mixed_sigma_m_rcp.y : 0.0f, 1.0f);
			real_prob_channels.z = glm::min(mixed_sigma_m.z > 0.0f ? mixed_sigma_t.z * mixed_sigma_m_rcp.z : 0.0f, 1.0f);
			glm::vec3 fake_prob_channels = glm::vec3(1.0f) - real_prob_channels;

			const glm::vec3 path_tr = glm::exp(-mixed_sigma_m * free_path_length);
			// note: sigma_m in pdf and tr conceal
			tr *= path_tr * mixed_sigma_m * fake_prob_channels * normalize_factor;
			pdf *= path_tr * mixed_sigma_m * normalize_factor;
			if (iteration + 1 > max_trace_time) 
			{
				float rr_factor = glm::max(tr.x, glm::max(tr.y, tr.z));
				rr_factor = glm::min(rr_factor, 1.0f);
				if (random() < rr_factor) {
					tr /= glm::max(rr_factor, MIN_COLOR_EPSILON);
				}
				else {
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
		const RayTransfer &shadow_ray_transfer,
		const float t_max,
		RandomSampler &sampler)
	{
		if (scene.volumes == nullptr || scene.volume_count == 0) { return glm::vec3(1.0f); }
		
		int volume_indices[MAX_BOUNDARY_RECORD];
		int overlapped_volume_count = shadow_ray_transfer.GetCurrentVolumeIndices(volume_indices, scene.volume_count);
		if (overlapped_volume_count == 0) { return glm::vec3(1.0f); }

		bool heterogeneous = false;
		for (int idx = 0; idx < overlapped_volume_count; ++idx) {
			const Volume &vol = scene.volumes[volume_indices[idx]];
			if (vol.density_texture_idx >= 0 && vol.density_texture_idx < scene.texture_count) {
				heterogeneous = true;
				break;
			}
		}

		if (heterogeneous) 
		{
			return EvalTrHeterogeneous(scene, image_tile_cache, overlapped_volume_count, volume_indices, ray, t_max, sampler);
		}
		else 
		{
			return EvalTrHomogeneous(scene, image_tile_cache, overlapped_volume_count, volume_indices, ray, t_max, sampler);
		}
	}

	__device__ __host__ inline glm::vec3 TraceTr(const Scene& scene,
		const ImageTileCache& image_tile_cache,
		const Ray& ray,
		RayTransfer &shadow_ray_transfer,
		RandomSampler &sampler)
	{
		if (scene.volumes == nullptr || scene.volume_count == 0) { 
			return glm::vec3(1.0f); 
		}

		glm::vec3 tr(1.0f);
		
		Ray tr_ray = ray; // distance from position to light
		const float max_trace_distance = ray.t; // shadow ray length
		float traced_distance = TMIN;
		while (traced_distance < max_trace_distance)
		{
			HitRecord hit_record;
			int nearby_hit_count = 0;
			NearbyHit nearby_hits[MAX_BOUNDARY_RECORD + 2];
			bool hit_boundary = TraceVolume(scene, tr_ray, &hit_record, &nearby_hit_count, nearby_hits);

			if (!hit_boundary) // nothing hit, reach the light
			{
				tr *= EvalTr(scene, image_tile_cache, tr_ray, shadow_ray_transfer, glm::max(max_trace_distance - traced_distance, 0.0f), sampler);
				break;
			}
			else
			{
				tr *= EvalTr(scene, image_tile_cache, tr_ray, shadow_ray_transfer, hit_record.hit_t, sampler);

				traced_distance += hit_record.hit_t;

				// trace forward the ray
				const PrimitiveInstance &prim = scene.primitive_instances[hit_record.hit_instance_idx];
				const Material &mtl = scene.materials[prim.material_idx];
				
				glm::vec3 hit_position(0.0f), hit_position_error(0.0f), hit_geometry_normal(0.0f);
				FetchGeometryNormal(scene, hit_record, &hit_position, &hit_position_error, &hit_geometry_normal);

				uint32_t mtl_ior_priority;
				float mtl_ior = mtl.FetchIOR(&mtl_ior_priority);

				BoundaryTransitionBunch(shadow_ray_transfer, scene.primitive_instances, scene.materials, nearby_hits, nearby_hit_count);
				tr_ray = Ray(OffsetRayOrigin(hit_position, hit_position_error, tr_ray.direction, hit_geometry_normal), tr_ray.direction, hit_record.hit_instance_idx, hit_record.hit_triangle_idx);
			}
		}
		return tr;
	}

	__device__ __host__ inline bool SampleMixedPhases(const float volume_weights[], const float gs[], const int volume_count, 
		 float u0,  float u1, const glm::vec3 &wo, float *weight, glm::vec3 *wi, float *pdf)
	{
		if (volume_count == 0) {
			return false;
		}

		float cdf = 0.0f;
		int selected_vol_idx = -1;
		for (int idx = 0; idx < volume_count; ++idx) {
			if (u0 < cdf + volume_weights[idx]) {
				selected_vol_idx = idx;
				u0 = (u0 - cdf) * SafeRcp(volume_weights[idx]);
				break;
			}
			cdf += volume_weights[idx];
		}
		if (selected_vol_idx == -1) { 
			selected_vol_idx = volume_count - 1;
		}
		if (selected_vol_idx == -1) { 
			return false; 
		}

		float phase_wi_pdf = 0.0f;
		const bool sample_valid = SamplePhase(gs[selected_vol_idx], u0, u1, wo, weight, wi, &phase_wi_pdf);
		if (!sample_valid) { 
			return false; 
		}

		float mixed_pdf = phase_wi_pdf * volume_weights[selected_vol_idx];
		float mixed_g = (*weight) * phase_wi_pdf * volume_weights[selected_vol_idx];
		for (int idx = 0; idx < volume_count; ++idx)
		{
			if (idx == selected_vol_idx) {
				continue; 
			}
			
			float wi_pdf;
			mixed_g += volume_weights[idx] * EvalPhase(gs[idx], wo, *wi, &wi_pdf);
			mixed_pdf += volume_weights[idx] * wi_pdf;
		}

		*pdf = mixed_pdf;
		*weight = mixed_g * SafeRcp(mixed_pdf);
		return true;
	}

	__device__ __host__ inline float EvalMixedPhases(const float volume_weights[], const float gs[], const int volume_count, 
		const glm::vec3 &wo, const glm::vec3 &wi, float *pdf)
	{
		if (volume_count == 0) { 
			*pdf = 0.0f;
			return 0.0f;
		}

		float mixed_g = 0.0f;
		float mixed_pdf = 0.0f;
		for (int idx = 0; idx < volume_count; ++idx)
		{
			float single_pdf = 0.0f;
			float single_g = EvalPhase(gs[idx], wo, wi, &single_pdf);
			mixed_g += volume_weights[idx] * single_g;
			mixed_pdf += volume_weights[idx] * single_pdf;
		}

		*pdf = mixed_pdf;
		return mixed_g;
	}


};