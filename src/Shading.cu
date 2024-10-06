#include <chrono>

#include <cuda_runtime.h>
#include <cuda_profiler_api.h>

#include <vector_types.h>
#include <driver_types.h>

#include <device_launch_parameters.h>
#include <cooperative_groups.h>

#include "Helper.h"
#include "MathCommon.h"
#include "Ray.h"
#include "Camera.h"
#include "SceneDefines.h"

#include "GeometryDefines.h"
#include "GeometryUtilities.h"
#include "PrimtiveInstance.h"
#include "Material.h"
#include "Light.h"
#include "Volume.h"
#include "Texture.h"

#include "ShadingUtilities.cuh"
#include "RandomSampler.cuh"
#include "SampleUtilities.h"
#include "BSDF.cuh"

#include "VolumeEval.cuh"

namespace YumeRT
{
	__device__ __host__ inline glm::vec3 Background(const glm::vec3 &direction)
	{
		return glm::mix(glm::vec3(1.0f), glm::vec3(0.05f, 0.2f, 0.85f), direction.y * 0.5f + 0.5f);
	}

	__device__ __host__ inline float PowerHeuristic(float main_pdf, float vice_pdf)
	{
		return SafeRcp(1.0f + Sqr(vice_pdf * SafeRcp(main_pdf)));
	}

	__device__ inline int AtomicAddInt(int *address, int val) {
		return atomicAdd(address, val);
	}

	__device__ inline int AtomicExchInt(int *address, int val) {
		return atomicExch(address, val);
	}

	__device__ inline float AtomicAddFloat(float *address, float val) {
		return atomicAdd(address, val);
	}

	__device__ inline float AtomicExchFloat(float *address, float val) {
		return atomicExch(address, val);
	}

	__device__ glm::vec3 EvalDistantLight(const RenderSetting &render_setting,
															const Scene &scene, 
															const ImageTileCache& image_tile_cache,
															MaterialBSDF &material_bsdf,
															const glm::vec3 &ray_direction, 
															const HitRecord &hit_record,
															const RayTransfer &ray_transfer,
															const int nearby_hit_count,
															const NearbyHit nearby_hits[],
															const glm::vec3 &hit_position, 
															const glm::vec3 &hit_position_error, 
															const glm::vec3 &hit_shading_normal, 
															const glm::vec3 &hit_geometry_normal,
															RandomSampler &sampler)
	{
		assert(scene.distant_lights != nullptr);
		if (scene.distant_lights == nullptr || scene.distant_light_count == 0) {
			return glm::vec3(0.0f);
		}
		auto random = [&]()->float {
			return sampler.Random1D(); 
		};
		
		glm::vec3 direct_lighting(0.0f);
		const glm::vec3 wo = material_bsdf.WorldToShading(-ray_direction);

		float distant_light_select_pdf = 1.0f / float(scene.distant_light_count);
		int selected_distant_light_idx = glm::max(glm::min((int)(random() * scene.distant_light_count), (int)scene.distant_light_count - 1), 0);

		// sample from light
		{
			glm::vec3 light_dir(0.0f), light_pos(0.0f);
			float light_sample_pdf = 0.0f;
			glm::vec3 Li(0.0f);
			const bool light_sample_valid	= scene.distant_lights[selected_distant_light_idx].SampleLi(scene, hit_position, random(), random(), &light_dir, &light_pos, &Li, &light_sample_pdf);

			glm::vec3 light_wi = material_bsdf.WorldToShading(light_dir);
			float light_wi_pdf = 0.0f;
			const glm::vec3 bsdf_weight = material_bsdf.EvalWi(wo, light_wi, &light_wi_pdf);

			if (MaxComponent(bsdf_weight) > MIN_COLOR_EPSILON && light_sample_valid)
			{
				const glm::vec3 shadow_ray_origin = OffsetRayOrigin(hit_position, hit_position_error, light_dir, hit_geometry_normal);
				Ray shadow_ray(shadow_ray_origin, light_dir);
				const bool transmit_boundary = glm::dot(-ray_direction, hit_geometry_normal) * glm::dot(light_dir, hit_geometry_normal) < 0.0f;
				const bool sample_invalid = (!SameHemisphere(wo, light_wi) && !transmit_boundary) || 
					(SameHemisphere(wo, light_wi) && transmit_boundary);

				HitRecord shadow_ray_record;
				if (!sample_invalid && !BVHTraverseShadow(scene, shadow_ray, &shadow_ray_record))
				{
					glm::vec3 tr(1.0f);
					if (render_setting.enable_volume_scattering)
					{
						RayTransfer shadow_ray_transfer(ray_transfer);
						
						if (transmit_boundary) { // refract
							BoundaryTransitionBunch(shadow_ray_transfer, scene.primitive_instances, scene.materials, nearby_hits, nearby_hit_count);
						}

						shadow_ray = Ray(shadow_ray_origin, light_dir, hit_record.hit_instance_idx, hit_record.hit_triangle_idx);
						shadow_ray.t = TMAX;
						tr = TraceTr(scene, image_tile_cache, shadow_ray, shadow_ray_transfer, sampler);
					}

					float mis_weight = PowerHeuristic(light_sample_pdf, light_wi_pdf);
					direct_lighting += mis_weight * tr * glm::min(bsdf_weight, glm::vec3(MAX_COLOR_CLAMP)) * glm::min(Li, glm::vec3(MAX_COLOR_CLAMP));
				}
			}
		}

		// sample from bsdf
		{
			glm::vec3 bsdf_weight, bsdf_wi;
			float bsdf_sample_pdf = 0.0f;
			bool sample_valid = material_bsdf.SampleWi(wo, &bsdf_weight, &bsdf_wi, &bsdf_sample_pdf, random(), random(), random());

			if (sample_valid)
			{
				const glm::vec3 light_dir = material_bsdf.ShadingToWorld(bsdf_wi);
				float bsdf_wi_pdf = 0.0f;
				const glm::vec3 Li = scene.distant_lights[selected_distant_light_idx].EvalLi(scene, light_dir, &bsdf_wi_pdf);

				if (MaxComponent(Li) > MIN_COLOR_EPSILON)
				{
					const glm::vec3 shadow_ray_origin = OffsetRayOrigin(hit_position, hit_position_error, light_dir, hit_geometry_normal);
					Ray shadow_ray(shadow_ray_origin, light_dir);
					const bool transmit_boundary = glm::dot(-ray_direction, hit_geometry_normal) * glm::dot(light_dir, hit_geometry_normal) < 0.0f;
					const bool sample_invalid = (!SameHemisphere(wo, bsdf_wi) && !transmit_boundary) ||
						(SameHemisphere(wo, bsdf_wi) && transmit_boundary);
	
					HitRecord shadow_ray_record;
					if (!sample_invalid && !BVHTraverseShadow(scene, shadow_ray, &shadow_ray_record))
					{
						glm::vec3 tr(1.0f);
						if (render_setting.enable_volume_scattering)
						{
							RayTransfer shadow_ray_transfer(ray_transfer);
							if (transmit_boundary) { // refract
								BoundaryTransitionBunch(shadow_ray_transfer, scene.primitive_instances, scene.materials, nearby_hits, nearby_hit_count);
							}

							shadow_ray = Ray(shadow_ray_origin, light_dir, hit_record.hit_instance_idx, hit_record.hit_triangle_idx);
							shadow_ray.t = TMAX;
							tr = TraceTr(scene, image_tile_cache, shadow_ray, shadow_ray_transfer, sampler);
						}

						float mis_weight = PowerHeuristic(bsdf_sample_pdf, bsdf_wi_pdf);
						direct_lighting += mis_weight * tr * glm::min(bsdf_weight, glm::vec3(MAX_COLOR_CLAMP)) * glm::min(Li, glm::vec3(MAX_COLOR_CLAMP));
					}
				}
			}
		}
		assert(!glm::isnan(direct_lighting.x) && !glm::isnan(direct_lighting.y) && !glm::isnan(direct_lighting.z));
		return  direct_lighting / distant_light_select_pdf;
	}



	__device__ glm::vec3 EvalVolumeDistantLight(const RenderSetting &render_setting,
																		const Scene &scene,
																		const ImageTileCache& image_tile_cache,
																		const int volume_count,
																		const float gs[],
																		const float volume_weighs[],
																		const Ray &ray,
																		const HitRecord &hit_record,
																		const RayTransfer &ray_transfer,
																		const glm::vec3 &volume_hit_position,
																		RandomSampler &sampler)
	{
		if (scene.distant_lights == nullptr || scene.distant_light_count == 0) { 
			return glm::vec3(0.0f); 
		}
		if (volume_count == 0) { 
			return glm::vec3(0.0f); 
		}
		auto random = [&]()->float {return sampler.Random1D(); };

		glm::vec3 direct_lighting(0.0f);
		const float distant_light_select_pdf = 1.0f / float(scene.distant_light_count);
		const int selected_distant_light_idx = glm::max(glm::min((int)(random() * scene.distant_light_count), (int)scene.distant_light_count - 1), 0);

		// sample from light
		{
			glm::vec3 light_dir(0.0f), light_pos(0.0f);
			float light_sample_pdf = 0.0f;
			glm::vec3 Li(0.0f);
			const bool light_sample_valid = scene.distant_lights[selected_distant_light_idx].SampleLi(scene, volume_hit_position, random(), random(), &light_dir, &light_pos, &Li, &light_sample_pdf);

			glm::vec3 light_wi = light_dir;
			float light_wi_pdf = 0.0f;
			const float phase_weight = EvalMixedPhases(volume_weighs, gs, volume_count, -ray.direction, light_wi, &light_wi_pdf);

			if (phase_weight > MIN_COLOR_EPSILON && light_sample_valid)
			{
				Ray shadow_ray(volume_hit_position, light_dir);
				shadow_ray.t = TMAX;

				HitRecord shadow_ray_record;
				if (!BVHTraverseShadow(scene, shadow_ray, &shadow_ray_record))
				{
					RayTransfer shadow_ray_transfer(ray_transfer);

					shadow_ray = Ray(volume_hit_position, light_dir, EMPTY_UINT32, EMPTY_UINT32);
					shadow_ray.t = TMAX;
					glm::vec3 tr = TraceTr(scene, image_tile_cache, shadow_ray, shadow_ray_transfer, sampler);

					float mis_weight = PowerHeuristic(light_sample_pdf, light_wi_pdf);
					direct_lighting += mis_weight * tr * glm::min(phase_weight, MAX_COLOR_CLAMP) * glm::min(Li, glm::vec3(MAX_COLOR_CLAMP));
				}
			}
		}

		// sample from volume
		{
			glm::vec3 phase_wi;
			float phase_sample_pdf = 0.0f, phase_weight = 0.0f;
			bool sample_valid = SampleMixedPhases(volume_weighs, gs, volume_count, random(), random(), -ray.direction, &phase_weight, &phase_wi, &phase_sample_pdf);

			if (sample_valid)
			{
				const glm::vec3 light_dir = phase_wi;
				float phase_wi_pdf = 0.0f;
				const glm::vec3 Li = scene.distant_lights[selected_distant_light_idx].EvalLi(scene, light_dir, &phase_wi_pdf);

				if (MaxComponent(Li) > MIN_COLOR_EPSILON)
				{
					Ray shadow_ray(volume_hit_position, light_dir);
					shadow_ray.t = TMAX;

					HitRecord shadow_ray_record;
					if (!BVHTraverseShadow(scene, shadow_ray, &shadow_ray_record))
					{
						RayTransfer shadow_ray_transfer(ray_transfer);
						
						shadow_ray = Ray(volume_hit_position, light_dir, EMPTY_UINT32, EMPTY_UINT32);
						shadow_ray.t = TMAX;
						glm::vec3 tr = TraceTr(scene, image_tile_cache, shadow_ray, shadow_ray_transfer, sampler);

						float mis_weight = PowerHeuristic(phase_sample_pdf, phase_wi_pdf);
						direct_lighting += mis_weight * tr * glm::min(phase_weight, MAX_COLOR_CLAMP) * glm::min(Li, glm::vec3(MAX_COLOR_CLAMP));
					}
				}
			}
		}

		return direct_lighting / distant_light_select_pdf;
	}


