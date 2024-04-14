#pragma once

#include <vector>
#include <memory>
#include <unordered_map>

#include <glad/glad.h>

#include <cuda_runtime.h>
#include <cuda_gl_interop.h>

#include "Helper.h"
#include "MathCommon.h"
#include "SceneDefines.h"
#include "SceneManager.h"

namespace YumeRT 
{
	extern "C" void RenderScene(const Scene &scene, 
													const TextureManager &texture_manager,
													const RenderSetting &render_setting, 
													glm::vec4 *image, 
													uint32_t *prim_idx_buffer, 
													uint32_t width, 
													uint32_t height, 
													int frame_count, 
													float *render_time);

	extern "C" void AccumulateImage(const glm::vec4 *beauty, glm::vec4 *accumulate, uint32_t width, uint32_t height, int frame_count, const RenderSetting &render_setting);

	extern "C" void PostProcessing(glm::vec4 *postprocess_image, uint32_t *prim_idx_buffer, const RenderSetting &render_setting, uint32_t width, uint32_t height, uint32_t select_prim_idx, bool disable_object_highlight);

	struct ImageResource
	{
		GLuint texture = 0;
		cudaGraphicsResource *resource = nullptr;
		glm::vec4 *image_buffer = nullptr;

		inline void Init(uint32_t image_width, uint32_t image_height)
		{	
			CUDA_CHECK(cudaMalloc(&image_buffer, sizeof(glm::vec4) * image_width * image_height));

			glGenTextures(1, &texture);
			glBindTexture(GL_TEXTURE_2D, texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, image_width, image_height, 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glBindTexture(GL_TEXTURE_2D, 0);

			CUDA_CHECK(cudaGraphicsGLRegisterImage(&resource, texture, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsNone));
		}

		inline void Destroy()
		{
			if (image_buffer != nullptr)
			{
				CUDA_CHECK(cudaFree(image_buffer));
			}
			if (resource != nullptr)
			{
				CUDA_CHECK(cudaGraphicsUnregisterResource(resource));
			}
			if (texture)
			{
				glDeleteTextures(1, &texture);
			}
			image_buffer = nullptr, resource = nullptr, texture = 0;
		}

		inline void Fill(uint32_t image_width, uint32_t image_height)
		{
			cudaArray *mapped_texture;
			CUDA_CHECK(cudaGraphicsMapResources(1, &resource, nullptr));
			CUDA_CHECK(cudaGraphicsSubResourceGetMappedArray(&mapped_texture, resource, 0, 0));
			CUDA_CHECK(cudaMemcpyToArray(mapped_texture, 0, 0, image_buffer, image_width * image_height * sizeof(glm::vec4), cudaMemcpyDeviceToDevice));
			CUDA_CHECK(cudaGraphicsUnmapResources(1, &resource, nullptr));
		}

		inline GLuint GetGLTexture() const
		{
			return texture;
		}

		inline glm::vec4* GetDevicePtr()
		{
			return image_buffer;
		}

	};

	class Renderer 
	{
	public:

		inline Renderer(const Renderer&) = delete;
		inline Renderer(const Renderer&&) = delete;
		inline Renderer& operator=(const Renderer&) = delete;
		inline Renderer() 
		{
			printf("Renderer module init...\n");
			resource_names = { "Beauty","Accumulate" };
			image_resources.resize(resource_names.size());
			for (uint32_t aov_idx = 0; aov_idx < (uint32_t)image_resources.size(); ++aov_idx)
			{
				resource_map[resource_names[aov_idx]] = aov_idx;
			}
		}
		inline ~Renderer() 
		{
			printf("Renderer module exit...\n");
			DestroyResources();
		}

		// scene should be accompanied with some scene flag
		inline void Render(const Scene &scene, const TextureManager &texture_manager, bool is_scene_change)
		{
			frame_count = (is_scene_change || render_setting_change) ? 1 : (frame_count + 1);

			hash_frame_count = (is_scene_change || render_setting_change) ? 1 : (hash_frame_count + 1);

			glm::vec4 *beauty_image = GetImageResource("Beauty").GetDevicePtr();
			glm::vec4 *accumulate_image = GetImageResource("Accumulate").GetDevicePtr();

			RenderScene(scene, 
								  texture_manager,
								  render_setting,
								  beauty_image, 
								  prim_idx_buffer,
								  image_width, 
								  image_height, 
								 (int)hash_frame_count,
							   &rendering_time);

			if (is_scene_change || render_setting_change) { CUDA_CHECK(cudaMemset(accumulate_image, 0, sizeof(glm::vec4) * image_width * image_height)); }

			AccumulateImage(beauty_image,
										  accumulate_image,
										  image_width, 
										  image_height, 
										  frame_count,
										  render_setting);
			
			FillGLTexture();

			render_setting_change = false;
		}

