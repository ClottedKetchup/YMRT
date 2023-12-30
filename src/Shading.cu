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

#include "ShadingUtilities.cuh"
#include "DeviceRandom.cuh"
#include "SampleUtilities.h"
#include "BSDF.cuh"

namespace YumeRT
{

#define MAX_RAY_DEPTH 16

#define TILE_X_RES 16u
#define TILE_Y_RES 16u
#define TILE_PIXEL_COUNT 256u
#define MAX_RAY_COUNT 1024 * 1024

	__device__ __host__ glm::vec3 Background(const glm::vec3 &direction)
	{
		// return glm::vec3(0.0f);
		return glm::mix(glm::vec3(1.0f), glm::vec3(0.05f, 0.2f, 0.85f), direction.y * 0.5f + 0.5f);
	}

	__device__ glm::vec3 EvalDistantLight(const Scene &scene, 
																UberBSDF &uber_bsdf, 
																const glm::vec3 &ray_direction, 
																const glm::vec3 &hit_position, 
																const glm::vec3 &hit_shading_normal, 
																const glm::vec3 &hit_geometry_normal,
																uint32_t px, 
																uint32_t py, 
																PixelRandom &pixel_random)
	{
		assert(scene.distant_lights != nullptr);
		if (scene.distant_lights == nullptr) {return glm::vec3(0.0f);}
		auto random = [&]()->float {return pixel_random.Random1D(px, py); };
		
		glm::vec3 direct_lighting(0.0f);

		glm::vec3 wo = uber_bsdf.WorldToShading(-ray_direction);

		float bsdf_select_pdf = 0.0f;
		int selected_bsdf_idx = uber_bsdf.SelectSingleBSDF(random(), &bsdf_select_pdf);
		if (selected_bsdf_idx == -1) { return glm::vec3(0.0f); }

		// sample from light
		{
			glm::vec3 light_dir, light_pos;
			float light_sample_pdf;
			glm::vec3 Li = scene.distant_lights[0].SampleLi(scene, hit_position, hit_shading_normal, random(), random(), &light_dir, &light_pos, &light_sample_pdf);

			glm::vec3 light_wi = uber_bsdf.WorldToShading(light_dir);
			float light_wi_pdf = 0.0f;
			const glm::vec3 bsdf_weight = uber_bsdf.EvalSingleBSDF(selected_bsdf_idx, wo, light_wi, &light_wi_pdf) * bsdf_select_pdf;

			const glm::vec3 shadow_ray_origin = OffsetRayOrigin(hit_position, glm::dot(light_dir, hit_geometry_normal) >= 0.0f ? hit_geometry_normal : -hit_geometry_normal);
			Ray shadow_ray(shadow_ray_origin, light_dir);

			HitRecord shadow_ray_record;
			if ((bsdf_weight.x + bsdf_weight.y + bsdf_weight.z > 1E-16f) && !BVHTraverse(scene, shadow_ray, &shadow_ray_record, true))
			{
				float mis_weight = 1.0f / (1.0f + Sqr(light_wi_pdf) / glm::min(1E24f, glm::max(Sqr(light_sample_pdf), 1E-16f)));
				direct_lighting +=  mis_weight * glm::min(bsdf_weight, glm::vec3(1E16f)) * glm::min(Li, glm::vec3(1E16f));
			}
		}

		// sample from bsdf
		{
			glm::vec3 bsdf_weight, bsdf_wi;
			float bsdf_sample_pdf = 0.0f;
			bool sample_valid = uber_bsdf.SampleSingleBSDF(selected_bsdf_idx, random(), random(), wo, &bsdf_weight, &bsdf_wi, &bsdf_sample_pdf);

			bsdf_weight *= bsdf_select_pdf;

			if (sample_valid)
			{
				const glm::vec3 light_dir = uber_bsdf.ShadingToWorld(bsdf_wi);
				const glm::vec3 shadow_ray_origin = OffsetRayOrigin(hit_position, glm::dot(light_dir, hit_geometry_normal) >= 0.0f ? hit_geometry_normal : -hit_geometry_normal);
				Ray shadow_ray(shadow_ray_origin, light_dir);

				float bsdf_wi_pdf = 0.0f;
				glm::vec3 Li = scene.distant_lights[0].EvalLi(scene, light_dir, &bsdf_wi_pdf);

				HitRecord shadow_ray_record;
				if ((Li.x + Li.y + Li.z > 1E-16f) && !BVHTraverse(scene, shadow_ray, &shadow_ray_record, true))
				{
					float mis_weight = 1.0f / (1.0f + Sqr(bsdf_wi_pdf) / glm::min(1E24f, glm::max(Sqr(bsdf_sample_pdf), 1E-16f)));
					direct_lighting += mis_weight * glm::min(bsdf_weight, glm::vec3(1E16f)) * glm::min(Li, glm::vec3(1E16f));
				}
			}
		}
		assert(!glm::isnan(direct_lighting.x) && !glm::isnan(direct_lighting.y) && !glm::isnan(direct_lighting.z));
		return  bsdf_select_pdf == 0.0f ? glm::vec3(0.0f) : direct_lighting / bsdf_select_pdf;
	}