	__device__ glm::vec3 EvalShapeLight(const RenderSetting &render_setting,
															const Scene &scene,
															const ImageTileCache& image_tile_cache,
															MaterialBSDF &material_bsdf,
															const glm::vec3 &ray_direction,
															const HitRecord &hit_record,
															const RayTransfer &ray_transfer,
															const int nearby_hit_count,
															const NearbyHit nearby_hits[],
															const glm::vec3 &hit_position,
															const glm::vec3 &hit_position_error,
															const glm::vec3 &hit_shading_normal,
															const glm::vec3 &hit_geometry_normal,
															RandomSampler &sampler)
	{
		if (scene.shape_light_count == 0 || scene.shape_lights == nullptr) { return glm::vec3(0.0f); }
		auto random = [&]()->float {return sampler.Random1D(); };

		glm::vec3 direct_lighting(0.0f);
		const glm::vec3 wo = material_bsdf.WorldToShading(-ray_direction);

		float light_select_pdf = 0.0f;
		int light_idx = SampleLightPower(scene, random(), &light_select_pdf);

		const ShapeLight &shape_light = scene.shape_lights[light_idx];

		// sample from light
		{
			glm::vec3 light_dir(0.0f), light_pos(0.0f), light_pos_error(0.0f), light_geo_normal(0.0f);
			float light_sample_pdf = 0.0f;
			glm::vec3 Li(0.0f);
			const bool light_sample_valid = shape_light.SampleLi(scene, hit_position, random(), random(), &light_dir, &light_pos, &light_pos_error, &light_geo_normal, &Li, &light_sample_pdf);

			glm::vec3 light_wi = material_bsdf.WorldToShading(light_dir);
			float light_wi_pdf = 0.0f;
			const glm::vec3 bsdf_weight = material_bsdf.EvalWi(wo, light_wi, &light_wi_pdf);

			if (MaxComponent(bsdf_weight) > MIN_COLOR_EPSILON && light_sample_valid)
			{
				const glm::vec3 shadow_ray_origin = OffsetRayOrigin(hit_position, hit_position_error, light_dir, hit_geometry_normal);

				// only for test occlusion, so we don't need ior and volume
				Ray shadow_ray(shadow_ray_origin, light_dir);
				float max_trace_distance = glm::length(OffsetRayOrigin(light_pos, light_pos_error, -light_dir, light_geo_normal) - hit_position) * SHADOW_RAY_CLAMP;
				
				shadow_ray.t = max_trace_distance;
				HitRecord shadow_ray_record;
				const bool transmit_boundary = glm::dot(-ray_direction, hit_geometry_normal) * glm::dot(light_dir, hit_geometry_normal) < 0.0f;
				const bool sample_invalid = (!SameHemisphere(wo, light_wi) && !transmit_boundary) || 
					(SameHemisphere(wo, light_wi) && transmit_boundary);

				if (!sample_invalid && !BVHTraverseShadow(scene, shadow_ray, &shadow_ray_record))
				{
					glm::vec3 tr(1.0f);
					if (render_setting.enable_volume_scattering) 
					{
						RayTransfer shadow_ray_transfer(ray_transfer);
						if (transmit_boundary) {
							BoundaryTransitionBunch(shadow_ray_transfer, scene.primitive_instances, scene.materials, nearby_hits, nearby_hit_count);
						}

						shadow_ray = Ray(shadow_ray_origin, light_dir, hit_record.hit_instance_idx, hit_record.hit_triangle_idx);
						shadow_ray.t = max_trace_distance;
						tr = TraceTr(scene, image_tile_cache, shadow_ray, shadow_ray_transfer, sampler);
					}
					
					float mis_weight = PowerHeuristic(light_sample_pdf, light_wi_pdf);
					direct_lighting += mis_weight * tr * glm::min(bsdf_weight, glm::vec3(MAX_COLOR_CLAMP)) * glm::min(Li, glm::vec3(MAX_COLOR_CLAMP));
				}
			}
		}

		// sample from bsdf
		{
			glm::vec3 bsdf_weight, bsdf_wi;
			float bsdf_sample_pdf = 0.0f;
			bool sample_valid = material_bsdf.SampleWi(wo, &bsdf_weight, &bsdf_wi, &bsdf_sample_pdf, random(), random(), random());

			if (sample_valid)
			{
				glm::vec3 light_dir = material_bsdf.ShadingToWorld(bsdf_wi);
				glm::vec3 light_pos(0.0f), light_pos_error(0.0f), light_geo_normal(0.0f);
				float bsdf_wi_pdf = 0.0f;
				const glm::vec3 Li = shape_light.EvalLi(scene, hit_position, light_dir, &light_pos, &light_pos_error, &light_geo_normal, &bsdf_wi_pdf);

				if (MaxComponent(Li) > MIN_COLOR_EPSILON)
				{
					glm::vec3 shadow_ray_origin = OffsetRayOrigin(hit_position,  hit_position_error, light_dir, hit_geometry_normal);
					
					// only for test occlusion, so we don't need ior and volume
					Ray shadow_ray(shadow_ray_origin, light_dir);
					float max_trace_distance = glm::length(OffsetRayOrigin(light_pos, light_pos_error, -light_dir, light_geo_normal) - hit_position) * SHADOW_RAY_CLAMP;

					shadow_ray.t = max_trace_distance;
					HitRecord shadow_ray_record;
					const bool transmit_boundary = glm::dot(-ray_direction, hit_geometry_normal) * glm::dot(light_dir, hit_geometry_normal) < 0.0f;
					const bool sample_invalid = (!SameHemisphere(wo, bsdf_wi) && !transmit_boundary) ||
						(SameHemisphere(wo, bsdf_wi) && transmit_boundary);
					
					if (!sample_invalid && !BVHTraverseShadow(scene, shadow_ray, &shadow_ray_record))
					{
						glm::vec3 tr(1.0f);
						if (render_setting.enable_volume_scattering)
						{
							RayTransfer shadow_ray_transfer(ray_transfer);
							if (transmit_boundary) {
								BoundaryTransitionBunch(shadow_ray_transfer, scene.primitive_instances, scene.materials, nearby_hits, nearby_hit_count);
							}
							
							shadow_ray = Ray(shadow_ray_origin, light_dir, hit_record.hit_instance_idx, hit_record.hit_triangle_idx);
							shadow_ray.t = max_trace_distance;
							tr = TraceTr(scene, image_tile_cache, shadow_ray, shadow_ray_transfer, sampler);
						}
						
						float mis_weight = PowerHeuristic(bsdf_sample_pdf, bsdf_wi_pdf);
						direct_lighting += mis_weight * tr * glm::min(bsdf_weight, glm::vec3(MAX_COLOR_CLAMP)) * glm::min(Li, glm::vec3(MAX_COLOR_CLAMP));
					}
				}
			}
		}
		
		// todo: eval shape light contrib 
		return   direct_lighting * SafeRcp(light_select_pdf);
	}

	__device__ glm::vec3 EvalVolumeShapeLight(const RenderSetting &render_setting,
																		const Scene &scene,
																		const ImageTileCache& image_tile_cache,
																		const int volume_count,
																		const float gs[],
																		const float volume_weighs[],
																		const Ray &ray,
																		const HitRecord &hit_record,
																		const RayTransfer &ray_transfer,
																		const glm::vec3 &volume_hit_position, 
																		RandomSampler &sampler)
	{
		if (scene.shape_light_count == 0 || scene.shape_lights == nullptr) {
			return glm::vec3(0.0f);
		}
		if (volume_count == 0) {
			return glm::vec3(0.0f);
		}
		auto random = [&]()->float {return sampler.Random1D(); };

		glm::vec3 direct_lighting(0.0f);

		float light_select_pdf = 0.0f;
		int light_idx = SampleLightPower(scene, random(), &light_select_pdf);

		const ShapeLight &shape_light = scene.shape_lights[light_idx];

		// sample from light
		{
			glm::vec3 light_dir(0.0f), light_pos(0.0f), light_pos_error(0.0f), light_geo_normal(0.0f);
			float light_sample_pdf = 0.0f;
			glm::vec3 Li(0.0f);
			const bool light_sample_valid = shape_light.SampleLi(scene, volume_hit_position, random(), random(), &light_dir, &light_pos, &light_pos_error, &light_geo_normal, &Li, &light_sample_pdf);

			float light_wi_pdf = 0.0f;
			float phase_weight = EvalMixedPhases(volume_weighs, gs, volume_count, -ray.direction ,light_dir, &light_wi_pdf);

			if (phase_weight > MIN_COLOR_EPSILON && light_sample_valid)
			{
				Ray shadow_ray(volume_hit_position, light_dir);
				float max_trace_distance = glm::length(OffsetRayOrigin(light_pos, light_pos_error, -light_dir, light_geo_normal) - volume_hit_position) * SHADOW_RAY_CLAMP;

				shadow_ray.t = max_trace_distance;
				HitRecord shadow_ray_record;
				if (!BVHTraverseShadow(scene, shadow_ray, &shadow_ray_record))
				{
					RayTransfer shadow_ray_transfer(ray_transfer);

					shadow_ray = Ray(volume_hit_position, light_dir, EMPTY_UINT32, EMPTY_UINT32);
					shadow_ray.t = max_trace_distance;
					glm::vec3 tr = TraceTr(scene, image_tile_cache, shadow_ray, shadow_ray_transfer, sampler);
					
					float mis_weight = PowerHeuristic(light_sample_pdf, light_wi_pdf);
					direct_lighting += mis_weight * tr * glm::min(phase_weight, MAX_COLOR_CLAMP) * glm::min(Li, glm::vec3(MAX_COLOR_CLAMP));
				}
			}
		}

		// sample from phase
		{
			glm::vec3 phase_wi;
			float phase_sample_pdf = 0.0f, phase_weight = 0.0f;
			bool sample_valid = SampleMixedPhases(volume_weighs, gs, volume_count, random(), random(), -ray.direction, &phase_weight, &phase_wi, &phase_sample_pdf);

			if (sample_valid)
			{
				glm::vec3 light_dir = phase_wi;
				glm::vec3 light_pos(0.0f), light_pos_error(0.0f), light_geo_normal(0.0f);
				float phase_wi_pdf = 0.0f;
				const glm::vec3 Li = shape_light.EvalLi(scene, volume_hit_position, light_dir, &light_pos, &light_pos_error, &light_geo_normal, &phase_wi_pdf);

				if (MaxComponent(Li) > MIN_COLOR_EPSILON)
				{
					Ray shadow_ray(volume_hit_position, light_dir);
					float max_trace_distance = glm::length(OffsetRayOrigin(light_pos, light_pos_error, -light_dir, light_geo_normal) - volume_hit_position) * SHADOW_RAY_CLAMP;

					shadow_ray.t = max_trace_distance;
					HitRecord shadow_ray_record;
					if (!BVHTraverseShadow(scene, shadow_ray, &shadow_ray_record))
					{
						RayTransfer shadow_ray_transfer(ray_transfer);
						
						shadow_ray = Ray(volume_hit_position, light_dir, EMPTY_UINT32, EMPTY_UINT32);
						shadow_ray.t = max_trace_distance;
						glm::vec3 tr = TraceTr(scene, image_tile_cache, shadow_ray, shadow_ray_transfer, sampler);
						
						float mis_weight = PowerHeuristic(phase_sample_pdf, phase_wi_pdf);
						direct_lighting += mis_weight * tr * glm::min(phase_weight, MAX_COLOR_CLAMP) * glm::min(Li, glm::vec3(MAX_COLOR_CLAMP));
					}
				}
			}
		}

		return light_select_pdf > 0.0f ? (direct_lighting / light_select_pdf) : glm::vec3(0.0f);
	}

	
	__global__ void RenderImage(Scene *scene_ptr,
													ImageTileCache *image_tile_cache_ptr,
													const RenderSetting *render_setting_ptr,
													const HaltonEnumerator *halton_enumerator_ptr,
													const int frame_count,
													glm::vec4 *image,
													uint32_t *prim_idx_buffer,
													uint32_t width,
													uint32_t height,
													uint32_t tile_count_x,
													uint32_t tile_count_y)
	{
		uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
		uint32_t total_pixel_count = TILE_PIXEL_COUNT * tile_count_x * tile_count_y;
		if (!(idx < total_pixel_count)) { 
			return;
		}

		uint32_t tile_id = idx / TILE_PIXEL_COUNT;
		uint32_t tile_x = tile_id % tile_count_x;
		uint32_t tile_y = tile_id / tile_count_x;

		uint32_t tile_pixel_id = idx & (TILE_PIXEL_COUNT - 1);
		uint32_t tile_pixel_x = tile_pixel_id & (TILE_X_RES - 1);
		uint32_t tile_pixel_y = tile_pixel_id / TILE_X_RES;

		uint32_t px = tile_x * TILE_X_RES + tile_pixel_x;
		uint32_t py = tile_y * TILE_Y_RES + tile_pixel_y;
		uint32_t pixel_idx = py * width + px;
		if (!(px < width && py < height)) {
			return;
		}
		
		Scene &scene = *scene_ptr;
		ImageTileCache &image_tile_cache = *image_tile_cache_ptr;

		const RenderSetting &render_setting = *render_setting_ptr;
		const HaltonEnumerator &halton_enumerator = *halton_enumerator_ptr;

		glm::vec3 col(0.0f);
		for (int sample_idx = 0; sample_idx < render_setting.ssp; ++sample_idx)
		{
			RandomSampler sampler;
			glm::vec2 pixel_offset;
			if (render_setting.sampler_type == PCG) 
			{
				sampler.InitPCGSampler(pixel_idx, sample_idx, 0, frame_count - 1);
				pixel_offset = sampler.SamplePixelOffset();
			}
			else if (render_setting.sampler_type == HALTON)
			{
				const uint64_t sample_index = halton_enumerator.GetIndex(px, py, (frame_count - 1) * render_setting.ssp + sample_idx);
				sampler.InitHaltonSampler(sample_index, 2, scene.sampler_data.halton_permute_table);
				pixel_offset = sampler.SamplePixelOffset();
				pixel_offset.x = halton_enumerator.ScaleX(pixel_offset.x) - float(px);
				pixel_offset.y = halton_enumerator.ScaleY(pixel_offset.y) - float(py);
				pixel_offset = glm::clamp(pixel_offset, glm::vec2(0.000001f), glm::vec2(0.999999f));
			}
			else if (render_setting.sampler_type == SOBOL)
			{
				sampler.InitSobolSampler(render_setting.ssp, sample_idx, frame_count - 1, 1, pixel_idx,  scene.sampler_data.sobol_matrices);
				pixel_offset = sampler.SamplePixelOffset();
			}
			else 
			{
				pixel_offset = glm::vec2(0.5f);
			}

			Ray ray = scene.camera->GenerateRay(float(px + pixel_offset.x) / float(width), float(py + pixel_offset.y) / float(height));
			RayTransfer ray_transfer(scene.camera->GetRayTransfer());

			glm::vec3 L(0.0f), throughput(1.0f);
			bool never_scatter = true;
			bool camera_ray = true; // indicate that ray is emitted from camera
			for (int depth = 1;;)
			{
				if (depth > MAX_RAY_DEPTH) { 
					break; 
				}
				
				HitRecord hit_record;
				int nearby_hit_count = 0;
				NearbyHit nearby_hits[MAX_BOUNDARY_RECORD + 2];
				bool hit_surface = TraceRay(scene, ray, &hit_record, &nearby_hit_count, nearby_hits);

				// prim index buffer
				if (sample_idx == 0 && camera_ray) {
					prim_idx_buffer[pixel_idx] = hit_surface ? hit_record.hit_instance_idx : EMPTY_UINT32;
				}

				// volume scatter
				if (render_setting.enable_volume_scattering)
				{
					glm::vec3 tr_weight(1.0f);
					float sampled_distance = 0.0f;

					// current exist volume
					int volume_indices[MAX_BOUNDARY_RECORD];
					float volume_weights[MAX_BOUNDARY_RECORD];
					float gs[MAX_BOUNDARY_RECORD];
					const int overlapped_volume_count = ray_transfer.GetCurrentVolumeIndices(volume_indices, scene.volume_count);
					for (int idx = 0; idx < overlapped_volume_count; ++idx) {
						gs[idx] = scene.volumes[volume_indices[idx]].g;
					}
					
					if (SampleVolumeScattering(scene, image_tile_cache, ray, overlapped_volume_count, volume_indices,  hit_surface ? hit_record.hit_t : TMAX, 
						&tr_weight, &sampled_distance, volume_weights, sampler))
					{
						throughput *= tr_weight;

						glm::vec3 volume_hit_position = ray.PositionAtT(sampled_distance);
						if (render_setting.enable_distant_light) {
							L += throughput * EvalVolumeDistantLight(render_setting, scene, image_tile_cache, overlapped_volume_count, gs, volume_weights, 
								ray, hit_record, ray_transfer, volume_hit_position, sampler);
						}

						L += throughput * EvalVolumeShapeLight(render_setting, scene, image_tile_cache, overlapped_volume_count, gs, volume_weights, 
							ray, hit_record, ray_transfer, volume_hit_position, sampler);

						// sample phase function
						// multiply phase weight
						glm::vec3 wi;
						float pdf = 0.0f, phase_weight = 0.0f;
						const bool continue_bounce = SampleMixedPhases(volume_weights, gs, overlapped_volume_count, 
							sampler.Random1D(), sampler.Random1D(), -ray.direction, &phase_weight, &wi, &pdf);
						if (!continue_bounce) { 
							break; 
						}

						throughput *= phase_weight;
						float rr = glm::max(throughput.x, glm::max(throughput.y, throughput.z));
						if (depth + 1 > render_setting.ray_depth)
						{
							if (render_setting.enable_russian_roulette && sampler.Random1D() < rr) { 
								throughput /= glm::max(rr, MIN_COLOR_EPSILON); 
							}
							else { 
								break;
							}
						}

						ray = Ray(volume_hit_position, wi, EMPTY_UINT32, EMPTY_UINT32);
						never_scatter = false; 
						camera_ray = false;
						++depth;
						continue;
					}
					else {
						throughput *= tr_weight;
					}
				}
			
				// surface shading
				if (hit_surface)
				{
					glm::vec3 hit_position(0.0f);
					glm::vec3 hit_position_error(0.0f);
					glm::vec3 hit_position_object_space(0.0f);
					glm::vec3	hit_shading_normal(0.0f);
					glm::vec3	hit_geometry_normal(0.0f);
					glm::vec2	hit_uv(0.0f);
					glm::vec3	hit_dpdu(0.0f);
					glm::vec3	hit_dpdv(0.0f);
					FetchShadingData(scene,
												hit_record,
												&hit_position,
												&hit_position_error,
												&hit_position_object_space,
												&hit_shading_normal,
												&hit_geometry_normal,
												&hit_uv,
												&hit_dpdu,
												&hit_dpdv);

					const PrimitiveInstance &prim = scene.primitive_instances[hit_record.hit_instance_idx];
					const Material &mtl = scene.materials[prim.material_idx];
					
					uint32_t mtl_ior_priority;
					const float mtl_ior = mtl.FetchIOR(&mtl_ior_priority);
					
					if (!ray_transfer.empty() && mtl_ior_priority < ray_transfer.GetMaxPriorityRecord().ior_priority)
					{
						// note: should not consume depth
						BoundaryTransitionBunch(ray_transfer, scene.primitive_instances, scene.materials, nearby_hits, nearby_hit_count);
						ray = Ray(OffsetRayOrigin(hit_position, hit_position_error, ray.direction, hit_geometry_normal), ray.direction, hit_record.hit_instance_idx, hit_record.hit_triangle_idx);
						camera_ray = false;
						continue;
					}

					if (prim.treat_as_boundary)
					{
						// note: should not consume depth
						// note: although volume is treat as boundary, but the material's ior will still affect its attribute, so remember to set it!
						BoundaryTransitionBunch(ray_transfer, scene.primitive_instances, scene.materials, nearby_hits, nearby_hit_count);
						ray = Ray(OffsetRayOrigin(hit_position, hit_position_error, ray.direction, hit_geometry_normal), ray.direction, hit_record.hit_instance_idx, hit_record.hit_triangle_idx);
						camera_ray = false;
						continue;
					}

					const glm::vec4 tex_coordinates_differentials =
						scene.camera->TextureCoordinatesDifferential(ray,
							hit_position,
							hit_shading_normal,
							hit_geometry_normal,
							hit_dpdu, hit_dpdv,
							width, height,
							0.25f, 0.25f);

					if (mtl.material_type == LIGHT_MTL) {
						if (never_scatter) { L += throughput * mtl.light_mtl.light_color * mtl.light_mtl.intensity; }
						break;
					}

					// indirect light
					MaterialBSDF material_bsdf;
					{
						TextureCoordinate texture_coordinate(hit_uv, hit_position, hit_position_object_space, tex_coordinates_differentials);

						// eval normal mapping or bump mapping, the hit_shading_normal, and dpdu and dpdv will get modified here
						material_bsdf.InitShadingSpace(mtl, 
							hit_record.hit_back,
							ray, 
							hit_geometry_normal, 
							hit_shading_normal, 
							hit_dpdu, 
							hit_dpdv, 
							scene.transforms[prim.transform_idx],
							scene.i_transforms[prim.transform_idx],
							scene.textures,
							image_tile_cache,
							texture_coordinate);
						
						// instance's internal ior is implicitly specified by its material
						material_bsdf.InitBSDFSettings(mtl,
																	scene.textures,
																	image_tile_cache,
																	texture_coordinate,
																	ray,
																	hit_record.hit_back,
																	ray_transfer.GetRayIOR(),
																	ray_transfer.GetExIOR(hit_record.hit_instance_idx));
					}
					
					if (true) 
					{
						// TODO: direct lighting
						// TODO: switch to turn off direct light (done)
						if (render_setting.enable_distant_light) {
							L += throughput * EvalDistantLight(render_setting,
								scene,
								image_tile_cache,
								material_bsdf,
								ray.direction,
								hit_record,
								ray_transfer,
								nearby_hit_count,
								nearby_hits,
								hit_position,
								hit_position_error,
								material_bsdf.GetShadingNormal(),
								hit_geometry_normal,
								sampler);
						}

						// TODO: sample table, light BVH...
						L += throughput * EvalShapeLight(render_setting,
							scene,
							image_tile_cache, 
							material_bsdf,
							ray.direction,
							hit_record,
							ray_transfer,
							nearby_hit_count,
							nearby_hits,
							hit_position,
							hit_position_error,
							material_bsdf.GetShadingNormal(),
							hit_geometry_normal,
							sampler);
					}

					// current a bunch noise are from indirect light!
					// indirect lighting
					glm::vec3 wo = material_bsdf.WorldToShading(-ray.direction), wi(0.0f), bsdf_weight(0.0f);
					float pdf = 0.0f;
					
					bool sample_valid = material_bsdf.SampleWi(wo, &bsdf_weight, &wi, &pdf, sampler.Random1D(), sampler.Random1D(), sampler.Random1D());
					assert(!glm::isnan(bsdf_weight.x) && !glm::isnan(bsdf_weight.y) && !glm::isnan(bsdf_weight.z));
					if (!sample_valid) { break; }

					// indirect light
					throughput *= bsdf_weight;
					float rr = glm::max(throughput.x, glm::max(throughput.y, throughput.z));
					if (depth + 1 > render_setting.ray_depth)
					{
						if (render_setting.enable_russian_roulette && sampler.Random1D() < rr) { 
							throughput /= glm::max(rr, MIN_COLOR_EPSILON); 
						}
						else { break; }
					}

					glm::vec3 new_direction = material_bsdf.ShadingToWorld(wi);
					const bool transmit_boundary = glm::dot(-ray.direction, hit_geometry_normal) * glm::dot(new_direction, hit_geometry_normal) < 0.0f;
					const bool sample_invalid = (!SameHemisphere(wo, wi) && !transmit_boundary) || 
						(SameHemisphere(wo, wi) && transmit_boundary);
					if (sample_invalid) {
						break;
					}
					if (transmit_boundary) {
						BoundaryTransitionBunch(ray_transfer, scene.primitive_instances, scene.materials, nearby_hits, nearby_hit_count);
					}

					// this method to avoid self intersection is still not robust, it makes the sphere self-intersection when radius is big
					bool front_side_bounce = glm::dot(hit_geometry_normal, new_direction) >= 0.0f;
					ray = Ray(OffsetRayOrigin(hit_position, hit_position_error, new_direction, hit_geometry_normal), new_direction, hit_record.hit_instance_idx, hit_record.hit_triangle_idx);
					never_scatter = false;
					camera_ray = false;
					++depth;
				}
				else
				{
					// TODO: switch to turn off env light
					L += render_setting.enable_env_light? throughput * Background(ray.direction) : glm::vec3(0.0f);
					break;
				}
			} // for depth
			col += L;
		} // for sample_idx

		assert(!glm::isnan(col.x));
		image[pixel_idx] = glm::vec4(col / float(render_setting.ssp), 1.0f);
	}