		inline void DestroyResources()
		{
			for (auto& resource : image_resources) 
			{
				resource.Destroy();
			}
			FREE_GPU_RESOURCE(prim_idx_buffer);
			postprocess_image.Destroy();
		}

		inline void PrepareResources()
		{
			for (auto& resource : image_resources)
			{
				resource.Init(image_width, image_height);
			}
			CUDA_CHECK(cudaMalloc(&prim_idx_buffer, sizeof(uint32_t) * image_width * image_height));
			postprocess_image.Init(image_width, image_height);
		}

		inline void FillGLTexture()
		{
			for (auto& resource : image_resources)
			{
				resource.Fill(image_width, image_height);
			}
		}

		inline void ReadPixel(uint32_t x, uint32_t y, uint32_t aov_idx, glm::vec4 *color, uint32_t *prim_idx)
		{
			x = glm::clamp(x, 0u, image_width - 1);
			y = glm::clamp(y, 0u, image_height - 1);

			aov_idx = glm::clamp(aov_idx, 0u, (uint32_t)image_resources.size() - 1u);

			glm::vec4 *aov_image = image_resources[aov_idx].GetDevicePtr(); 

			assert(aov_image != nullptr);
			uint32_t pixel_idx = y * image_width + x;
			CUDA_CHECK(cudaMemcpy(color, &aov_image[pixel_idx], sizeof(glm::vec4), cudaMemcpyDeviceToHost));

			assert(prim_idx_buffer != nullptr);
			CUDA_CHECK(cudaMemcpy(prim_idx, &prim_idx_buffer[pixel_idx], sizeof(uint32_t), cudaMemcpyDeviceToHost));

			CUDA_CHECK(cudaDeviceSynchronize());
		}

		// need prim_idx to do postprocessing 
		inline GLuint SetDisplayAov(uint32_t aov_idx, uint32_t select_prim_idx)
		{ 
			assert(postprocess_image.GetDevicePtr() != nullptr);

			aov_idx = glm::clamp(aov_idx, 0u, (uint32_t)image_resources.size() - 1u);

			CUDA_CHECK(cudaMemcpy(postprocess_image.GetDevicePtr(), 
								   image_resources[aov_idx].GetDevicePtr(), 
								   sizeof(glm::vec4) * image_width * image_height, cudaMemcpyDeviceToDevice));

			PostProcessing(postprocess_image.GetDevicePtr(), prim_idx_buffer, render_setting, image_width, image_height, select_prim_idx, disable_object_highlight);

			postprocess_image.Fill(image_width, image_height);
			return postprocess_image.GetGLTexture();
		}

		inline uint32_t GetWidth() const { return image_width; }

		inline uint32_t GetHeight() const { return image_height; }

		inline float GetRenderingTime() const { return rendering_time; }

		inline void SetWidth(uint32_t width) { image_width = width; }

		inline void SetHeight(uint32_t height) { image_height = height; }

		inline const std::vector<std::string>& GetAovStrings() { return resource_names; }

		inline RenderSetting& GetRenderSetting() { return render_setting; }

		inline void SetRenderSettingChange(bool change) { render_setting_change = change; }

		inline bool& GetObjectHighlightFlag() { return disable_object_highlight; }

		inline int GetFrameCount()const { return frame_count; }

	private:
		std::vector<ImageResource> image_resources;
		std::vector<std::string> resource_names;
		std::unordered_map<std::string, uint32_t> resource_map;

		uint32_t hash_frame_count = 0;
		int frame_count = 1;
		float rendering_time;
		uint32_t image_width = 0, image_height = 0;

		uint32_t *prim_idx_buffer = nullptr;
		ImageResource postprocess_image;

		// TODO: when the render_setting change, clear accumulate buffer
		RenderSetting render_setting;
		bool disable_object_highlight = false;
		bool render_setting_change = false;

		inline ImageResource& GetImageResource(const std::string &aov_name)
		{
			return image_resources[resource_map[aov_name]];
		}
	};
};