	__device__ glm::vec3 EvalShapeLight(const Scene &scene,
																UberBSDF &uber_bsdf,
																const glm::vec3 &ray_direction,
																const glm::vec3 &hit_position,
																const glm::vec3 &hit_shading_normal,
																const glm::vec3 &hit_geometry_normal,
																uint32_t px,
																uint32_t py,
																PixelRandom &pixel_random)
	{
		if (scene.shape_light_count == 0 || scene.shape_lights == nullptr) { return glm::vec3(0.0f); }
		auto random = [&]()->float {return pixel_random.Random1D(px, py); };

		glm::vec3 direct_lighting(0.0f);

		glm::vec3 wo = uber_bsdf.WorldToShading(-ray_direction);

		float bsdf_select_pdf = 0.0f;
		int selected_bsdf_idx = uber_bsdf.SelectSingleBSDF(random(), &bsdf_select_pdf);
		if (selected_bsdf_idx == -1 || bsdf_select_pdf == 0.0f) { return glm::vec3(0.0f); }

		float light_select_pdf = 0.0f;
		int light_idx = SampleLightPower(scene, random(), &light_select_pdf);

		const ShapeLight &shape_light = scene.shape_lights[light_idx];

		// sample from light
		{
			glm::vec3 light_dir, light_pos, light_geo_normal;
			float light_sample_pdf;
			glm::vec3 Li = shape_light.SampleLi(scene, hit_position, hit_shading_normal, random(), random(), &light_dir, &light_pos, &light_geo_normal, &light_sample_pdf);

			glm::vec3 light_wi = uber_bsdf.WorldToShading(light_dir);
			float light_wi_pdf = 0.0f;
			const glm::vec3 bsdf_weight = uber_bsdf.EvalSingleBSDF(selected_bsdf_idx, wo, light_wi, &light_wi_pdf) * bsdf_select_pdf;

			if (bsdf_weight.x + bsdf_weight.y + bsdf_weight.z > 1E-16f)
			{
				HitRecord shadow_ray_record;
				const glm::vec3 shadow_ray_origin = OffsetRayOrigin(hit_position, glm::dot(light_dir, hit_geometry_normal) >= 0.0f ? hit_geometry_normal : -hit_geometry_normal);
				Ray shadow_ray(shadow_ray_origin, light_dir);
				shadow_ray.t = glm::length(OffsetRayOrigin(light_pos, light_geo_normal) - hit_position) * 0.9996f;

				if (!BVHTraverse(scene, shadow_ray, &shadow_ray_record, true))
				{
					float mis_weight = 1.0f / (1.0f + Sqr(light_wi_pdf) / glm::min(1E24f, glm::max(Sqr(light_sample_pdf), 1E-16f)));
					direct_lighting += mis_weight * glm::min(bsdf_weight, glm::vec3(1E16f)) * glm::min(Li, glm::vec3(1E16f));
				}
			}
		}

		// sample from bsdf
		{
			glm::vec3 bsdf_weight, bsdf_wi;
			float bsdf_sample_pdf = 0.0f;
			bool sample_valid = uber_bsdf.SampleSingleBSDF(selected_bsdf_idx, random(), random(), wo, &bsdf_weight, &bsdf_wi, &bsdf_sample_pdf);

			bsdf_weight *= bsdf_select_pdf;

			if (sample_valid)
			{
				glm::vec3 light_dir = uber_bsdf.ShadingToWorld(bsdf_wi);
				glm::vec3 light_pos, light_geo_normal;
				float bsdf_wi_pdf = 0.0f;
				glm::vec3 Li = shape_light.EvalLi(scene, hit_position, light_dir, &light_pos, &light_geo_normal, &bsdf_wi_pdf);

				if ((Li.x + Li.y + Li.z > 1E-16f))
				{
					HitRecord shadow_ray_record;
					glm::vec3 shadow_ray_origin = OffsetRayOrigin(hit_position, glm::dot(light_dir, hit_geometry_normal) >= 0.0f ? hit_geometry_normal : -hit_geometry_normal);
					Ray shadow_ray(shadow_ray_origin, light_dir);
					shadow_ray.t = glm::length(OffsetRayOrigin(light_pos, light_geo_normal) - hit_position) * 0.9996f;

					if (!BVHTraverse(scene, shadow_ray, &shadow_ray_record, true))
					{
						float mis_weight = 1.0f / (1.0f + Sqr(bsdf_wi_pdf) / glm::min(1E24f, glm::max(Sqr(bsdf_sample_pdf), 1E-16f)));
						direct_lighting += mis_weight * glm::min(bsdf_weight, glm::vec3(1E16f)) * glm::min(Li, glm::vec3(1E16f));
					}
				}
			}
		}
		
		// todo: eval shape light contrib 
		return   (bsdf_select_pdf * light_select_pdf)  < 1E-10f?  glm::vec3(0.0f) : (direct_lighting / (bsdf_select_pdf * light_select_pdf));
	}