	extern "C" void RenderScene(const Scene &scene,
		const ImageTextureManager &image_texture_manager,
		const RenderSetting &render_setting,
		glm::vec4 *image, 
		uint32_t *prim_idx_buffer, 
		uint32_t width, 
		uint32_t height, 
		int frame_count,
		float *render_time)
	{
		uint32_t tile_count_x = (uint32_t)glm::ceil(float(width) / float(TILE_X_RES));
		uint32_t tile_count_y = (uint32_t)glm::ceil(float(height) / float(TILE_Y_RES));
		uint32_t total_pixel_count = tile_count_x * TILE_X_RES  * tile_count_y * TILE_Y_RES;

		HaltonEnumerator halton_enumerator(width, height);

		auto start_time = std::chrono::high_resolution_clock::now();
		{
			Scene *scene_ptr = nullptr;
			UPLOAD_TO_GPU(scene_ptr, &scene, sizeof(Scene));

			RenderSetting *render_setting_ptr = nullptr;
			UPLOAD_TO_GPU(render_setting_ptr, &render_setting, sizeof(RenderSetting));

			HaltonEnumerator *halton_enumerator_ptr = nullptr;
			UPLOAD_TO_GPU(halton_enumerator_ptr, &halton_enumerator, sizeof(HaltonEnumerator));

			ImageTileCache *image_tile_cache_ptr = nullptr;
			UPLOAD_TO_GPU(image_tile_cache_ptr, &image_texture_manager.image_tile_cache, sizeof(ImageTileCache));

			dim3 block_dim(32, 1, 1);
			dim3 grid_dim(Round_Block_Count(total_pixel_count, block_dim.x), 1, 1);
			void *args[] = {&scene_ptr, 
									&image_tile_cache_ptr,
									&render_setting_ptr, 
									&halton_enumerator_ptr,
									&frame_count,
									&image, 
									&prim_idx_buffer, 
									&width, 
									&height, 
									&tile_count_x, 
									&tile_count_y};
			CUDA_CHECK(cudaLaunchKernel((void*)RenderImage, grid_dim, block_dim, args, 0, 0));
			CUDA_CHECK(cudaStreamSynchronize(0));

			FREE_GPU_RESOURCE(scene_ptr);
			FREE_GPU_RESOURCE(render_setting_ptr);
			FREE_GPU_RESOURCE(halton_enumerator_ptr);
			FREE_GPU_RESOURCE(image_tile_cache_ptr);
		}

		auto end_time = std::chrono::high_resolution_clock::now();
		*render_time = float(std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count()) / 1000.f;
	}

	extern "C" void WaveFrontPathTracing()
	{
	
	}

	__global__ void AccumulateContrib(const glm::vec4 *beauty, 
		glm::vec4 *accumulate, 
		uint32_t width, 
		uint32_t height, 
		int frame_count)
	{
		uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
		uint32_t idy = blockIdx.y * blockDim.y + threadIdx.y;
		if (idx < width && idy < height)
		{
			uint32_t pixel_idx = idy * width + idx;
			accumulate[pixel_idx] = (accumulate[pixel_idx] * float(glm::max(frame_count - 1, 0)) + beauty[pixel_idx]) / float(frame_count);
		}
	}

	extern "C" void AccumulateImage(const glm::vec4 *beauty, glm::vec4 *accumulate, uint32_t width, uint32_t height, int frame_count, const RenderSetting &render_setting)
	{
		assert(beauty != nullptr && accumulate != nullptr);
		if (render_setting.max_frame_count > 0 && frame_count > render_setting.max_frame_count) { return; }

		dim3 block_dim(32, 32, 1);
		dim3 grid_dim(Round_Block_Count(width, block_dim.x), Round_Block_Count(height, block_dim.y), 1);
		void *args[] = { &beauty,
								 &accumulate,
								 &width,
								 &height,
								 &frame_count };
		CUDA_CHECK(cudaLaunchKernel((void*)AccumulateContrib, grid_dim, block_dim, args, 0, 0));
		CUDA_CHECK(cudaStreamSynchronize(0));
	}

	__global__ void DrawPostProcessingEffect(glm::vec4 *postprocess_image, 
		uint32_t *prim_idx_buffer, 
		uint32_t width, 
		uint32_t height, 
		uint32_t select_prim_idx)
	{
		int idx = blockIdx.x * blockDim.x + threadIdx.x;
		int idy = blockIdx.y * blockDim.y + threadIdx.y;
		if (idx < width && idy < height)
		{
			auto in_range = [](int x, int y, int width, int height)->bool
			{
				return x >= 0 && x < width && y >= 0 && y < height;
			};
			auto get_pixel_idx = [](int x, int y, int width, int height)->uint32_t 
			{
				return y * width + x;
			};
			auto gamma_correction = [](const glm::vec3 &col)->glm::vec3
			{
				return glm::pow(col, glm::vec3(1.0f / 2.2f));
			};

			int highlight = 0, total_sample = 0;
			for (int x = -1; x <= 1; ++x) 
			{
				for (int y = -1; y <= 1; ++y)
				{
					int sample_x = idx + x, sample_y = idy + y;
					if (in_range(sample_x, sample_y, width, height))
					{
						++total_sample;
						if (prim_idx_buffer[get_pixel_idx(sample_x, sample_y, width, height)] == select_prim_idx)
						{
							++highlight;
						}
					}
				}
			}

			uint32_t pixel_idx = get_pixel_idx(idx, idy, width, height);
			const glm::vec4 dim_color = postprocess_image[pixel_idx];
			
			glm::vec3 result_col(0.0f);
			if (highlight == 0)
			{	
				result_col = glm::vec3(dim_color);
			}
			else if (highlight == total_sample)
			{
				result_col = glm::mix(glm::vec3(dim_color), glm::vec3(0.9f, 0.2f, 0.2f), 0.6f);
			}
			else 
			{
				result_col = glm::vec3(1.0f, 0.0f, 0.0f);
			}
			postprocess_image[pixel_idx] = glm::vec4(result_col, 1.0f);
		}
	}

	__global__ void GammaCorrection(glm::vec4 *postprocess_image,
		const RenderSetting render_setting,
		uint32_t width,
		uint32_t height)
	{
		int idx = blockIdx.x * blockDim.x + threadIdx.x;
		int idy = blockIdx.y * blockDim.y + threadIdx.y;
		if (idx < width && idy < height)
		{
			auto get_pixel_idx = [](int x, int y, int width, int height)->uint32_t {
				return y * width + x;
			};
			auto gamma_correction = [&](const glm::vec3& col)->glm::vec3 {
				return glm::pow(col, glm::vec3(1.0f / render_setting.gamma));
			};

			auto aces_tonemapping = [&](glm::vec3 col, float exposure)->glm::vec3 
			{
				const float a = 2.51f;
				const float b = 0.03f;
				const float c = 2.43f;
				const float d = 0.59f;
				const float e = 0.14f;
				col *= exposure;
				return (col * (a * col + b)) / (col * (c * col + d) + e);
			};

			uint32_t pixel_idx = get_pixel_idx(idx, idy, width, height);
			glm::vec3 result_col = aces_tonemapping(glm::vec3(postprocess_image[pixel_idx]), render_setting.exposure);
			postprocess_image[pixel_idx] = glm::vec4(gamma_correction(result_col), 1.0f);
		}
	}

