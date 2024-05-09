#include <chrono>

#include <cuda_runtime.h>
#include <cuda_profiler_api.h>

#include <vector_types.h>
#include <driver_types.h>

#include <device_launch_parameters.h>

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

#define MAX_RAY_DEPTH 32

#define TILE_X_RES 16u
#define TILE_Y_RES 16u
#define TILE_PIXEL_COUNT 256u
#define MAX_RAY_COUNT 1024 * 1024

	__device__ __host__ inline glm::vec3 Background(const glm::vec3 &direction)
	{
		return glm::mix(glm::vec3(1.0f), glm::vec3(0.05f, 0.2f, 0.85f), direction.y * 0.5f + 0.5f);
	}

	__device__ __host__ inline float PowerHeuristic(float main_pdf, float vice_pdf)
	{
		return SafeRcp(1.0f + Sqr(vice_pdf * SafeRcp(main_pdf)));
	}



	__device__ glm::vec3 EvalDistantLight(const RenderSetting &render_setting,
																const Scene &scene, 
																const ImageTileCache& image_tile_cache,
																MaterialBSDF &material_bsdf,
																const glm::vec3 &ray_direction, 
																const PrimitiveInstance& hit_prim,
																const glm::vec3 &hit_position, 
																const glm::vec3 &hit_shading_normal, 
																const glm::vec3 &hit_geometry_normal,
		RandomSampler &sampler)
	{
		assert(scene.distant_lights != nullptr);
		if (scene.distant_lights == nullptr || scene.distant_light_count == 0) {return glm::vec3(0.0f);}
		auto random = [&]()->float {return sampler.Random1D(); };
		
		glm::vec3 direct_lighting(0.0f);
		const glm::vec3 wo = material_bsdf.WorldToShading(-ray_direction);

		float distant_light_select_pdf = 1.0f / float(scene.distant_light_count);
		int selected_distant_light_idx = glm::max(glm::min((int)(random() * scene.distant_light_count), (int)scene.distant_light_count - 1), 0);

		// sample from light
		{
			glm::vec3 light_dir, light_pos;
			float light_sample_pdf;
			glm::vec3 Li = scene.distant_lights[selected_distant_light_idx].SampleLi(scene, hit_position, random(), random(), &light_dir, &light_pos, &light_sample_pdf);

			glm::vec3 light_wi = material_bsdf.WorldToShading(light_dir);
			float light_wi_pdf = 0.0f;
			const glm::vec3 bsdf_weight = material_bsdf.EvalWi(wo, light_wi, &light_wi_pdf);

			if (bsdf_weight.x + bsdf_weight.y + bsdf_weight.z > 1E-16f)
			{
				bool front_side_light = glm::dot(light_dir, hit_geometry_normal) >= 0.0f;
				const glm::vec3 shadow_ray_origin = OffsetRayOrigin(hit_position,  front_side_light ? hit_geometry_normal : -hit_geometry_normal);
				Ray shadow_ray(shadow_ray_origin, light_dir);

				HitRecord shadow_ray_record;
				if (!BVHTraverseShadow(scene, shadow_ray, &shadow_ray_record)) 
				{
					glm::vec3 tr(1.0f);
					if (render_setting.enable_volume_scattering)
					{
						// don't need ior, set it arbitrary value.
						shadow_ray = Ray(shadow_ray_origin, light_dir, 1.0f,
													  front_side_light ? hit_prim.outer_volume_idx : hit_prim.inner_volume_idx);
						shadow_ray.t = TMAX;
						tr = TraceTr(scene, image_tile_cache, shadow_ray, sampler);
					}

					float mis_weight = PowerHeuristic(light_sample_pdf, light_wi_pdf);
					direct_lighting += mis_weight * tr * glm::min(bsdf_weight, glm::vec3(1E16f)) * glm::min(Li, glm::vec3(1E16f));
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
				glm::vec3 Li = scene.distant_lights[selected_distant_light_idx].EvalLi(scene, light_dir, &bsdf_wi_pdf);

				if (Li.x + Li.y + Li.z > 1E-16f)
				{
					bool front_side_light = glm::dot(light_dir, hit_geometry_normal) >= 0.0f;
					const glm::vec3 shadow_ray_origin = OffsetRayOrigin(hit_position, front_side_light ? hit_geometry_normal : -hit_geometry_normal);
					Ray shadow_ray(shadow_ray_origin, light_dir);

					HitRecord shadow_ray_record;
					if (!BVHTraverseShadow(scene, shadow_ray, &shadow_ray_record))
					{
						glm::vec3 tr(1.0f);
						if (render_setting.enable_volume_scattering)
						{
							// don't need ior, set it arbitrary value.
							shadow_ray = Ray(shadow_ray_origin, light_dir, 1.0f,
														  front_side_light ? hit_prim.outer_volume_idx : hit_prim.inner_volume_idx);
							shadow_ray.t = TMAX;
							tr = TraceTr(scene, image_tile_cache, shadow_ray, sampler);
						}

						float mis_weight = PowerHeuristic(bsdf_sample_pdf, bsdf_wi_pdf);
						direct_lighting += mis_weight * tr * glm::min(bsdf_weight, glm::vec3(1E16f)) * glm::min(Li, glm::vec3(1E16f));
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
																			 const Volume &volume,
																			 const Ray &ray,
																			 const glm::vec3 &volume_hit_position,
		RandomSampler &sampler)
	{
		if (scene.distant_lights == nullptr) { return glm::vec3(0.0f); }
		auto random = [&]()->float {return sampler.Random1D(); };

		glm::vec3 direct_lighting(0.0f);

		// sample from light
		{
			glm::vec3 light_dir, light_pos;
			float light_sample_pdf;
			glm::vec3 Li = scene.distant_lights[0].SampleLi(scene, volume_hit_position, random(), random(), &light_dir, &light_pos, &light_sample_pdf);

			glm::vec3 light_wi = light_dir;
			float light_wi_pdf = 0.0f;
			const float phase_weight = volume.EvalPhase(-ray.direction, light_wi, &light_wi_pdf);

			if (phase_weight > 1E16f)
			{
				Ray shadow_ray(volume_hit_position, light_dir);
				shadow_ray.t = TMAX;

				HitRecord shadow_ray_record;
				if (!BVHTraverseShadow(scene, shadow_ray, &shadow_ray_record))
				{
					// don't need ior, set it arbitrary value.
					shadow_ray = Ray(volume_hit_position, light_dir, 1.0f, ray.volume_idx);
					shadow_ray.t = TMAX;
					glm::vec3 tr = TraceTr(scene, image_tile_cache, shadow_ray, sampler);

					float mis_weight = PowerHeuristic(light_sample_pdf, light_wi_pdf);
					direct_lighting += mis_weight * tr * glm::min(phase_weight, 1E16f) * glm::min(Li, glm::vec3(1E16f));
				}
			}
		}

		// sample from volume
		{
			glm::vec3 phase_wi;
			float phase_sample_pdf = 0.0f, phase_weight = 0.0f;
			bool sample_valid = volume.SamplePhase(random(), random(), -ray.direction, &phase_weight, &phase_wi, &phase_sample_pdf);

			if (sample_valid)
			{
				const glm::vec3 light_dir = phase_wi;
				float phase_wi_pdf = 0.0f;
				glm::vec3 Li = scene.distant_lights[0].EvalLi(scene, light_dir, &phase_wi_pdf);

				if (Li.x + Li.y + Li.z > 1E-16f)
				{
					Ray shadow_ray(volume_hit_position, light_dir);
					shadow_ray.t = TMAX;

					HitRecord shadow_ray_record;
					if (!BVHTraverseShadow(scene, shadow_ray, &shadow_ray_record))
					{
						// don't need ior, set it arbitrary value.
						shadow_ray = Ray(volume_hit_position, light_dir, 1.0f, ray.volume_idx);
						shadow_ray.t = TMAX;
						glm::vec3 tr = TraceTr(scene, image_tile_cache, shadow_ray, sampler);

						float mis_weight = PowerHeuristic(phase_sample_pdf, phase_wi_pdf);
						direct_lighting += mis_weight * tr * glm::min(phase_weight, 1E16f) * glm::min(Li, glm::vec3(1E16f));
					}
				}
			}
		}

		return direct_lighting;
	}


	__device__ glm::vec3 EvalShapeLight(const RenderSetting &render_setting,
																const Scene &scene,
																const ImageTileCache& image_tile_cache,
																MaterialBSDF &material_bsdf,
																const glm::vec3 &ray_direction,
																const PrimitiveInstance &hit_prim,
																const glm::vec3 &hit_position,
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
			glm::vec3 light_dir, light_pos, light_geo_normal;
			float light_sample_pdf;
			glm::vec3 Li = shape_light.SampleLi(scene, hit_position, random(), random(), &light_dir, &light_pos, &light_geo_normal, &light_sample_pdf);

			glm::vec3 light_wi = material_bsdf.WorldToShading(light_dir);
			float light_wi_pdf = 0.0f;
			const glm::vec3 bsdf_weight = material_bsdf.EvalWi(wo, light_wi, &light_wi_pdf);

			if (bsdf_weight.x + bsdf_weight.y + bsdf_weight.z > 1E-16f)
			{
				bool front_side_light = glm::dot(light_dir, hit_geometry_normal) >= 0.0f;
				const glm::vec3 shadow_ray_origin = OffsetRayOrigin(hit_position, front_side_light ? hit_geometry_normal : -hit_geometry_normal);

				// only for test occlusion, so we don't need ior and volume
				Ray shadow_ray(shadow_ray_origin, light_dir);
				float max_trace_distance = glm::length(OffsetRayOrigin(light_pos, light_geo_normal) - hit_position) * 0.9996f;
				
				shadow_ray.t = max_trace_distance;
				HitRecord shadow_ray_record;
				if (!BVHTraverseShadow(scene, shadow_ray, &shadow_ray_record))
				{
					glm::vec3 tr(1.0f);
					if (render_setting.enable_volume_scattering) 
					{
						// don't need ior, set it arbitrary value.
						shadow_ray = Ray(shadow_ray_origin, light_dir, 1.0f, 
													  front_side_light ? hit_prim.outer_volume_idx : hit_prim.inner_volume_idx);
						shadow_ray.t = max_trace_distance;
						tr = TraceTr(scene, image_tile_cache, shadow_ray, sampler);
					}
					
					float mis_weight = PowerHeuristic(light_sample_pdf, light_wi_pdf);
					direct_lighting += mis_weight * tr * glm::min(bsdf_weight, glm::vec3(1E16f)) * glm::min(Li, glm::vec3(1E16f));
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
				glm::vec3 light_pos, light_geo_normal;
				float bsdf_wi_pdf = 0.0f;
				glm::vec3 Li = shape_light.EvalLi(scene, hit_position, light_dir, &light_pos, &light_geo_normal, &bsdf_wi_pdf);

				if (Li.x + Li.y + Li.z > 1E-16f)
				{
					bool front_side_light = glm::dot(light_dir, hit_geometry_normal) >= 0.0f;
					glm::vec3 shadow_ray_origin = OffsetRayOrigin(hit_position,  front_side_light? hit_geometry_normal : -hit_geometry_normal);
					
					// only for test occlusion, so we don't need ior and volume
					Ray shadow_ray(shadow_ray_origin, light_dir);
					float max_trace_distance = glm::length(OffsetRayOrigin(light_pos, light_geo_normal) - hit_position) * 0.9996f;

					shadow_ray.t = max_trace_distance;
					HitRecord shadow_ray_record;
					if (!BVHTraverseShadow(scene, shadow_ray, &shadow_ray_record))
					{
						glm::vec3 tr(1.0f);
						if (render_setting.enable_volume_scattering)
						{
							// don't need ior, set it arbitrary value.
							shadow_ray = Ray(shadow_ray_origin, light_dir, 1.0f, 
														  front_side_light ? hit_prim.outer_volume_idx : hit_prim.inner_volume_idx);
							shadow_ray.t = max_trace_distance;
							tr = TraceTr(scene, image_tile_cache, shadow_ray, sampler);
						}
						
						float mis_weight = PowerHeuristic(bsdf_sample_pdf, bsdf_wi_pdf);
						direct_lighting += mis_weight * tr * glm::min(bsdf_weight, glm::vec3(1E16f)) * glm::min(Li, glm::vec3(1E16f));
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
																			const Volume &volume,
																			const Ray &ray,
																			const glm::vec3 &volume_hit_position, 
		RandomSampler &sampler)
	{
		if (scene.shape_light_count == 0 || scene.shape_lights == nullptr) { return glm::vec3(0.0f); }
		auto random = [&]()->float {return sampler.Random1D(); };

		glm::vec3 direct_lighting(0.0f);

		float light_select_pdf = 0.0f;
		int light_idx = SampleLightPower(scene, random(), &light_select_pdf);

		const ShapeLight &shape_light = scene.shape_lights[light_idx];

		// sample from light
		{
			glm::vec3 light_dir, light_pos, light_geo_normal;
			float light_sample_pdf;
			glm::vec3 Li = shape_light.SampleLi(scene, volume_hit_position, random(), random(), &light_dir, &light_pos, &light_geo_normal, &light_sample_pdf);

			float light_wi_pdf = 0.0f;
			float phase_weight = volume.EvalPhase(-ray.direction ,light_dir, &light_wi_pdf);

			if (phase_weight > 1E-16f)
			{
				Ray shadow_ray(volume_hit_position, light_dir);
				float max_trace_distance = glm::length(OffsetRayOrigin(light_pos, light_geo_normal) - volume_hit_position) * 0.9996f;

				shadow_ray.t = max_trace_distance;
				HitRecord shadow_ray_record;
				if (!BVHTraverseShadow(scene, shadow_ray, &shadow_ray_record))
				{
					// don't need ior, set it arbitrary value.
					shadow_ray = Ray(volume_hit_position,
												  light_dir,
												  1.0f,
												  ray.volume_idx);
					shadow_ray.t = max_trace_distance;
					glm::vec3 tr = TraceTr(scene, image_tile_cache, shadow_ray, sampler);
					
					float mis_weight = PowerHeuristic(light_sample_pdf, light_wi_pdf);
					direct_lighting += mis_weight * tr * glm::min(phase_weight, 1E16f) * glm::min(Li, glm::vec3(1E16f));
				}
			}
		}

		// sample from phase
		{
			glm::vec3 phase_wi;
			float phase_sample_pdf = 0.0f, phase_weight = 0.0f;
			bool sample_valid = volume.SamplePhase(random(), random(), -ray.direction, &phase_weight, &phase_wi, &phase_sample_pdf);

			if (sample_valid)
			{
				glm::vec3 light_dir = phase_wi;
				glm::vec3 light_pos, light_geo_normal;
				float phase_wi_pdf = 0.0f;
				glm::vec3 Li = shape_light.EvalLi(scene, volume_hit_position, light_dir, &light_pos, &light_geo_normal, &phase_wi_pdf);

				if (Li.x + Li.y + Li.z > 1E-16f)
				{
					// only for test occlusion, so we don't need ior and volume
					Ray shadow_ray(volume_hit_position, light_dir);
					float max_trace_distance = glm::length(OffsetRayOrigin(light_pos, light_geo_normal) - volume_hit_position) * 0.9996f;

					shadow_ray.t = max_trace_distance;
					HitRecord shadow_ray_record;
					if (!BVHTraverseShadow(scene, shadow_ray, &shadow_ray_record))
					{
						// don't need ior, set it arbitrary value.
						shadow_ray = Ray(volume_hit_position,
													  light_dir,
													  1.0f,
													  ray.volume_idx);
						shadow_ray.t = max_trace_distance;
						glm::vec3 tr = TraceTr(scene, image_tile_cache, shadow_ray, sampler);
						
						float mis_weight = PowerHeuristic(phase_sample_pdf, phase_wi_pdf);
						direct_lighting += mis_weight * tr * glm::min(phase_weight, 1E16f) * glm::min(Li, glm::vec3(1E16f));
					}
				}
			}
		}

		return light_select_pdf == 0.0f ? glm::vec3(0.0f) : (direct_lighting / light_select_pdf);
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
		if (!(idx < total_pixel_count)) { return; }

		uint32_t tile_id = idx / TILE_PIXEL_COUNT;
		uint32_t tile_x = tile_id % tile_count_x;
		uint32_t tile_y = tile_id / tile_count_x;

		uint32_t tile_pixel_id = idx & (TILE_PIXEL_COUNT - 1);
		uint32_t tile_pixel_x = tile_pixel_id & (TILE_X_RES - 1);
		uint32_t tile_pixel_y = tile_pixel_id / TILE_X_RES;

		uint32_t px = tile_x * TILE_X_RES + tile_pixel_x;
		uint32_t py = tile_y * TILE_Y_RES + tile_pixel_y;
		if (!(px < width && py < height)) { return; }
		uint32_t pixel_idx = py * width + px;
		
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
			RayTransfer ray_transfer;
			assert(ray_transfer.record_count == 0);

			glm::vec3 L(0.0f), throughput(1.0f);
			bool never_scatter = true;
			for (int depth = 1;;)
			{
				if (depth > MAX_RAY_DEPTH) { break; }
				
				HitRecord hit_record;
				bool hit_surface = BVHTraverse(scene, ray, &hit_record);

				if (sample_idx == 0 && depth == 1) {
					prim_idx_buffer[pixel_idx] = hit_surface ? hit_record.hit_instance_idx : EMPTY_UINT32;
				}

				// volume scatter
				glm::vec3 tr_weight(1.0f);
				float sampled_distance = 0.0f;
				if (render_setting.enable_volume_scattering && 
					SampleVolumeScattering(scene, image_tile_cache, ray, hit_surface? hit_record.hit_t : TMAX, &tr_weight, &sampled_distance, sampler))
				{
					const Volume &vol = scene.volumes[ray.volume_idx];

					throughput *= tr_weight;
					glm::vec3 volume_hit_position = ray.PositionAtT(sampled_distance);

					if (render_setting.enable_distant_light)
					{
						L += throughput * EvalVolumeDistantLight(render_setting, scene, image_tile_cache, vol, ray, volume_hit_position, sampler);
					}

					L += throughput * EvalVolumeShapeLight(render_setting, scene, image_tile_cache, vol, ray, volume_hit_position, sampler);
					
					// sample phase function
					// multiply phase weight
					glm::vec3 wi;
					float pdf = 0.0f, phase_weight = 0.0f;
					bool continue_bounce = vol.SamplePhase(sampler.Random1D(), sampler.Random1D(), -ray.direction, &phase_weight, &wi, &pdf);
					if (!continue_bounce) { break; }

					throughput *= phase_weight;
					float rr = glm::max(throughput.x, glm::max(throughput.y, throughput.z));
					if (depth + 1 > render_setting.ray_depth)
					{
						if (render_setting.enable_russian_roulette && sampler.Random1D() < rr) { throughput /= glm::max(rr, 1E-20f); }
						else { break; }
					}

					ray = Ray(volume_hit_position, wi, ray.ray_ior, ray.volume_idx);
					never_scatter = false;
					++depth;
					continue;
				}
				else
				{
					throughput *= tr_weight;
				}
				

				if (hit_surface)
				{
					glm::vec3 hit_position;
					glm::vec3 hit_position_object_space;
					glm::vec3	hit_shading_normal;
					glm::vec3	hit_geometry_normal;
					glm::vec2	hit_uv;
					glm::vec3	hit_dpdu;
					glm::vec3	hit_dpdv;
					FetchShadingData(scene,
												hit_record,
												&hit_position,
												&hit_position_object_space,
												&hit_shading_normal,
												&hit_geometry_normal,
												&hit_uv,
												&hit_dpdu,
												&hit_dpdv);

					const PrimitiveInstance &prim = scene.prim_instances[hit_record.hit_instance_idx];
					const Material &mtl = scene.materials[prim.material_idx];
					
					float mtl_ior; 
					uint32_t mtl_ior_priority;
					mtl.FetchIOR(&mtl_ior, &mtl_ior_priority);
					if (!ray_transfer.empty() && mtl_ior_priority < ray_transfer.GetMaxPriorityRecord().ior_priority)
					{
						bool bounce_outside = glm::dot(hit_geometry_normal, ray.direction) > 0.0f;
						if (bounce_outside) {
							ray_transfer.PopRecord(hit_record.hit_instance_idx);
						}
						else {
							ray_transfer.PushRecord(hit_record.hit_instance_idx, mtl_ior, mtl_ior_priority, prim.inner_volume_idx);
						}
						ray = Ray(OffsetRayOrigin(hit_position, bounce_outside ? hit_geometry_normal : -hit_geometry_normal),
							ray.direction,
							ray.ray_ior,
							bounce_outside ? prim.outer_volume_idx : prim.inner_volume_idx);
						++depth;
						continue;
					}

					if (prim.treat_as_boundary)
					{
						bool bounce_outside = glm::dot(hit_geometry_normal, ray.direction) > 0.0f;
						if (bounce_outside) {
							ray_transfer.PopRecord(hit_record.hit_instance_idx);
						}
						else {
							ray_transfer.PushRecord(hit_record.hit_instance_idx, mtl_ior, mtl_ior_priority, prim.inner_volume_idx);
						}
						ray = Ray(OffsetRayOrigin(hit_position, bounce_outside ? hit_geometry_normal : -hit_geometry_normal),
							ray.direction, 
							ray.ray_ior, 
							bounce_outside ? prim.outer_volume_idx : prim.inner_volume_idx);
						++depth;
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
						if (render_setting.enable_distant_light)
						{
							L += throughput * EvalDistantLight(render_setting,
								scene,
								image_tile_cache,
								material_bsdf,
								ray.direction,
								prim,
								hit_position,
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
							prim,
							hit_position,
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
						if (render_setting.enable_russian_roulette && sampler.Random1D() < rr) { throughput /= glm::max(rr, 1E-20f); }
						else { break; }
					}

					glm::vec3 new_direction = material_bsdf.ShadingToWorld(wi);
					if (wi.z < 0.0f) // refract
					{
						bool bounce_outside = glm::dot(hit_geometry_normal, new_direction) > 0.0f;
						if (bounce_outside) {
							ray_transfer.PopRecord(hit_record.hit_instance_idx);
						}
						else {
							//printf("push record, prim_idx = %d.\n", hit_record.hit_instance_idx);
							ray_transfer.PushRecord(hit_record.hit_instance_idx, mtl_ior, mtl_ior_priority, prim.inner_volume_idx);
						}
					}

					// this method to avoid self intersection is still not robust, it makes the sphere self-intersection when radius is big
					bool front_side_bounce = glm::dot(hit_geometry_normal, new_direction) >= 0.0f;
					ray = Ray(OffsetRayOrigin(hit_position, front_side_bounce ? hit_geometry_normal : -hit_geometry_normal),
									new_direction,
									front_side_bounce? prim.external_ior : mtl.default_mtl.ior_n, 
									front_side_bounce? prim.outer_volume_idx : prim.inner_volume_idx);
					never_scatter = false;
					++depth;
				}
				else
				{
					// TODO: switch to turn off env light
					L += render_setting.enable_env_light? throughput * Background(ray.direction) : glm::vec3(0.0f);
					break;
				}
			} // for depth
			// if (render_setting.sampler_type == SOBOL) printf("per pixel dim:%d \n", sampler.sobol_sampler.GetDim());
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
			auto get_pixel_idx = [](int x, int y, int width, int height)->uint32_t
			{
				return y * width + x;
			};
			auto gamma_correction = [&](const glm::vec3 &col)->glm::vec3
			{
				return glm::pow(col, glm::vec3(1.0f / render_setting.gamma));
			};

			auto aces_tonemapping = [&](glm::vec3 col, float exposure)->glm::vec3 
			{
				const float A = 2.51f;
				const float B = 0.03f;
				const float C = 2.43f;
				const float D = 0.59f;
				const float E = 0.14f;

				col *= exposure;
				return (col * (A * col + B)) / (col * (C * col + D) + E);
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
};