	__global__ void RenderImage(Scene *scene_ptr,
													PixelRandom *pixel_randoms,
													const RenderSetting render_setting,
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

		uint32_t tile_pixel_id = idx % TILE_PIXEL_COUNT;
		uint32_t tile_pixel_x = tile_pixel_id % TILE_X_RES;
		uint32_t tile_pixel_y = tile_pixel_id / TILE_X_RES;

		uint32_t px = tile_x * TILE_X_RES + tile_pixel_x;
		uint32_t py = tile_y * TILE_Y_RES + tile_pixel_y;
		if (!(px < width && py < height)) { return; }
		uint32_t pixel_idx = py * width + px;
		Scene &scene = *scene_ptr;

		auto random_1D = [&]() ->float {return pixel_randoms[pixel_idx].Random1D(px, py); };

		glm::vec3 col(0.0f);
		for (int sample_idx = 0; sample_idx < render_setting.ssp; ++sample_idx)
		{
			Ray ray;
			if (render_setting.stratified) 
			{
				int stratified_sample_count = render_setting.max_frame_count;
				int current_sample_idx = glm::min(frame_count, stratified_sample_count);
				int stratified_width = (int)glm::ceil(glm::sqrt((float)stratified_sample_count));
				int stratified_x = current_sample_idx / stratified_width;
				int stratified_y = current_sample_idx % stratified_width;
				ray = scene.camera->generateRay(float(px + ((float)stratified_x + random_1D()) / float(stratified_width)) / float(width),
																		 float(py + ((float)stratified_y + random_1D()) / float(stratified_width)) / float(height));
			}
			else 
			{
				ray = scene.camera->generateRay(float(px + random_1D()) / float(width), float(py + random_1D()) / float(height));
			}

			glm::vec3 L(0.0f), throughput(1.0f);
			for (int depth = 1;;)
			{
				if (depth > MAX_RAY_DEPTH) { break; }
				bool hit = false;
				HitRecord hit_record;
				if (BVHTraverse(scene, ray, &hit_record, false))
				{
					glm::vec3 hit_position;
					glm::vec3	hit_shading_normal;
					glm::vec3	hit_geometry_normal;
					glm::vec2	hit_uv;
					glm::vec3	hit_dpdu;
					glm::vec3	hit_dpdv;

					FetchShadingData(scene,
												hit_record,
												&hit_position,
												&hit_shading_normal,
												&hit_geometry_normal,
												&hit_uv,
												&hit_dpdu,
												&hit_dpdv);

					if (sample_idx == 0 && depth == 1) {
						prim_idx_buffer[pixel_idx] = hit_record.hit_instance_idx;
					}

					const PrimitiveInstance &prim = scene.prim_instances[hit_record.hit_instance_idx];
					const Material &mtl = scene.materials[prim.material_idx];

					if (mtl.material_type == LIGHT_MTL)
					{
						if (depth == 1) { L += throughput * mtl.light_material.light_color * mtl.light_material.intensity; }
						break;
					}

					// indirect light
					UberBSDF uber_bsdf;
					uber_bsdf.InitShadingSpace(hit_record.hit_back? -hit_shading_normal : hit_shading_normal,
																  hit_record.hit_back? -hit_dpdu: hit_dpdu);
					// instance's internal ior is implicitly specified by its material
					uber_bsdf.InitBSDFSettings(mtl, ray, hit_record.hit_back, prim.external_ior);
					
					// TODO: direct lighting
					// TODO: switch to turn off direct light
					if (render_setting.enable_distant_light) {
						L += throughput * EvalDistantLight(scene,
							uber_bsdf,
							ray.direction,
							hit_position,
							hit_shading_normal,
							hit_geometry_normal,
							px,
							py,
							pixel_randoms[pixel_idx]);
					}

					// TODO: sample table, light BVH...
					L += throughput * EvalShapeLight(scene,
																			uber_bsdf,
																			ray.direction,
																			hit_position,
																			hit_shading_normal,
																			hit_geometry_normal,
																			px,
																			py,
																			pixel_randoms[pixel_idx]);
					
					// break;
					// current a bunch noise are from indirect light!
					// indirect lighting
					glm::vec3 wo = uber_bsdf.WorldToShading(-ray.direction), wi(0.0f), bsdf_weight(0.0f);
					float pdf = 0.0f;
					BOUNCE_TYPE bounce_type;
					
					bool sample_valid = uber_bsdf.SampleIndirectMix(random_1D(), random_1D(), wo, &bsdf_weight, &wi, &pdf, &bounce_type);
					assert(!glm::isnan(bsdf_weight.x) && !glm::isnan(bsdf_weight.y) && !glm::isnan(bsdf_weight.z));
					if (!sample_valid) { break; }

					// indirect light
					throughput *= bsdf_weight;
					float rr = glm::max(throughput.x, glm::max(throughput.y, throughput.z));
					if (depth + 1 > render_setting.ray_depth)
					{
						// break;
						if (random_1D() < rr) { throughput /= glm::max(rr, 1E-10f); }
						else { break; }
					}

					glm::vec3 new_direction = uber_bsdf.ShadingToWorld(wi);
					bool front_side_bounce = glm::dot(hit_geometry_normal, new_direction) >= 0.0f;
					// this method to avoid self intersection is still not robust, it makes the sphere self-intersection when radius is big
					glm::vec3 new_origin = OffsetRayOrigin(hit_position, front_side_bounce? hit_geometry_normal : -hit_geometry_normal);
					
					ray = Ray(new_origin, new_direction, front_side_bounce? prim.external_ior : mtl.surface_material.ior_n);
					++depth;
				}
				else
				{
					if (sample_idx == 0 && depth == 1) {
						prim_idx_buffer[pixel_idx] = INVALID_UINT_32;
					}
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

		RenderSetting rt_settings = render_setting;

		std::vector<PixelRandom> per_pixel_randoms(width * height, PixelRandom(frame_count));
		PixelRandom *pixel_randoms = nullptr;
		UPLOAD_TO_GPU(pixel_randoms, per_pixel_randoms.data(), sizeof(PixelRandom) * per_pixel_randoms.size());
		FREE_STL(per_pixel_randoms);

		auto start_time = std::chrono::high_resolution_clock::now();
		{
			Scene *scene_ptr = nullptr;
			UPLOAD_TO_GPU(scene_ptr, &scene, sizeof(Scene));

			dim3 block_dim(32, 1, 1);
			dim3 grid_dim(Round_Block_Count(total_pixel_count, block_dim.x), 1, 1);
			void *args[] = {&scene_ptr, 
									&pixel_randoms, 
									&rt_settings, 
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
		}

		FREE_GPU_RESOURCE(pixel_randoms);

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

		if (select_prim_idx != INVALID_UINT_32 && !disable_object_highlight) 
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