	extern "C" void PostProcessing(glm::vec4 *postprocess_image, uint32_t *prim_idx_buffer, const RenderSetting &render_setting, uint32_t width, uint32_t height, uint32_t select_prim_idx, bool disable_object_highlight)
	{
		assert(postprocess_image != nullptr && prim_idx_buffer != nullptr);

		dim3 block_dim(32, 32, 1);
		dim3 grid_dim(Round_Block_Count(width, block_dim.x), Round_Block_Count(height, block_dim.y), 1);
		
		RenderSetting rt_settings = render_setting;

		if (select_prim_idx != EMPTY_UINT32 && !disable_object_highlight) 
		{
			void *args[] = { &postprocess_image,
									 &prim_idx_buffer,
									 &width,
									 &height,
									 &select_prim_idx };
			CUDA_CHECK(cudaLaunchKernel((void*)DrawPostProcessingEffect, grid_dim, block_dim, args, 0, 0));
			CUDA_CHECK(cudaStreamSynchronize(0));
		}
		
		{
			// do gamma correction
			void *args[] = { &postprocess_image,
									 &rt_settings,
									 &width,
									 &height};
			CUDA_CHECK(cudaLaunchKernel((void*)GammaCorrection, grid_dim, block_dim, args, 0, 0));
			CUDA_CHECK(cudaStreamSynchronize(0));
		}
	}

	extern "C" void AllocateRayCounter(RayCounterData & ray_counter_data) {
		ray_counter_data.ray_counter_host = (RayCounter*)_aligned_malloc(sizeof(RayCounter), 4ull);
		assert(ray_counter_data.ray_counter_host != nullptr);
		auto& ray_counter = *(ray_counter_data.ray_counter_host);
		ray_counter.hit_counter = 0;
		ray_counter.shading_ray_counter = 0;
		ray_counter.shadow_ray_counter = 0;
		UPLOAD_TO_GPU(ray_counter_data.ray_counter_device, &ray_counter, sizeof(RayCounter));
	}

	extern "C" void ReleaseRayCounter(RayCounterData & ray_counter_data) {
		_aligned_free(ray_counter_data.ray_counter_host);
		CUDA_CHECK(cudaFree(ray_counter_data.ray_counter_device));
	}

	extern "C" void AllocateShadingRayData(ShadingRayData & shading_ray_data) {
		// TODO: memory consume profile.
		CUDA_CHECK(cudaMalloc(&(shading_ray_data.shading_ray_data_ray), sizeof(Ray) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(shading_ray_data.shading_ray_data_received_light), sizeof(glm::vec3) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(shading_ray_data.shading_ray_data_throughput), sizeof(glm::vec3) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(shading_ray_data.shading_ray_data_pixel_position_x), sizeof(int) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(shading_ray_data.shading_ray_data_pixel_position_y), sizeof(int) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(shading_ray_data.shading_ray_data_ray_depth), sizeof(int) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(shading_ray_data.shading_ray_data_never_scatter), sizeof(int) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(shading_ray_data.shading_ray_data_camera_ray), sizeof(int) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(shading_ray_data.shading_ray_data_pixel_sample_index), sizeof(int) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(shading_ray_data.shading_ray_data_sampler), sizeof(RandomSampler) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(shading_ray_data.shading_ray_data_ray_transfer), sizeof(RayTransfer) * MAX_RAY_COUNT));
	}

	extern "C" void ReleaseShadingRayData(ShadingRayData & shading_ray_data) {
		FREE_GPU_RESOURCE(shading_ray_data.shading_ray_data_ray);
		FREE_GPU_RESOURCE(shading_ray_data.shading_ray_data_received_light);
		FREE_GPU_RESOURCE(shading_ray_data.shading_ray_data_throughput);
		FREE_GPU_RESOURCE(shading_ray_data.shading_ray_data_pixel_position_x);
		FREE_GPU_RESOURCE(shading_ray_data.shading_ray_data_pixel_position_y);
		FREE_GPU_RESOURCE(shading_ray_data.shading_ray_data_ray_depth);
		FREE_GPU_RESOURCE(shading_ray_data.shading_ray_data_never_scatter);
		FREE_GPU_RESOURCE(shading_ray_data.shading_ray_data_camera_ray);
		FREE_GPU_RESOURCE(shading_ray_data.shading_ray_data_pixel_sample_index);
		FREE_GPU_RESOURCE(shading_ray_data.shading_ray_data_sampler);
		FREE_GPU_RESOURCE(shading_ray_data.shading_ray_data_ray_transfer);
	}

