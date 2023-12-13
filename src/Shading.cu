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
		return glm::mix(glm::vec3(1.0f), glm::vec3(0.05f, 0.2f, 0.65f), direction.y * 0.5f + 0.5f);
		// return glm::vec3(0.f);
	}

	__device__ glm::vec3 EvalDirectLighting(const Scene &scene, 
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
		auto random = [&]()->float {return pixel_random.Random(px, py); };
		
		glm::vec3 direct_lighting(0.0f);

		glm::vec3 wo = uber_bsdf.WorldToShading(-ray_direction);

		float bsdf_select_pdf = 0.0f;
		BSDF *sampled_bsdf_ptr = uber_bsdf.SampleOneBSDF(random(), &bsdf_select_pdf);
		if (sampled_bsdf_ptr == nullptr) { return glm::vec3(0.0f); }

		BSDF &sampled_bsdf = *sampled_bsdf_ptr;

		// sample from light
		{
			glm::vec3 light_dir, light_pos;
			float light_sample_pdf;
			glm::vec3 Li = scene.distant_lights[0].SampleLi(hit_position, hit_shading_normal, random(), random(), &light_dir, &light_pos, &light_sample_pdf);

			glm::vec3 light_wi = uber_bsdf.WorldToShading(light_dir);
			float light_wi_pdf = 0.0f;
			const glm::vec3 bsdf_weight = sampled_bsdf.Eval(wo, light_wi, &light_wi_pdf) * bsdf_select_pdf;

			const glm::vec3 shadow_ray_origin = OffsetRayOrigin(hit_position, glm::dot(light_dir, hit_geometry_normal) > 0.0f ? hit_geometry_normal : -hit_geometry_normal);
			Ray shadow_ray(shadow_ray_origin, light_dir);

			HitRecord shadow_ray_record;
			if (!BVHTraverse(scene, shadow_ray, &shadow_ray_record, true))
			{
				float mis_weight = light_sample_pdf > 1E12f ? 1.0f : (Sqr(light_sample_pdf) / glm::max(Sqr(light_sample_pdf) + Sqr(light_wi_pdf), 1E-12f));
				direct_lighting +=  mis_weight * bsdf_weight * Li;
			}
		}

		// sample from bsdf
		{
			glm::vec3 bsdf_weight, bsdf_wi;
			float bsdf_sample_pdf = 0.0f;
			bool sample_valid = sampled_bsdf.Sample(random(), random(), wo, &bsdf_weight, &bsdf_wi, &bsdf_sample_pdf);

			bsdf_weight *= bsdf_select_pdf;

			if (sample_valid)
			{
				const glm::vec3 light_dir = uber_bsdf.ShadingToWorld(bsdf_wi);
				const glm::vec3 shadow_ray_origin = OffsetRayOrigin(hit_position, glm::dot(light_dir, hit_geometry_normal) > 0.0f ? hit_geometry_normal : -hit_geometry_normal);
				Ray shadow_ray(shadow_ray_origin, light_dir);

				float bsdf_wi_pdf = 0.0f;
				const glm::vec3 Li = scene.distant_lights[0].EvalLi(light_dir, &bsdf_wi_pdf);

				HitRecord shadow_ray_record;
				if (!BVHTraverse(scene, shadow_ray, &shadow_ray_record, true))
				{
					float mis_weight = bsdf_sample_pdf > 1E12f ? 1.0f : (Sqr(bsdf_sample_pdf) / glm::max(Sqr(bsdf_sample_pdf) + Sqr(bsdf_wi_pdf), 1E-12f));
					direct_lighting += mis_weight *  bsdf_weight * Li;
				}
			}
		}
		assert(!glm::isnan(direct_lighting.x) && !glm::isnan(direct_lighting.y) && !glm::isnan(direct_lighting.z));
		return  bsdf_select_pdf == 0.0f ? glm::vec3(0.0f) : direct_lighting / bsdf_select_pdf;
	}

	__global__ void RenderImage(Scene *scene_ptr,
													PixelRandom *pixel_randoms,
													const RenderSetting render_setting,
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

		auto random = [&]() ->float {return pixel_randoms[pixel_idx].Random(px, py); };
		
		glm::vec3 col(0.0f);
		for (int sample_idx = 0; sample_idx < render_setting.ssp; ++sample_idx)
		{
			Ray ray = scene.camera->generateRay(float(px + random()) / float(width), float(py + random()) / float(height));

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
						L += throughput * mtl.light_material.light_color * mtl.light_material.intensity;
						break;
					}

					// indirect light
					UberBSDF uber_bsdf;
					uber_bsdf.InitShadingSpace(hit_record.hit_back? -hit_shading_normal : hit_shading_normal,
																  hit_record.hit_back? -hit_dpdu: hit_dpdu);
					uber_bsdf.InitBSDFSettings(mtl, ray.direction, hit_record.hit_back, prim.external_ior);
					
					// TODO: direct lighting
					L += throughput * EvalDirectLighting(scene, 
						uber_bsdf, 
						ray.direction, 
						hit_position, 
						hit_shading_normal, 
						hit_geometry_normal, 
						px, 
						py, 
						pixel_randoms[pixel_idx]);

					
					// current a bunch noise are from indirect light!
					// indirect lighting
					glm::vec3 wo = uber_bsdf.WorldToShading(-ray.direction), wi(0.0f), bsdf_weight(0.0f);
					float pdf = 0.0f;
					BOUNCE_TYPE bounce_type;
					
					bool sample_valid = uber_bsdf.SampleIndirectMix(random(), random(), wo, &bsdf_weight, &wi, &pdf, &bounce_type);
					assert(!glm::isnan(bsdf_weight.x) && !glm::isnan(bsdf_weight.y) && !glm::isnan(bsdf_weight.z));
					if (!sample_valid) { break; }

					// indirect light
					throughput *= bsdf_weight;
					float rr = glm::max(throughput.x, glm::max(throughput.y, throughput.z));
					if (depth + 1 > render_setting.ray_depth)
					{
						if (random() < rr) { throughput /= glm::max(rr, 1E-8f); }
						else { break; }
					}

					glm::vec3 new_direction = uber_bsdf.ShadingToWorld(wi);
					glm::vec3 new_origin = OffsetRayOrigin(hit_position, glm::dot(hit_geometry_normal, new_direction) > 0.0f? hit_geometry_normal : -hit_geometry_normal);
					
					ray = Ray(new_origin, new_direction);
					++depth;
				}
				else
				{
					if (sample_idx == 0 && depth == 1) {
						prim_idx_buffer[pixel_idx] = INVALID_UINT_32;
					}
					L += throughput * Background(ray.direction);
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
			void *args[] = {&scene_ptr, &pixel_randoms, &rt_settings, &image, &prim_idx_buffer, &width, &height, &tile_count_x, &tile_count_y};
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
		float frame_count)
	{
		uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
		uint32_t idy = blockIdx.y * blockDim.y + threadIdx.y;
		if (idx < width && idy < height)
		{
			uint32_t pixel_idx = idy * width + idx;
			accumulate[pixel_idx] = (accumulate[pixel_idx] * glm::max(frame_count - 1.0f, 0.0f) + beauty[pixel_idx]) / frame_count;
		}
	}

	extern "C" void AccumulateImage(const glm::vec4 *beauty, glm::vec4 *accumulate, uint32_t width, uint32_t height, float frame_count, const RenderSetting &render_setting)
	{
		assert(beauty != nullptr && accumulate != nullptr);
		if ((int)frame_count <= 1) { CUDA_CHECK(cudaMemset(accumulate, 0, sizeof(glm::vec4) * width * height)); }
		if (render_setting.max_frame_count > 0 && (int)frame_count >= render_setting.max_frame_count && (int)frame_count != 1) { return; }

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