	extern "C" void AllocateHitData(HitData & hit_data) {
		// TODO: memory consume profile.
		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_hit_record), sizeof(HitRecord) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_nearby_hit_count), sizeof(int) * MAX_RAY_COUNT));
		
		const size_t max_nearby_hit_count = MAX_BOUNDARY_RECORD + 2;
		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_nearby_hit_primitive_index), sizeof(uint32_t) * max_nearby_hit_count * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_nearby_t_hit), sizeof(float) * max_nearby_hit_count * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_nearby_hit_back), sizeof(int) * max_nearby_hit_count * MAX_RAY_COUNT));

		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_volume_weights), sizeof(float) * MAX_BOUNDARY_RECORD * MAX_RAY_COUNT));

		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_ray), sizeof(Ray) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_received_light), sizeof(glm::vec3) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_throughput), sizeof(glm::vec3) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_pixel_position_x), sizeof(int) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_pixel_position_y), sizeof(int) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_ray_depth), sizeof(int) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_never_scatter), sizeof(int) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_camera_ray), sizeof(int) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_pixel_sample_index), sizeof(int) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_sampler), sizeof(RandomSampler) * MAX_RAY_COUNT));
		CUDA_CHECK(cudaMalloc(&(hit_data.hit_data_ray_transfer), sizeof(RayTransfer) * MAX_RAY_COUNT));
	}

	extern "C" void ReleaseHitData(HitData & hit_data) {
		FREE_GPU_RESOURCE(hit_data.hit_data_hit_record);
		FREE_GPU_RESOURCE(hit_data.hit_data_nearby_hit_count);

		FREE_GPU_RESOURCE(hit_data.hit_data_nearby_hit_primitive_index);
		FREE_GPU_RESOURCE(hit_data.hit_data_nearby_t_hit);
		FREE_GPU_RESOURCE(hit_data.hit_data_nearby_hit_back);

		FREE_GPU_RESOURCE(hit_data.hit_data_volume_weights);

		FREE_GPU_RESOURCE(hit_data.hit_data_ray);
		FREE_GPU_RESOURCE(hit_data.hit_data_received_light);
		FREE_GPU_RESOURCE(hit_data.hit_data_throughput);
		FREE_GPU_RESOURCE(hit_data.hit_data_pixel_position_x);
		FREE_GPU_RESOURCE(hit_data.hit_data_pixel_position_y);
		FREE_GPU_RESOURCE(hit_data.hit_data_ray_depth);
		FREE_GPU_RESOURCE(hit_data.hit_data_never_scatter);
		FREE_GPU_RESOURCE(hit_data.hit_data_camera_ray);
		FREE_GPU_RESOURCE(hit_data.hit_data_pixel_sample_index);
		FREE_GPU_RESOURCE(hit_data.hit_data_sampler);
		FREE_GPU_RESOURCE(hit_data.hit_data_ray_transfer);
	}

	extern "C" void AllocateShadowRayData(ShadowRayData & shadow_ray_data) {
		// TODO: memory consume profile.
	}

	extern "C" void ReleaseShadowRayData(ShadowRayData & shadow_ray_data) {

	}

	__global__ void ImguiTestDrawing(const Scene *scene_ptr, glm::vec4 *beauty, uint32_t *primitive_index, const int width, const int height)
	{
		const int idx = blockIdx.x * blockDim.x + threadIdx.x;
		const int idy = blockIdx.y * blockDim.y + threadIdx.y;
		if (idx < width && idy < height) 
		{
			const uint32_t pixel_idx = idy * width + idx;
			float ndc_x = float(idx) / float(width);
			float ndc_y = float(idy) / float(height);
			
			const auto& scene = *scene_ptr;
			Ray ray = scene.camera[EDITOR_CAMERA_INDEX].GenerateRay(ndc_x, ndc_y);
			
			HitRecord hit_record;
			int nearby_hit_count = 0;
			NearbyHit nearby_hits[MAX_BOUNDARY_RECORD + 2];
			bool hit_surface = TraceRay(scene, ray, &hit_record, &nearby_hit_count, nearby_hits);

			primitive_index[pixel_idx] = hit_surface ? hit_record.hit_instance_idx : EMPTY_UINT32;

			glm::vec3 surface_albedo(0.0f);
			if (hit_surface) {
				glm::vec3 hit_position(0.0f), hit_position_error(0.0f), hit_position_object_space(0.0f);
				glm::vec3	hit_shading_normal(0.0f), hit_geometry_normal(0.0f);
				glm::vec2	hit_uv(0.0f);
				glm::vec3	hit_dpdu(0.0f), hit_dpdv(0.0f);
				FetchShadingData(scene, hit_record,
					&hit_position, &hit_position_error, &hit_position_object_space,
					&hit_shading_normal, &hit_geometry_normal,
					&hit_uv, 
					&hit_dpdu, &hit_dpdv);

				auto material_index = scene.primitive_instances[hit_record.hit_instance_idx].material_idx;
				if (material_index != EMPTY_UINT32) {
					auto& material = scene.materials[material_index];
					if (material.material_type == DEFAULT_MTL) {
						if (material.default_mtl.diffuse_albedo_tex == EMPTY_UINT32) {
							surface_albedo = material.default_mtl.diffuse_albedo;
						}
						else {
							TextureCoordinate texture_coordinate(hit_uv, hit_position, hit_position_object_space, glm::vec4(0.0f));
							ImageTileCache tile_cache;

							surface_albedo = TextureEval(scene.textures[material.default_mtl.diffuse_albedo_tex], 
								scene.textures, tile_cache, texture_coordinate);
						}
					}
				}
			}

			beauty[pixel_idx] = glm::vec4(hit_surface ? surface_albedo : Background(ray.direction), 1.0f);
		}
	}

	extern "C" void ImguiTestingAov(const Scene &scene, glm::vec4 *beauty, uint32_t *primitive_index, int width, int height, cudaStream_t &stream)
	{
		Scene *scene_device = nullptr;
		// answer to whether can use pinned memory: https://forums.developer.nvidia.com/t/does-cudamemcpyasync-require-pinned-memory/40411/2.
		CUDA_CHECK(cudaMallocAsync(&scene_device, sizeof(Scene), stream));
		CUDA_CHECK(cudaMemcpyAsync(scene_device, &scene, sizeof(Scene), cudaMemcpyHostToDevice, stream));

		dim3 block_dim(4, 4, 1);
		dim3 grid_dim(Round_Block_Count(width, block_dim.x), Round_Block_Count(height, block_dim.y), 1);

		void* args[] = { &scene_device,
								 &beauty,
								 &primitive_index,
								 &width,
								 &height };
		CUDA_CHECK(cudaLaunchKernel((void*)ImguiTestDrawing, grid_dim, block_dim, args, 0, stream));
		CUDA_CHECK(cudaStreamSynchronize(stream));

		FREE_GPU_RESOURCE(scene_device);
	}

	__global__ void ImguiTestPathTracing(const Scene *scene, glm::vec4 *beauty, const int width, const int height)
	{
		const int idx = blockIdx.x * blockDim.x + threadIdx.x;
		const int idy = blockIdx.y * blockDim.y + threadIdx.y;
		if (idx < width && idy < height)
		{
			const uint32_t pixel_idx = idy * width + idx;
			float ndc_x = float(idx) / float(width);
			float ndc_y = float(idy) / float(height);
			Ray ray = scene->camera[RENDERING_CAMERA_INDEX].GenerateRay(ndc_x, ndc_y);
			beauty[pixel_idx] = glm::vec4(Background(ray.direction), 1.0f);
		}
	}

	extern "C" void ImguiTestingRendering(const Scene &scene, glm::vec4 *beauty, int width, int height, cudaStream_t &stream)
	{
		Scene *scene_device = nullptr;
		// answer to whether can use pinned memory: https://forums.developer.nvidia.com/t/does-cudamemcpyasync-require-pinned-memory/40411/2.
		CUDA_CHECK(cudaMallocAsync(&scene_device, sizeof(Scene), stream));
		CUDA_CHECK(cudaMemcpyAsync(scene_device, &scene, sizeof(Scene), cudaMemcpyHostToDevice, stream));

		dim3 block_dim(8, 8, 1);
		dim3 grid_dim(Round_Block_Count(width, block_dim.x), Round_Block_Count(height, block_dim.y), 1);

		void* args[] = { &scene_device,
								 &beauty,
								 &width,
								 &height };
		CUDA_CHECK(cudaLaunchKernel((void*)ImguiTestPathTracing, grid_dim, block_dim, args, 0, stream));
		CUDA_CHECK(cudaStreamSynchronize(stream));

		FREE_GPU_RESOURCE(scene_device);
	}

	__global__ void Ray_generation_editor_view(const int current_frame_index,
		const Scene * scene_device, 
		const RayCounter * ray_counter_device, 
		const uint32_t next_tile_index, 
		const uint32_t ray_count_this_batch, 
		const uint32_t width, 
		const uint32_t height,
		const uint32_t tile_count_x,
		const uint32_t tile_count_y,
		Ray * shading_ray_data_ray,
		int * shading_ray_data_pixel_position_x,
		int * shading_ray_data_pixel_position_y,
		int * shading_ray_data_camera_ray,
		RayTransfer * shading_ray_data_ray_transfer)
	{
		const uint32_t thread_index = blockIdx.x * blockDim.x + threadIdx.x;
		if (!(thread_index < ray_count_this_batch)) {
			return;
		}
		const auto& ray_counter = *(ray_counter_device);
		
		const uint32_t ray_index = ray_counter.shading_ray_counter + thread_index;

		const uint32_t tile_index_this_batch = thread_index / TILE_PIXEL_COUNT;
		const uint32_t tile_index_y = (tile_index_this_batch + next_tile_index) / tile_count_x;
		const uint32_t tile_index_x = (tile_index_this_batch + next_tile_index) - tile_count_x * tile_index_y;

		const uint32_t tile_local_sample_index = thread_index - tile_index_this_batch * TILE_PIXEL_COUNT;
		const uint32_t tile_pixel_index = tile_local_sample_index;
		const uint32_t pixel_sample_index = tile_local_sample_index - tile_pixel_index;
		assert(pixel_sample_index == 0);

		const uint32_t tile_pixel_index_y = tile_pixel_index / TILE_X_RES;
		const uint32_t tile_pixel_index_x = tile_pixel_index - tile_pixel_index_y * TILE_X_RES;

		const uint32_t pixel_index_x = tile_index_x * TILE_X_RES + tile_pixel_index_x;
		const uint32_t pixel_index_y = tile_index_y * TILE_Y_RES + tile_pixel_index_y;

		shading_ray_data_pixel_position_x[ray_index] = pixel_index_x;
		shading_ray_data_pixel_position_y[ray_index] = pixel_index_y;
		if (!(pixel_index_x < width && pixel_index_y < height)) {
			return;
		}

		const auto& scene = *scene_device;
		const auto& render_setting = *scene.render_setting;
		const auto& image_tile_cache = *scene.image_tile_cache;
		
		const uint32_t pixel_index = pixel_index_y * width + pixel_index_x;

		Ray ray = scene.camera[EDITOR_CAMERA_INDEX].GenerateRay(float(pixel_index_x + 0.5f) / float(width), float(pixel_index_y + 0.5f) / float(height));

		shading_ray_data_ray[ray_index] = ray;
		shading_ray_data_camera_ray[ray_index] = true;
		shading_ray_data_ray_transfer[ray_index] = scene.camera[EDITOR_CAMERA_INDEX].GetRayTransfer();
	}

	extern "C" void RayGenerationEditorView(int current_frame_index, const RenderSetting & render_setting, Scene * scene_device, RayCounter * ray_counter_device, uint32_t next_tile_index, uint32_t tile_count_this_batch, uint32_t width, uint32_t height, uint32_t tile_count_x, uint32_t tile_count_y, ShadingRayData & shading_ray_data, cudaStream_t & stream)
	{
		uint32_t ray_count_this_batch = tile_count_this_batch * TILE_PIXEL_COUNT;
		if (ray_count_this_batch == 0) {
			return;
		}

		dim3 block_dim(STAGE_RAY_GEN_BLOCK_SIZE, 1, 1);
		dim3 grid_dim(Round_Block_Count(ray_count_this_batch, block_dim.x), 1, 1);

		void* args[] = { &current_frame_index,
								 &scene_device,
								 &ray_counter_device,
								 &next_tile_index,
								 &ray_count_this_batch,
								 &width,
								 &height,
								 &tile_count_x,
								 &tile_count_y,
								 &shading_ray_data.shading_ray_data_ray,
								 &shading_ray_data.shading_ray_data_pixel_position_x,
								 &shading_ray_data.shading_ray_data_pixel_position_y,
								 &shading_ray_data.shading_ray_data_camera_ray,
								 &shading_ray_data.shading_ray_data_ray_transfer };

		CUDA_CHECK(cudaLaunchKernel((void*)Ray_generation_editor_view, grid_dim, block_dim, args, 0, stream));
		CUDA_CHECK(cudaStreamSynchronize(stream));
	}

	__global__ void Ray_trace_editor_view(const int current_frame_index,
		const Scene * scene_device,
		RayCounter * ray_counter_device,
		const uint32_t total_ray_count_this_batch,
		const uint32_t width,
		const uint32_t height,
		glm::vec4 * beauty,
		uint32_t * primitive_index,
		const Ray * shading_ray_data_ray, // note: shading ray data.
		const int * shading_ray_data_pixel_position_x,
		const int * shading_ray_data_pixel_position_y,
		const int * shading_ray_data_camera_ray,
		const RayTransfer * shading_ray_data_ray_transfer,
		HitRecord * hit_data_hit_record, // note: hit data.
		int * hit_data_nearby_hit_count,
		uint32_t * hit_data_nearby_hit_primitive_index,
		float * hit_data_nearby_t_hit,
		int * hit_data_nearby_hit_back,
		Ray * hit_data_ray,
		int * hit_data_pixel_position_x,
		int * hit_data_pixel_position_y,
		int * hit_data_camera_ray,
		RayTransfer * hit_data_ray_transfer)
	{
		const uint32_t ray_index = blockIdx.x * blockDim.x + threadIdx.x;
		if (!(ray_index < total_ray_count_this_batch)) {
			return;
		}

		const uint32_t pixel_position_x = shading_ray_data_pixel_position_x[ray_index];
		const uint32_t pixel_position_y = shading_ray_data_pixel_position_y[ray_index];
		const uint32_t pixel_index = pixel_position_y * width + pixel_position_x;
		if (!(pixel_position_x < width && pixel_position_y < height)) {
			return;
		}

		const auto& scene = *scene_device;
		const auto& ray_counter = *ray_counter_device;
		const auto& render_setting = *scene.render_setting;
		const auto& image_tile_cache = *scene.image_tile_cache;

		const Ray ray = shading_ray_data_ray[ray_index];

		HitRecord hit_record;
		int nearby_hit_count = 0;
		NearbyHit nearby_hits[MAX_BOUNDARY_RECORD + 2];
		const bool hit_something = TraceRay(scene, ray, &hit_record, &nearby_hit_count, nearby_hits);

		if (!hit_something) {
			const glm::vec4 col = glm::vec4((render_setting.enable_env_light ? Background(ray.direction) : glm::vec3(0.0f)), 1.0f);
			AtomicAddFloat(&(beauty[pixel_index][0]), col[0]);
			AtomicAddFloat(&(beauty[pixel_index][1]), col[1]);
			AtomicAddFloat(&(beauty[pixel_index][2]), col[2]);
			AtomicAddFloat(&(beauty[pixel_index][3]), col[3]);
			return;
		}

		// TODO: compute hit's morton code for sorting, we should classify volume hit and surface hit.
		const uint32_t hit_index = AtomicAddInt((int*)(&(ray_counter.hit_counter)), 1);
			
		hit_data_hit_record[hit_index] = hit_record;
		hit_data_nearby_hit_count[hit_index] = nearby_hit_count;
		const uint32_t nearby_hits_start_index = (MAX_BOUNDARY_RECORD + 2) * hit_index;

#pragma unroll (MAX_BOUNDARY_RECORD + 2)
		for (uint32_t nearby_hit_index = 0; nearby_hit_index < MAX_BOUNDARY_RECORD + 2; ++nearby_hit_index) {
			const uint32_t actual_index = nearby_hits_start_index + nearby_hit_index;
			hit_data_nearby_hit_primitive_index[actual_index] = nearby_hits[nearby_hit_index].hit_prim_idx;
			hit_data_nearby_t_hit[actual_index] = nearby_hits[nearby_hit_index].t_hit;
			hit_data_nearby_hit_back[actual_index] = nearby_hits[nearby_hit_index].hit_back;
		}

		hit_data_ray[hit_index] = ray; // note: local ray's t may already get modified.
		hit_data_pixel_position_x[hit_index] = shading_ray_data_pixel_position_x[ray_index];
		hit_data_pixel_position_y[hit_index] = shading_ray_data_pixel_position_y[ray_index];
		hit_data_camera_ray[hit_index] = shading_ray_data_camera_ray[ray_index];
		hit_data_ray_transfer[hit_index] = shading_ray_data_ray_transfer[ray_index];
	}

	extern "C" void RayTraceEditorView(int current_frame_index, const RenderSetting & render_setting, Scene * scene_device, RayCounter * ray_counter_device, RayCounter * ray_counter_host, uint32_t width, uint32_t height, glm::vec4 * beauty, uint32_t * primitive_index, ShadingRayData & shading_ray_data, HitData & hit_data, cudaStream_t & stream)
	{
		uint32_t total_ray_count_this_batch = ray_counter_host->shading_ray_counter;
		if (total_ray_count_this_batch == 0) {
			return;
		}

		dim3 block_dim(STAGE_RAY_TRACE_BLOCK_SIZE, 1, 1);
		dim3 grid_dim(Round_Block_Count(total_ray_count_this_batch, block_dim.x), 1, 1);

		void* args[] = { &current_frame_index,
								 &scene_device,
								 &ray_counter_device,
								 &total_ray_count_this_batch,
								 &width,
								 &height,
								 &beauty,
								 &primitive_index,
								 &shading_ray_data.shading_ray_data_ray,
								 &shading_ray_data.shading_ray_data_pixel_position_x,
								 &shading_ray_data.shading_ray_data_pixel_position_y,
								 &shading_ray_data.shading_ray_data_camera_ray,
								 &shading_ray_data.shading_ray_data_ray_transfer,
								 &hit_data.hit_data_hit_record,
								 &hit_data.hit_data_nearby_hit_count,
								 &hit_data.hit_data_nearby_hit_primitive_index,
							 	 &hit_data.hit_data_nearby_t_hit,
								 &hit_data.hit_data_nearby_hit_back,
								 &hit_data.hit_data_ray,
								 &hit_data.hit_data_pixel_position_x,
								 &hit_data.hit_data_pixel_position_y,
								 &hit_data.hit_data_camera_ray,
								 &hit_data.hit_data_ray_transfer };

		CUDA_CHECK(cudaLaunchKernel((void*)Ray_trace_editor_view, grid_dim, block_dim, args, 0, stream));
		CUDA_CHECK(cudaStreamSynchronize(stream));
	}

	__global__ void Ray_shading_editor_view(const int current_frame_index,
		const Scene * scene_device,
		RayCounter * ray_counter_device,
		const uint32_t total_hit_count_this_batch,
		const uint32_t width,
		const uint32_t height,
		glm::vec4 * beauty,
		uint32_t * primitive_index,
		Ray * shading_ray_data_ray, // note: shading ray data.
		int * shading_ray_data_pixel_position_x,
		int * shading_ray_data_pixel_position_y,
		int * shading_ray_data_camera_ray,
		RayTransfer * shading_ray_data_ray_transfer,
		const HitRecord * hit_data_hit_record, // note: hit data.
		const int * hit_data_nearby_hit_count,
		const uint32_t * hit_data_nearby_hit_primitive_index,
		const float * hit_data_nearby_t_hit,
		const int * hit_data_nearby_hit_back,
		const Ray * hit_data_ray,
		const int * hit_data_pixel_position_x,
		const int * hit_data_pixel_position_y,
		const int * hit_data_camera_ray,
		const RayTransfer * hit_data_ray_transfer)
	{
		const uint32_t hit_index = blockIdx.x * blockDim.x + threadIdx.x;
		if (!(hit_index < total_hit_count_this_batch)) {
			return;
		}

		const uint32_t pixel_position_x = hit_data_pixel_position_x[hit_index];
		const uint32_t pixel_position_y = hit_data_pixel_position_y[hit_index];
		const uint32_t pixel_index = pixel_position_y * width + pixel_position_x;
		assert(pixel_position_x < width&& pixel_position_y < height);

		const auto& scene = *scene_device;
		const auto& ray_counter = *ray_counter_device;
		const auto& render_setting = *scene.render_setting;
		const auto& image_tile_cache = *scene.image_tile_cache;

		Ray ray = hit_data_ray[hit_index];
		RayTransfer ray_transfer = hit_data_ray_transfer[hit_index];
		int camera_ray = hit_data_camera_ray[hit_index];
		
		const HitRecord hit_record = hit_data_hit_record[hit_index];
		const int nearby_hit_count = hit_data_nearby_hit_count[hit_index];
		NearbyHit nearby_hits[MAX_BOUNDARY_RECORD + 2];
		const uint32_t nearby_hits_start_index = (MAX_BOUNDARY_RECORD + 2) * hit_index;

#pragma unroll (MAX_BOUNDARY_RECORD + 2)
		for (uint32_t nearby_hit_index = 0; nearby_hit_index < MAX_BOUNDARY_RECORD + 2; ++nearby_hit_index) {
			const uint32_t actual_index = nearby_hits_start_index + nearby_hit_index;
			auto& nearby_hit = nearby_hits[nearby_hit_index];
			nearby_hit.hit_prim_idx = hit_data_nearby_hit_primitive_index[actual_index];
			nearby_hit.t_hit = hit_data_nearby_t_hit[actual_index];
			nearby_hit.hit_back = hit_data_nearby_hit_back[actual_index];
		}

		auto write_color = [&beauty, &pixel_index](const glm::vec4 &col) {
			AtomicAddFloat(&(beauty[pixel_index][0]), col[0]);
			AtomicAddFloat(&(beauty[pixel_index][1]), col[1]);
			AtomicAddFloat(&(beauty[pixel_index][2]), col[2]);
			AtomicAddFloat(&(beauty[pixel_index][3]), col[3]);
		};

		const bool hit_something = hit_record.hit_instance_idx != EMPTY_UINT32;
		if (camera_ray) {
			AtomicExchInt((int*)(&primitive_index[pixel_index]), hit_something? scene.primitive_instances[hit_record.hit_instance_idx].unique_index : EMPTY_UINT32);
		}

		glm::vec3 hit_position(0.0f), hit_position_error(0.0f), hit_position_object_space(0.0f);
		glm::vec3	hit_shading_normal(0.0f), hit_geometry_normal(0.0f);
		glm::vec2	hit_uv(0.0f);
		glm::vec3	hit_dpdu(0.0f), hit_dpdv(0.0f);
		FetchShadingData(scene, hit_record,
			&hit_position, &hit_position_error, &hit_position_object_space,
			&hit_shading_normal, &hit_geometry_normal,
			&hit_uv,
			&hit_dpdu, &hit_dpdv);

		const auto &primitive = scene.primitive_instances[hit_record.hit_instance_idx];
		const auto material_index = scene.primitive_instances[hit_record.hit_instance_idx].material_idx;
		if (!(material_index < scene.material_count)) {
			write_color(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			return;
		}

		const auto &material = scene.materials[material_index];
		if (primitive.treat_as_boundary) {
			BoundaryTransitionBunch(ray_transfer, scene.primitive_instances, scene.materials, nearby_hits, nearby_hit_count);
			Ray indirect_ray = Ray(OffsetRayOrigin(hit_position, hit_position_error, ray.direction, hit_geometry_normal), ray.direction, hit_record.hit_instance_idx, hit_record.hit_triangle_idx);
			camera_ray = false;

			const uint32_t indirect_ray_index = AtomicAddInt((int*)(&(ray_counter.shading_ray_counter)), 1);
			shading_ray_data_ray[indirect_ray_index] = indirect_ray;
			shading_ray_data_pixel_position_x[indirect_ray_index] = pixel_position_x;
			shading_ray_data_pixel_position_y[indirect_ray_index] = pixel_position_y;
			shading_ray_data_camera_ray[indirect_ray_index] = camera_ray;
			shading_ray_data_ray_transfer[indirect_ray_index] = ray_transfer;
			return;
		}

		const glm::vec4 tex_coordinates_differentials =
			scene.camera->TextureCoordinatesDifferential(ray,
				hit_position,
				hit_shading_normal,
				hit_geometry_normal,
				hit_dpdu, hit_dpdv,
				width, height,
				0.25f, 0.25f);

		const TextureCoordinate texture_coordinate(hit_uv, hit_position, hit_position_object_space, tex_coordinates_differentials);
		const glm::vec3 surface_color = MaterialSurfaceCol(material, scene.textures, image_tile_cache, texture_coordinate);
		write_color(glm::vec4(surface_color, 1.0f));
	}

	extern "C" void RayShadingEditorView(int current_frame_index, const RenderSetting & render_setting, Scene * scene_device, RayCounter * ray_counter_device, RayCounter * ray_counter_host, uint32_t width, uint32_t height, glm::vec4 * beauty, uint32_t * primitive_index, ShadingRayData & shading_ray_data, HitData & hit_data, ShadowRayData & shadow_ray_data, cudaStream_t & stream)
	{
		uint32_t total_hit_count_this_batch = ray_counter_host->hit_counter;
		if (total_hit_count_this_batch == 0) {
			return;
		}

		dim3 block_dim(STAGE_RAY_SHADING_BLOCK_SIZE, 1, 1);
		dim3 grid_dim(Round_Block_Count(total_hit_count_this_batch, block_dim.x), 1, 1);

		void* args[] = { &current_frame_index,
								 &scene_device,
								 &ray_counter_device,
								 &total_hit_count_this_batch,
								 &width,
								 &height,
								 &beauty,
								 &primitive_index,
								 &shading_ray_data.shading_ray_data_ray,
								 &shading_ray_data.shading_ray_data_pixel_position_x,
								 &shading_ray_data.shading_ray_data_pixel_position_y,
								 &shading_ray_data.shading_ray_data_camera_ray,
								 &shading_ray_data.shading_ray_data_ray_transfer,
								 &hit_data.hit_data_hit_record,
								 &hit_data.hit_data_nearby_hit_count,
								 &hit_data.hit_data_nearby_hit_primitive_index,
								 &hit_data.hit_data_nearby_t_hit,
								 &hit_data.hit_data_nearby_hit_back,
								 &hit_data.hit_data_ray,
								 &hit_data.hit_data_pixel_position_x,
								 &hit_data.hit_data_pixel_position_y,
								 &hit_data.hit_data_camera_ray,
								 &hit_data.hit_data_ray_transfer };

		CUDA_CHECK(cudaLaunchKernel((void*)Ray_shading_editor_view, grid_dim, block_dim, args, 0, stream));
		CUDA_CHECK(cudaStreamSynchronize(stream));
	}

	__global__ void Ray_generation_path_tracing(const int current_frame_index,
		const Scene * scene_device,
		const HaltonEnumerator * halton_enumerator_device,
		const RayCounter * ray_counter_device,
		const uint32_t next_tile_index,
		const uint32_t ray_count_this_batch,
		const uint32_t sample_per_pixel,
		const uint32_t width,
		const uint32_t height,
		const uint32_t tile_count_x,
		const uint32_t tile_count_y,
		Ray * shading_ray_data_ray,
		glm::vec3 * shading_ray_data_received_light,
		glm::vec3 * shading_ray_data_throughput,
		int * shading_ray_data_pixel_position_x,
		int * shading_ray_data_pixel_position_y,
		int * shading_ray_data_ray_depth,
		int * shading_ray_data_never_scatter,
		int * shading_ray_data_camera_ray,
		int * shading_ray_data_pixel_sample_index,
		RandomSampler * shading_ray_data_sampler,
		RayTransfer * shading_ray_data_ray_transfer)
	{
		const uint32_t thread_index = blockIdx.x * blockDim.x + threadIdx.x;
		if (!(thread_index < ray_count_this_batch)) {
			return;
		}
		const auto& ray_counter = *(ray_counter_device);

		const uint32_t ray_index = ray_counter.shading_ray_counter + thread_index;

		const uint32_t tile_index_this_batch = thread_index / (TILE_PIXEL_COUNT * sample_per_pixel);
		const uint32_t tile_index_y = (tile_index_this_batch + next_tile_index) / tile_count_x;
		const uint32_t tile_index_x = (tile_index_this_batch + next_tile_index) - tile_count_x * tile_index_y;

		const uint32_t tile_local_sample_index = thread_index - tile_index_this_batch * (TILE_PIXEL_COUNT * sample_per_pixel);
		const uint32_t tile_pixel_index = tile_local_sample_index / sample_per_pixel;
		const uint32_t pixel_sample_index = tile_local_sample_index - tile_pixel_index * sample_per_pixel;

		const uint32_t tile_pixel_index_y = tile_pixel_index / TILE_X_RES;
		const uint32_t tile_pixel_index_x = tile_pixel_index - tile_pixel_index_y * TILE_X_RES;

		const uint32_t pixel_index_x = tile_index_x * TILE_X_RES + tile_pixel_index_x;
		const uint32_t pixel_index_y = tile_index_y * TILE_Y_RES + tile_pixel_index_y;

		shading_ray_data_pixel_position_x[ray_index] = pixel_index_x;
		shading_ray_data_pixel_position_y[ray_index] = pixel_index_y;
		if (!(pixel_index_x < width && pixel_index_y < height)) {
			return;
		}

		const auto& scene = *scene_device;
		const auto& halton_enumerator = *halton_enumerator_device;
		const auto& render_setting = *scene.render_setting;
		const auto& image_tile_cache = *scene.image_tile_cache;

		const uint32_t pixel_index = pixel_index_y * width + pixel_index_x;

		RandomSampler sampler;
		glm::vec2 pixel_offset;
		if (render_setting.sampler_type == PCG) {
			sampler.InitPCGSampler(pixel_index, pixel_sample_index, 0, current_frame_index - 1);
			pixel_offset = sampler.SamplePixelOffset();
		}
		else if (render_setting.sampler_type == HALTON) {
			const uint64_t sample_index = halton_enumerator.GetIndex(pixel_index_x, pixel_index_y, (current_frame_index - 1) * render_setting.ssp + pixel_sample_index);
			sampler.InitHaltonSampler(sample_index, 2, scene.sampler_data.halton_permute_table);
			pixel_offset = sampler.SamplePixelOffset();
			pixel_offset.x = halton_enumerator.ScaleX(pixel_offset.x) - float(pixel_index_x);
			pixel_offset.y = halton_enumerator.ScaleY(pixel_offset.y) - float(pixel_index_y);
			pixel_offset = glm::clamp(pixel_offset, glm::vec2(0.000001f), glm::vec2(0.999999f));
		}
		else if (render_setting.sampler_type == SOBOL) {
			sampler.InitSobolSampler(render_setting.ssp, pixel_sample_index, current_frame_index - 1, 1, pixel_index, scene.sampler_data.sobol_matrices);
			pixel_offset = sampler.SamplePixelOffset();
		}
		else {
			pixel_offset = glm::vec2(0.5f);
		}

		Ray ray = scene.camera[RENDERING_CAMERA_INDEX].GenerateRay(float(pixel_index_x + pixel_offset.x) / float(width), float(pixel_index_y + pixel_offset.y) / float(height));

		shading_ray_data_ray[ray_index] = ray;
		shading_ray_data_received_light[ray_index] = glm::vec3(0.0f);
		shading_ray_data_throughput[ray_index] = glm::vec3(1.0f);
		shading_ray_data_ray_depth[ray_index] = 1;
		shading_ray_data_never_scatter[ray_index] = true;
		shading_ray_data_camera_ray[ray_index] = true;
		shading_ray_data_pixel_sample_index[ray_index] = pixel_sample_index;
		shading_ray_data_sampler[ray_index] = sampler;
		shading_ray_data_ray_transfer[ray_index] = scene.camera[RENDERING_CAMERA_INDEX].GetRayTransfer();
	}

	extern "C" void RayGenerationPathTracing(int current_frame_index, const RenderSetting & render_setting, Scene * scene_device, HaltonEnumerator * halton_enumerator_device, RayCounter * ray_counter_device, uint32_t next_tile_index, uint32_t tile_count_this_batch, uint32_t width, uint32_t height, uint32_t tile_count_x, uint32_t tile_count_y, ShadingRayData & shading_ray_data, cudaStream_t & stream)
	{
		uint32_t sample_per_pixel = render_setting.ssp;
		uint32_t ray_count_this_batch = tile_count_this_batch * sample_per_pixel * TILE_PIXEL_COUNT;
		if (ray_count_this_batch == 0) {
			return;
		}

		dim3 block_dim(STAGE_RAY_GEN_BLOCK_SIZE, 1, 1);
		dim3 grid_dim(Round_Block_Count(ray_count_this_batch, block_dim.x), 1, 1);

		void* args[] = { &current_frame_index,
								 &scene_device,
								 &halton_enumerator_device,
								 &ray_counter_device,
								 &next_tile_index,
								 &ray_count_this_batch,
								 &sample_per_pixel,
								 &width,
								 &height,
								 &tile_count_x,
								 &tile_count_y,
								 &shading_ray_data.shading_ray_data_ray,
								 &shading_ray_data.shading_ray_data_received_light,
								 &shading_ray_data.shading_ray_data_throughput,
								 &shading_ray_data.shading_ray_data_pixel_position_x,
								 &shading_ray_data.shading_ray_data_pixel_position_y,
								 &shading_ray_data.shading_ray_data_ray_depth,
								 &shading_ray_data.shading_ray_data_never_scatter,
								 &shading_ray_data.shading_ray_data_camera_ray,
								 &shading_ray_data.shading_ray_data_pixel_sample_index,
								 &shading_ray_data.shading_ray_data_sampler,
								 &shading_ray_data.shading_ray_data_ray_transfer };

		CUDA_CHECK(cudaLaunchKernel((void*)Ray_generation_path_tracing, grid_dim, block_dim, args, 0, stream));
		CUDA_CHECK(cudaStreamSynchronize(stream));
	}

	__global__ void Ray_trace_path_tracing(const int current_frame_index,
		const Scene * scene_device,
		RayCounter * ray_counter_device,
		const uint32_t total_ray_count_this_batch,
		const uint32_t width,
		const uint32_t height,
		glm::vec4 * noise_image,
		const Ray * shading_ray_data_ray, // note: shading ray data.
		const glm::vec3 * shading_ray_data_received_light,
		const glm::vec3 * shading_ray_data_throughput,
		const int * shading_ray_data_pixel_position_x,
		const int * shading_ray_data_pixel_position_y,
		const int * shading_ray_data_ray_depth,
		const int * shading_ray_data_never_scatter,
		const int * shading_ray_data_camera_ray,
		const int * shading_ray_data_pixel_sample_index,
		const RandomSampler * shading_ray_data_sampler,
		const RayTransfer * shading_ray_data_ray_transfer,
		HitRecord * hit_data_hit_record, // note: hit data.
		int * hit_data_nearby_hit_count,
		uint32_t * hit_data_nearby_hit_primitive_index,
		float * hit_data_nearby_t_hit,
		int * hit_data_nearby_hit_back,
		float * hit_data_volume_weights,
		Ray * hit_data_ray,
		glm::vec3 * hit_data_received_light,
		glm::vec3 * hit_data_throughput,
		int * hit_data_pixel_position_x,
		int * hit_data_pixel_position_y,
		int * hit_data_ray_depth,
		int * hit_data_never_scatter,
		int * hit_data_camera_ray,
		int * hit_data_pixel_sample_index,
		RandomSampler * hit_data_sampler,
		RayTransfer * hit_data_ray_transfer)
	{
		const uint32_t ray_index = blockIdx.x * blockDim.x + threadIdx.x;
		if (!(ray_index < total_ray_count_this_batch)) {
			return;
		}

		const uint32_t pixel_position_x = shading_ray_data_pixel_position_x[ray_index];
		const uint32_t pixel_position_y = shading_ray_data_pixel_position_y[ray_index];
		const uint32_t pixel_index = pixel_position_y * width + pixel_position_x;
		if (!(pixel_position_x < width && pixel_position_y < height)) {
			return;
		}

		const auto& scene = *scene_device;
		const auto& ray_counter = *ray_counter_device;
		const auto& render_setting = *scene.render_setting;
		const auto& image_tile_cache = *scene.image_tile_cache;

		const Ray ray = shading_ray_data_ray[ray_index];
		const RayTransfer ray_transfer = shading_ray_data_ray_transfer[ray_index];

		glm::vec3 ray_throughput = shading_ray_data_throughput[ray_index];
		glm::vec3 ray_received_light = shading_ray_data_received_light[ray_index];

		RandomSampler ray_sampler = shading_ray_data_sampler[ray_index];

		HitRecord hit_record;
		int nearby_hit_count = 0;
		NearbyHit nearby_hits[MAX_BOUNDARY_RECORD + 2];
		bool hit_something = TraceRay(scene, ray, &hit_record, &nearby_hit_count, nearby_hits);

		// note: volume weights have to transfer to next stage so should put it out.
		float volume_weights[MAX_BOUNDARY_RECORD];
		if (render_setting.enable_volume_scattering) {
			glm::vec3 tr_weight(1.0f);
			float sampled_distance = 0.0f;

			// note: current existing volume.
			int volume_indices[MAX_BOUNDARY_RECORD];
			float gs[MAX_BOUNDARY_RECORD];
			const int overlapped_volume_count = ray_transfer.GetCurrentVolumeIndices(volume_indices, scene.volume_count);
			for (int idx = 0; idx < overlapped_volume_count; ++idx) {
				gs[idx] = scene.volumes[volume_indices[idx]].g;
			}

			bool hit_volume = SampleVolumeScattering(scene, image_tile_cache, ray, overlapped_volume_count, volume_indices, hit_something ? hit_record.hit_t : TMAX,
				&tr_weight, &sampled_distance, volume_weights, ray_sampler);
			hit_something = hit_something || hit_volume;

			ray_throughput *= tr_weight;
			if (hit_volume) {
				hit_record.volume_hit = true;
				hit_record.hit_barycentric = ray.PositionAtT(sampled_distance);
			}
		}

		if (!hit_something) {
			ray_received_light += render_setting.enable_env_light ? ray_throughput * Background(ray.direction) : glm::vec3(0.0f);
			const float inv_ssp = 1.0f / glm::max(render_setting.ssp, 1);
			AtomicAddFloat(&(noise_image[pixel_index][0]), ray_received_light[0] * inv_ssp);
			AtomicAddFloat(&(noise_image[pixel_index][1]), ray_received_light[1] * inv_ssp);
			AtomicAddFloat(&(noise_image[pixel_index][2]), ray_received_light[2] * inv_ssp);
			AtomicAddFloat(&(noise_image[pixel_index][3]), inv_ssp);
			return;
		}

		// TODO: compute hit's morton code for sorting, we should classify volume hit and surface hit.
		const uint32_t hit_index = AtomicAddInt((int*)(&(ray_counter.hit_counter)), 1);

		hit_data_hit_record[hit_index] = hit_record;
		hit_data_nearby_hit_count[hit_index] = nearby_hit_count;
		const uint32_t nearby_hits_start_index = (MAX_BOUNDARY_RECORD + 2) * hit_index;

#pragma unroll (MAX_BOUNDARY_RECORD + 2)
		for (uint32_t nearby_hit_index = 0; nearby_hit_index < MAX_BOUNDARY_RECORD + 2; ++nearby_hit_index) {
			const uint32_t actual_index = nearby_hits_start_index + nearby_hit_index;
			hit_data_nearby_hit_primitive_index[actual_index] = nearby_hits[nearby_hit_index].hit_prim_idx;
			hit_data_nearby_t_hit[actual_index] = nearby_hits[nearby_hit_index].t_hit;
			hit_data_nearby_hit_back[actual_index] = nearby_hits[nearby_hit_index].hit_back;
		}

		if (hit_record.volume_hit) {
			const uint32_t volume_weights_start_index = MAX_BOUNDARY_RECORD * hit_index;
#pragma unroll MAX_BOUNDARY_RECORD
			for (uint32_t volume_weight_index = 0; volume_weight_index < MAX_BOUNDARY_RECORD; ++volume_weight_index) {
				hit_data_volume_weights[volume_weights_start_index + volume_weight_index] = volume_weights[volume_weight_index];
			}
		}

		hit_data_ray[hit_index] = ray; // note: local ray's t may already get modified.
		hit_data_received_light[hit_index] = shading_ray_data_received_light[ray_index];
		hit_data_throughput[hit_index] = ray_throughput;
		hit_data_pixel_position_x[hit_index] = pixel_position_x;
		hit_data_pixel_position_y[hit_index] = pixel_position_y;
		hit_data_ray_depth[hit_index] = shading_ray_data_ray_depth[ray_index];
		hit_data_never_scatter[hit_index] = shading_ray_data_never_scatter[ray_index];
		hit_data_camera_ray[hit_index] = shading_ray_data_camera_ray[ray_index];
		hit_data_pixel_sample_index[hit_index] = shading_ray_data_pixel_sample_index[ray_index];
		hit_data_sampler[hit_index] = ray_sampler;
		hit_data_ray_transfer[hit_index] = ray_transfer;
	}

	extern "C" void RayTracePathTracing(int current_frame_index, const RenderSetting & render_setting, Scene * scene_device, RayCounter * ray_counter_device, RayCounter * ray_counter_host, uint32_t width, uint32_t height, glm::vec4 * noise_image, ShadingRayData & shading_ray_data, HitData & hit_data, cudaStream_t & stream)
	{
		uint32_t total_ray_count_this_batch = ray_counter_host->shading_ray_counter;
		if (total_ray_count_this_batch == 0) {
			return;
		}

		dim3 block_dim(STAGE_RAY_TRACE_BLOCK_SIZE, 1, 1);
		dim3 grid_dim(Round_Block_Count(total_ray_count_this_batch, block_dim.x), 1, 1);

		void* args[] = { &current_frame_index,
								 &scene_device,
								 &ray_counter_device,
								 &total_ray_count_this_batch,
								 &width,
								 &height,
								 &noise_image,
								 &shading_ray_data.shading_ray_data_ray,
								 &shading_ray_data.shading_ray_data_received_light,
								 &shading_ray_data.shading_ray_data_throughput,
								 &shading_ray_data.shading_ray_data_pixel_position_x,
								 &shading_ray_data.shading_ray_data_pixel_position_y,
								 &shading_ray_data.shading_ray_data_ray_depth,
								 &shading_ray_data.shading_ray_data_never_scatter,
								 &shading_ray_data.shading_ray_data_camera_ray,
								 &shading_ray_data.shading_ray_data_pixel_sample_index,
								 &shading_ray_data.shading_ray_data_sampler,
								 &shading_ray_data.shading_ray_data_ray_transfer,
								 &hit_data.hit_data_hit_record,
								 &hit_data.hit_data_nearby_hit_count,
								 &hit_data.hit_data_nearby_hit_primitive_index,
								 &hit_data.hit_data_nearby_t_hit,
								 &hit_data.hit_data_nearby_hit_back,
								 &hit_data.hit_data_volume_weights,
								 &hit_data.hit_data_ray,
								 &hit_data.hit_data_received_light,
								 &hit_data.hit_data_throughput,
								 &hit_data.hit_data_pixel_position_x,
								 &hit_data.hit_data_pixel_position_y,
								 &hit_data.hit_data_ray_depth,
								 &hit_data.hit_data_never_scatter,
								 &hit_data.hit_data_camera_ray,
								 &hit_data.hit_data_pixel_sample_index,
								 &hit_data.hit_data_sampler,
								 &hit_data.hit_data_ray_transfer };

		CUDA_CHECK(cudaLaunchKernel((void*)Ray_trace_path_tracing, grid_dim, block_dim, args, 0, stream));
		CUDA_CHECK(cudaStreamSynchronize(stream));
	}

	__global__ void Ray_shading_path_tracing(const int current_frame_index,
		const Scene * scene_device,
		RayCounter * ray_counter_device,
		const uint32_t total_hit_count_this_batch,
		const uint32_t width,
		const uint32_t height,
		glm::vec4 * noise_image,
		Ray * shading_ray_data_ray, // note: shading ray data.
		glm::vec3 * shading_ray_data_received_light,
		glm::vec3 * shading_ray_data_throughput,
		int * shading_ray_data_pixel_position_x,
		int * shading_ray_data_pixel_position_y,
		int * shading_ray_data_ray_depth,
		int * shading_ray_data_never_scatter,
		int * shading_ray_data_camera_ray,
		int * shading_ray_data_pixel_sample_index,
		RandomSampler * shading_ray_data_sampler,
		RayTransfer * shading_ray_data_ray_transfer,
		const HitRecord * hit_data_hit_record, // note: hit data.
		const int * hit_data_nearby_hit_count,
		const uint32_t * hit_data_nearby_hit_primitive_index,
		const float * hit_data_nearby_t_hit,
		const int * hit_data_nearby_hit_back,
		const float * hit_data_volume_weights,
		const Ray * hit_data_ray,
		const glm::vec3 * hit_data_received_light,
		const glm::vec3 * hit_data_throughput,
		const int * hit_data_pixel_position_x,
		const int * hit_data_pixel_position_y,
		const int * hit_data_ray_depth,
		const int * hit_data_never_scatter,
		const int * hit_data_camera_ray,
		const int * hit_data_pixel_sample_index,
		const RandomSampler * hit_data_sampler,
		const RayTransfer * hit_data_ray_transfer)
	{
		const uint32_t hit_index = blockIdx.x * blockDim.x + threadIdx.x;
		if (!(hit_index < total_hit_count_this_batch)) {
			return;
		}

		const uint32_t pixel_position_x = hit_data_pixel_position_x[hit_index];
		const uint32_t pixel_position_y = hit_data_pixel_position_y[hit_index];
		const uint32_t pixel_index = pixel_position_y * width + pixel_position_x;
		assert(pixel_position_x < width&& pixel_position_y < height);

		const auto& scene = *scene_device;
		const auto& ray_counter = *ray_counter_device;
		const auto& render_setting = *scene.render_setting;
		const auto& image_tile_cache = *scene.image_tile_cache;

		Ray ray = hit_data_ray[hit_index];
		RayTransfer ray_transfer = hit_data_ray_transfer[hit_index];
		glm::vec3 ray_throughput = hit_data_throughput[hit_index];
		glm::vec3 ray_received_light = hit_data_received_light[hit_index];

		int ray_depth = hit_data_ray_depth[hit_index];
		int never_scatter = hit_data_never_scatter[hit_index];
		int camera_ray = hit_data_camera_ray[hit_index];

		RandomSampler ray_sampler = hit_data_sampler[hit_index];
		const int pixel_sample_index = hit_data_pixel_sample_index[hit_index];

		const HitRecord hit_record = hit_data_hit_record[hit_index];
		const int nearby_hit_count = hit_data_nearby_hit_count[hit_index];
		NearbyHit nearby_hits[MAX_BOUNDARY_RECORD + 2];
		const uint32_t nearby_hits_start_index = (MAX_BOUNDARY_RECORD + 2) * hit_index;

#pragma unroll (MAX_BOUNDARY_RECORD + 2)
		for (uint32_t nearby_hit_index = 0; nearby_hit_index < MAX_BOUNDARY_RECORD + 2; ++nearby_hit_index) {
			const uint32_t actual_index = nearby_hits_start_index + nearby_hit_index;
			auto& nearby_hit = nearby_hits[nearby_hit_index];
			nearby_hit.hit_prim_idx = hit_data_nearby_hit_primitive_index[actual_index];
			nearby_hit.t_hit = hit_data_nearby_t_hit[actual_index];
			nearby_hit.hit_back = hit_data_nearby_hit_back[actual_index];
		}

		const float inv_ssp = 1.0f / glm::max(render_setting.ssp, 1);
		auto write_ray_received_light = [&noise_image, &pixel_index, &inv_ssp](const glm::vec3& light) {
			AtomicAddFloat(&(noise_image[pixel_index][0]), light[0] * inv_ssp);
			AtomicAddFloat(&(noise_image[pixel_index][1]), light[1] * inv_ssp);
			AtomicAddFloat(&(noise_image[pixel_index][2]), light[2] * inv_ssp);
			AtomicAddFloat(&(noise_image[pixel_index][3]), inv_ssp);
		};
		auto generate_indirect_ray = [&](const Ray &indirect_ray, 
			const glm::vec3 &indirect_ray_received_light, 
			const glm::vec3 &indirect_ray_throughput, 
			const int indirect_pixel_pos_x,
			const int indirect_pixel_pos_y,
			const int indirect_ray_depth, 
			const int indirect_never_scatter,
			const int indirect_camera_ray,
			const int indirect_pixel_sample_index,
			const RandomSampler &indirect_ray_sampler,
			const RayTransfer &indirect_ray_transfer)
		{
			const uint32_t indirect_ray_index = AtomicAddInt((int*)(&(ray_counter.shading_ray_counter)), 1);
			shading_ray_data_ray[indirect_ray_index] = indirect_ray;
			shading_ray_data_received_light[indirect_ray_index] = indirect_ray_received_light;
			shading_ray_data_throughput[indirect_ray_index] = indirect_ray_throughput;
			shading_ray_data_pixel_position_x[indirect_ray_index] = indirect_pixel_pos_x;
			shading_ray_data_pixel_position_y[indirect_ray_index] = indirect_pixel_pos_y;
			shading_ray_data_ray_depth[indirect_ray_index] = indirect_ray_depth;
			shading_ray_data_never_scatter[indirect_ray_index] = indirect_never_scatter;
			shading_ray_data_camera_ray[indirect_ray_index] = indirect_camera_ray;
			shading_ray_data_pixel_sample_index[indirect_ray_index] = indirect_pixel_sample_index;
			shading_ray_data_sampler[indirect_ray_index] = indirect_ray_sampler;
			shading_ray_data_ray_transfer[indirect_ray_index] = indirect_ray_transfer;
		};

		const bool hit_something = hit_record.volume_hit || hit_record.hit_instance_idx != EMPTY_UINT32;
	
		if (render_setting.enable_volume_scattering && hit_record.volume_hit) {
			float volume_weights[MAX_BOUNDARY_RECORD];

			const uint32_t volume_weights_start_index = MAX_BOUNDARY_RECORD * hit_index;
#pragma unroll MAX_BOUNDARY_RECORD
			for (uint32_t volume_weight_index = 0; volume_weight_index < MAX_BOUNDARY_RECORD; ++volume_weight_index) {
				volume_weights[volume_weight_index] = hit_data_volume_weights[volume_weights_start_index + volume_weight_index];
			}
			
			int volume_indices[MAX_BOUNDARY_RECORD];
			float gs[MAX_BOUNDARY_RECORD];
			const int overlapped_volume_count = ray_transfer.GetCurrentVolumeIndices(volume_indices, scene.volume_count);
			for (int idx = 0; idx < overlapped_volume_count; ++idx) {
				gs[idx] = scene.volumes[volume_indices[idx]].g;
			}

			const glm::vec3 volume_hit_position = hit_record.hit_barycentric;
			if (render_setting.enable_distant_light) {
				ray_received_light += ray_throughput * EvalVolumeDistantLight(render_setting, scene, image_tile_cache, overlapped_volume_count, gs, volume_weights,
					ray, hit_record, ray_transfer, volume_hit_position, ray_sampler);
			}
			if (render_setting.enable_shape_light) {
				ray_received_light += ray_throughput * EvalVolumeShapeLight(render_setting, scene, image_tile_cache, overlapped_volume_count, gs, volume_weights,
					ray, hit_record, ray_transfer, volume_hit_position, ray_sampler);
			}

			glm::vec3 wi;
			float pdf = 0.0f, phase_weight = 0.0f;
			const bool continue_bounce = SampleMixedPhases(volume_weights, gs, overlapped_volume_count,
				ray_sampler.Random1D(), ray_sampler.Random1D(), -ray.direction, &phase_weight, &wi, &pdf);
			if (!continue_bounce) {
				write_ray_received_light(ray_received_light);
				return;
			}

			ray_throughput *= phase_weight;
			float rr = glm::max(ray_throughput.x, glm::max(ray_throughput.y, ray_throughput.z));
			if (ray_depth + 1 > render_setting.ray_depth) {
				if (render_setting.enable_russian_roulette && ray_sampler.Random1D() < rr) {
					ray_throughput /= glm::max(rr, MIN_COLOR_EPSILON);
				}
				else {
					write_ray_received_light(ray_received_light);
					return;
				}
			}
			
			Ray indirect_ray = Ray(volume_hit_position, wi, EMPTY_UINT32, EMPTY_UINT32);
			never_scatter = false;
			camera_ray = false;
			ray_depth += 1;

			if (ray_depth > MAX_RAY_DEPTH) {
				write_ray_received_light(ray_received_light);
				return;
			}

			generate_indirect_ray(indirect_ray, ray_received_light, ray_throughput, pixel_position_x, pixel_position_y, ray_depth, never_scatter, camera_ray, pixel_sample_index, ray_sampler, ray_transfer);
			return;
		}

		glm::vec3 hit_position(0.0f), hit_position_error(0.0f), hit_position_object_space(0.0f);
		glm::vec3	hit_shading_normal(0.0f), hit_geometry_normal(0.0f);
		glm::vec2	hit_uv(0.0f);
		glm::vec3	hit_dpdu(0.0f), hit_dpdv(0.0f);
		FetchShadingData(scene, hit_record,
			&hit_position, &hit_position_error, &hit_position_object_space,
			&hit_shading_normal, &hit_geometry_normal,
			&hit_uv,
			&hit_dpdu, &hit_dpdv);

		const auto& primitive = scene.primitive_instances[hit_record.hit_instance_idx];
		const auto material_index = scene.primitive_instances[hit_record.hit_instance_idx].material_idx;
		if (!(material_index < scene.material_count)) {
			write_ray_received_light(ray_received_light);
			return;
		}

		const auto& material = scene.materials[material_index];
		uint32_t material_ior_priority;
		const float material_ior = material.FetchIOR(&material_ior_priority);

		// note: skip hit with lower priority;
		if (!ray_transfer.empty() && material_ior_priority < ray_transfer.GetMaxPriorityRecord().ior_priority) {
			// note: should not consume depth
			BoundaryTransitionBunch(ray_transfer, scene.primitive_instances, scene.materials, nearby_hits, nearby_hit_count);
			Ray indirect_ray = Ray(OffsetRayOrigin(hit_position, hit_position_error, ray.direction, hit_geometry_normal), ray.direction, hit_record.hit_instance_idx, hit_record.hit_triangle_idx);
			camera_ray = false;

			generate_indirect_ray(indirect_ray, ray_received_light, ray_throughput, pixel_position_x, pixel_position_y, ray_depth, never_scatter, camera_ray, pixel_sample_index, ray_sampler, ray_transfer);
			return;
		}
		 
		// note: skip volume boundary.
		if (primitive.treat_as_boundary) {
			// note: should not consume depth
			// note: although volume is treat as boundary, but the material's ior will still affect its attribute, so remember to set it!
			BoundaryTransitionBunch(ray_transfer, scene.primitive_instances, scene.materials, nearby_hits, nearby_hit_count);
			Ray indirect_ray = Ray(OffsetRayOrigin(hit_position, hit_position_error, ray.direction, hit_geometry_normal), ray.direction, hit_record.hit_instance_idx, hit_record.hit_triangle_idx);
			camera_ray = false;

			generate_indirect_ray(indirect_ray, ray_received_light, ray_throughput, pixel_position_x, pixel_position_y, ray_depth, never_scatter, camera_ray, pixel_sample_index, ray_sampler, ray_transfer);
			return;
		}

		const glm::vec4 tex_coordinates_differentials =
			scene.camera->TextureCoordinatesDifferential(ray,
				hit_position,
				hit_shading_normal,
				hit_geometry_normal,
				hit_dpdu, hit_dpdv,
				width, height,
				0.25f, 0.25f);

		if (material.material_type == LIGHT_MTL) {
			if (never_scatter) {
				ray_received_light += ray_throughput * material.light_mtl.light_color * material.light_mtl.intensity;
			}
			write_ray_received_light(ray_received_light);
			return;
		}

		MaterialBSDF material_bsdf;
		const TextureCoordinate texture_coordinate(hit_uv, hit_position, hit_position_object_space, tex_coordinates_differentials);

		// note: eval normal mapping or bump mapping, the hit_shading_normal, and dpdu and dpdv will get modified here
		material_bsdf.InitShadingSpace(material,
			hit_record.hit_back,
			ray,
			hit_geometry_normal,
			hit_shading_normal,
			hit_dpdu,
			hit_dpdv,
			scene.transforms[primitive.transform_idx],
			scene.i_transforms[primitive.transform_idx],
			scene.textures,
			image_tile_cache,
			texture_coordinate);

		// note: instance's internal ior is implicitly specified by its material
		material_bsdf.InitBSDFSettings(material,
			scene.textures,
			image_tile_cache,
			texture_coordinate,
			ray,
			hit_record.hit_back,
			ray_transfer.GetRayIOR(),
			ray_transfer.GetExIOR(hit_record.hit_instance_idx));
		

		if (render_setting.enable_distant_light) {
			ray_received_light += ray_throughput *
				EvalDistantLight(render_setting,
					scene,
					image_tile_cache,
					material_bsdf,
					ray.direction,
					hit_record,
					ray_transfer,
					nearby_hit_count,
					nearby_hits,
					hit_position,
					hit_position_error,
					material_bsdf.GetShadingNormal(),
					hit_geometry_normal,
					ray_sampler);
		}

		// TODO: sample table, light BVH...
		if (render_setting.enable_shape_light) {
			ray_received_light += ray_throughput *
				EvalShapeLight(render_setting,
					scene,
					image_tile_cache,
					material_bsdf,
					ray.direction,
					hit_record,
					ray_transfer,
					nearby_hit_count,
					nearby_hits,
					hit_position,
					hit_position_error,
					material_bsdf.GetShadingNormal(),
					hit_geometry_normal,
					ray_sampler);
		}

		// note: indirect.
		glm::vec3 wo = material_bsdf.WorldToShading(-ray.direction), wi(0.0f), bsdf_weight(0.0f);
		float pdf = 0.0f;

		const bool sample_valid = material_bsdf.SampleWi(wo, &bsdf_weight, &wi, &pdf, ray_sampler.Random1D(), ray_sampler.Random1D(), ray_sampler.Random1D());
		assert(!glm::isnan(bsdf_weight.x) && !glm::isnan(bsdf_weight.y) && !glm::isnan(bsdf_weight.z));
		if (!sample_valid) {
			write_ray_received_light(ray_received_light);
			return;
		}

		ray_throughput *= bsdf_weight;
		float rr = glm::max(ray_throughput.x, glm::max(ray_throughput.y, ray_throughput.z));
		if (ray_depth + 1 > render_setting.ray_depth) {
			if (render_setting.enable_russian_roulette && ray_sampler.Random1D() < rr) {
				ray_throughput /= glm::max(rr, MIN_COLOR_EPSILON);
			}
			else {
				write_ray_received_light(ray_received_light);
				return;
			}
		}

		const glm::vec3 new_direction = material_bsdf.ShadingToWorld(wi);
		const bool transmit_boundary = glm::dot(-ray.direction, hit_geometry_normal) * glm::dot(new_direction, hit_geometry_normal) < 0.0f;
		const bool sample_invalid = (!SameHemisphere(wo, wi) && !transmit_boundary) || (SameHemisphere(wo, wi) && transmit_boundary);
		if (sample_invalid) {
			write_ray_received_light(ray_received_light);
			return;
		}
		if (transmit_boundary) {
			BoundaryTransitionBunch(ray_transfer, scene.primitive_instances, scene.materials, nearby_hits, nearby_hit_count);
		}

		const bool front_side_bounce = glm::dot(hit_geometry_normal, new_direction) >= 0.0f;
		Ray indirect_ray = Ray(OffsetRayOrigin(hit_position, hit_position_error, new_direction, hit_geometry_normal), new_direction, hit_record.hit_instance_idx, hit_record.hit_triangle_idx);
		never_scatter = false;
		camera_ray = false;
		ray_depth += 1;

		if (ray_depth > MAX_RAY_DEPTH) {
			write_ray_received_light(ray_received_light);
			return;
		}

		generate_indirect_ray(indirect_ray, ray_received_light, ray_throughput, pixel_position_x, pixel_position_y, ray_depth, never_scatter, camera_ray, pixel_sample_index, ray_sampler, ray_transfer);
		return;
}

	extern "C" void RayShadingPathTracing(int current_frame_index, const RenderSetting & render_setting, Scene * scene_device, RayCounter * ray_counter_device, RayCounter * ray_counter_host, uint32_t width, uint32_t height, glm::vec4 * noise_image, ShadingRayData & shading_ray_data, HitData & hit_data, ShadowRayData & shadow_ray_data, cudaStream_t & stream)
	{
		uint32_t total_hit_count_this_batch = ray_counter_host->hit_counter;
		if (total_hit_count_this_batch == 0) {
			return;
		}

		dim3 block_dim(STAGE_RAY_SHADING_BLOCK_SIZE, 1, 1);
		dim3 grid_dim(Round_Block_Count(total_hit_count_this_batch, block_dim.x), 1, 1);

		void* args[] = { &current_frame_index,
								 &scene_device,
								 &ray_counter_device,
								 &total_hit_count_this_batch,
								 &width,
								 &height,
								 &noise_image,
								 &shading_ray_data.shading_ray_data_ray,
								 &shading_ray_data.shading_ray_data_received_light,
								 &shading_ray_data.shading_ray_data_throughput,
								 &shading_ray_data.shading_ray_data_pixel_position_x,
								 &shading_ray_data.shading_ray_data_pixel_position_y,
								 &shading_ray_data.shading_ray_data_ray_depth,
								 &shading_ray_data.shading_ray_data_never_scatter,
								 &shading_ray_data.shading_ray_data_camera_ray,
								 &shading_ray_data.shading_ray_data_pixel_sample_index,
								 &shading_ray_data.shading_ray_data_sampler,
								 &shading_ray_data.shading_ray_data_ray_transfer,
								 &hit_data.hit_data_hit_record,
								 &hit_data.hit_data_nearby_hit_count,
								 &hit_data.hit_data_nearby_hit_primitive_index,
								 &hit_data.hit_data_nearby_t_hit,
								 &hit_data.hit_data_nearby_hit_back,
								 &hit_data.hit_data_volume_weights,
								 &hit_data.hit_data_ray,
								 &hit_data.hit_data_received_light,
								 &hit_data.hit_data_throughput,
								 &hit_data.hit_data_pixel_position_x,
								 &hit_data.hit_data_pixel_position_y,
								 &hit_data.hit_data_ray_depth,
								 &hit_data.hit_data_never_scatter,
								 &hit_data.hit_data_camera_ray,
								 &hit_data.hit_data_pixel_sample_index,
								 &hit_data.hit_data_sampler,
								 &hit_data.hit_data_ray_transfer };

		CUDA_CHECK(cudaLaunchKernel((void*)Ray_shading_path_tracing, grid_dim, block_dim, args, 0, stream));
		CUDA_CHECK(cudaStreamSynchronize(stream));
	}

	__global__ void Accumulate_image_path_tracing(const glm::vec4 * noise_image,
		glm::vec4 * accumulated_image,
		uint32_t width,
		uint32_t height,
		int frame_index)
	{
		uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
		uint32_t idy = blockIdx.y * blockDim.y + threadIdx.y;
		if (!(idx < width && idy < height)) {
			return;
		}
		
		uint32_t pixel_index = idy * width + idx;
		if (frame_index == 1) {
			accumulated_image[pixel_index] = noise_image[pixel_index];
		}
		else {
			accumulated_image[pixel_index] = (accumulated_image[pixel_index] * float(frame_index - 1) + noise_image[pixel_index]) / float(frame_index);
		}
	}

	extern "C" void AccumulateImagePathTracing(const glm::vec4 * noise_image, glm::vec4 * accumulated_image, uint32_t width, uint32_t height, int frame_index, const int max_accumulate_frame, cudaStream_t & stream)
	{
		assert(noise_image != nullptr && accumulated_image != nullptr);
		if (max_accumulate_frame > 0 && frame_index > max_accumulate_frame) {
			return; 
		}

		dim3 block_dim(32, 32, 1);
		dim3 grid_dim(Round_Block_Count(width, block_dim.x), Round_Block_Count(height, block_dim.y), 1);

		void* args[] = { &noise_image,
								 &accumulated_image,
								 &width,
								 &height,
								 &frame_index };
		CUDA_CHECK(cudaLaunchKernel((void*)Accumulate_image_path_tracing, grid_dim, block_dim, args, 0, stream));
		CUDA_CHECK(cudaStreamSynchronize(stream));
	}

	
	__global__ void Gamma_correction_common(glm::vec4 * image,
		const float gamma,
		const float exposure,
		const uint32_t width,
		const uint32_t height)
	{
		int idx = blockIdx.x * blockDim.x + threadIdx.x;
		int idy = blockIdx.y * blockDim.y + threadIdx.y;
		if (!(idx < width && idy < height)) {
			return;
		}

		auto gamma_correction = [&gamma](const glm::vec3& col) {
			return glm::pow(col, glm::vec3(1.0f / gamma));
		};

		auto aces_tonemapping = [&](glm::vec3 col, float exposure) {
			const float a = 2.51f;
			const float b = 0.03f;
			const float c = 2.43f;
			const float d = 0.59f;
			const float e = 0.14f;
			col *= exposure;
			return (col * (a * col + b)) / (col * (c * col + d) + e);
		};

		uint32_t pixel_index = idy * width + idx;
		glm::vec3 result_col = aces_tonemapping(glm::vec3(image[pixel_index]), exposure);
		image[pixel_index] = glm::vec4(gamma_correction(result_col), 1.0f);
	}

	__global__ void Draw_outline(glm::vec4 * image,
		const uint32_t * primitive_index,
		const uint32_t width,
		const uint32_t height,
		const uint32_t clicked_primitive_index)
	{
		int idx = blockIdx.x * blockDim.x + threadIdx.x;
		int idy = blockIdx.y * blockDim.y + threadIdx.y;
		if (!(idx < width && idy < height)) {
			return;
		}

		auto in_range = [](int x, int y, int width, int height) {
			return x >= 0 && x < width && y >= 0 && y < height;
		};
		auto get_pixel_index = [](int x, int y, int width, int height) {
			return y * width + x;
		};

		int highlight = 0, total_sample = 0;
		for (int x = -1; x <= 1; ++x) {
			for (int y = -1; y <= 1; ++y) {
				int sample_x = idx + x, sample_y = idy + y;
				if (in_range(sample_x, sample_y, width, height))
				{
					++total_sample;
					if (primitive_index[get_pixel_index(sample_x, sample_y, width, height)] == clicked_primitive_index) {
						++highlight;
					}
				}
			}
		}

		const uint32_t pixel_index = get_pixel_index(idx, idy, width, height);
		const glm::vec4 dim_color = image[pixel_index];

		glm::vec3 result_col(0.0f);
		if (highlight == 0) {
			result_col = glm::vec3(dim_color);
		}
		else if (highlight == total_sample) {
			result_col = glm::mix(glm::vec3(dim_color), glm::vec3(0.9f, 0.2f, 0.2f), 0.6f);
		}
		else {
			result_col = glm::vec3(1.0f, 0.0f, 0.0f);
		}
		image[pixel_index] = glm::vec4(result_col, 1.0f);
	}

	extern "C" void PostProcessingEditorView(glm::vec4 * image, uint32_t * primitive_index, uint32_t width,  uint32_t height, uint32_t clicked_primitive_index, float gamma, float exposure, bool draw_selected_effect, cudaStream_t & stream)
	{
		assert(image != nullptr && primitive_index != nullptr);

		dim3 block_dim(32, 32, 1);
		dim3 grid_dim(Round_Block_Count(width, block_dim.x), Round_Block_Count(height, block_dim.y), 1);
			
		void* gamma_args[] = { &image,
											&gamma,
											&exposure,
											&width,
											&height };
		CUDA_CHECK(cudaLaunchKernel((void*)Gamma_correction_common, grid_dim, block_dim, gamma_args, 0, stream));

		const bool draw_outline = draw_selected_effect && (clicked_primitive_index != EMPTY_UINT32);
		if (draw_outline) {
			void* outline_args[] = { &image,
												&primitive_index,
												&width,
												&height, 
												&clicked_primitive_index };
			CUDA_CHECK(cudaLaunchKernel((void*)Draw_outline, grid_dim, block_dim, outline_args, 0, stream));
		}
		CUDA_CHECK(cudaStreamSynchronize(stream));
	}

	extern "C" void PostProcessingPathTracing(glm::vec4 * image, uint32_t width, uint32_t height, float gamma, float exposure, cudaStream_t & stream)
	{
		assert(image != nullptr);

		dim3 block_dim(32, 32, 1);
		dim3 grid_dim(Round_Block_Count(width, block_dim.x), Round_Block_Count(height, block_dim.y), 1);

		void* args[] = { &image,
								 &gamma,
								 &exposure,
								 &width,
								 &height };
		CUDA_CHECK(cudaLaunchKernel((void*)Gamma_correction_common, grid_dim, block_dim, args, 0, stream));
		CUDA_CHECK(cudaStreamSynchronize(stream));
	}
};