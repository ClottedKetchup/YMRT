#pragma once

#include "src/Helper.h"
#include "src/MathCommon.h"
#include "src/SceneDefines.h"
#include "src/HaltonEnumerator.h"
#include "src/core/SceneModule.h"
#include "src/Timer.h"

#include <chrono>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <vector>
#include <list>
#include <memory>
#include <unordered_map>

#include <glad/glad.h>

#include <cuda_runtime.h>
#include <cuda_gl_interop.h>

namespace YMRT {

	static const std::string aov_name_primitive_index = "primitive index";
	static const std::string aov_name_beauty = "beauty";
	static const std::string aov_name_normal = "normal";
	static const std::string aov_name_accumulate = "accumulate";
	static const std::string aov_name_post_processing = "post_processing";

	// note: allocate and transfer on default stream.
	struct DuskImageResource 
	{
	public:
		uint32_t image_width, image_height;

		inline DuskImageResource() = default;

		inline DuskImageResource(uint32_t width, uint32_t height): 
			texture(0), resource(nullptr), image_buffer(nullptr), image_width(width), image_height(height)
		{
		
		}

		inline DuskImageResource(const DuskImageResource& other) = default;

		inline DuskImageResource& operator=(const DuskImageResource& other) = default;

		inline void ResetSize(uint32_t width, uint32_t height) {
			image_width = width, image_height = height;
		}

		inline void Init()
		{
			assert(resource == nullptr && image_buffer == nullptr);
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
			if (image_buffer != nullptr) {
				CUDA_CHECK(cudaFree(image_buffer));
			}
			if (resource != nullptr) {
				CUDA_CHECK(cudaGraphicsUnregisterResource(resource));
			}
			if (texture) {
				glDeleteTextures(1, &texture);
			}
			image_buffer = nullptr, resource = nullptr, texture = 0;
		}

		inline void FillGLTexture()
		{
			cudaArray* mapped_texture;
			CUDA_CHECK(cudaGraphicsMapResources(1, &resource, nullptr));
			CUDA_CHECK(cudaGraphicsSubResourceGetMappedArray(&mapped_texture, resource, 0, 0));
			CUDA_CHECK(cudaMemcpyToArray(mapped_texture, 0, 0, image_buffer, image_width * image_height * sizeof(glm::vec4), cudaMemcpyDeviceToDevice));
			CUDA_CHECK(cudaGraphicsUnmapResources(1, &resource, nullptr));
		}

		inline GLuint GetGLTexture() const {
			return texture;
		}

		inline glm::vec4* GetDevicePtr() {
			return image_buffer;
		}

	private:
		GLuint texture;
		cudaGraphicsResource *resource;
		glm::vec4 *image_buffer;
	};
	
	// note: may use different stream to create memory.
	template<typename T>
	struct DuskDeviceMemory {
		 uint32_t width;
		 uint32_t height;

		 // note: this construction take exist memory resource, don't allocate new memory.
		 DuskDeviceMemory() : width(0), height(0), mem_ptr(nullptr) {}

		DuskDeviceMemory(uint32_t width, uint32_t height, T *ptr): width(width), height(height), mem_ptr(ptr){
			assert(ptr != nullptr);
		}
		DuskDeviceMemory(uint32_t width, uint32_t height) :width(width), height(height), mem_ptr(nullptr) {
			Allocate();
		}
		~DuskDeviceMemory() {
			Release();
		}

		DuskDeviceMemory(const DuskDeviceMemory&) = delete;
		DuskDeviceMemory& operator=(const DuskDeviceMemory&) = delete;

		DuskDeviceMemory(DuskDeviceMemory&& other) noexcept : width(other.width), height(other.height), mem_ptr(other.mem_ptr) {
			other.width = 0, other.height = 0, other.mem_ptr = nullptr;
		}

		DuskDeviceMemory& operator=(DuskDeviceMemory&& other) noexcept {
			width = other.width, height = other.height, mem_ptr = other.mem_ptr;
			other.width = 0, other.height = 0, other.mem_ptr = nullptr;
			return *this;
		}

		T* GetMemPtr() {
			return mem_ptr;
		}

		void Resize(uint32_t _width, uint32_t _height) {
			if (mem_ptr != nullptr) {
				Release();
			}
			width = _width, height = _height;
			Allocate();
		}
		
	private:
		T* mem_ptr;

		inline void Allocate() {
			CUDA_CHECK(cudaMalloc(&mem_ptr, sizeof(T) * width * height));
		}
		inline void Release() {
			FREE_GPU_RESOURCE(mem_ptr);
		}
		
	};

	template<typename T>
	struct DeviceMemoryGuard {
		DeviceMemoryGuard(T *ptr): ptr(ptr) 
		{
		
		}
		~DeviceMemoryGuard() {
			FREE_GPU_RESOURCE(ptr);
		}
		inline T* GetMemPtr() {
			return ptr;
		}
	private:
		T *ptr;
	};

	struct TaskParams {
		SceneResource *scene_resource_ptr;
		const int width, height;
		const int scene_updated;
		const RenderSetting render_setting;
		const uint64_t scene_change_time;
		TaskParams(SceneResource *scene_resource_ptr, const int frame_width, const int frame_height, const int _scene_updated, const RenderSetting &_render_setting, const uint64_t scene_change_time) :
			scene_resource_ptr(scene_resource_ptr), width(frame_width), height(frame_height), scene_updated(_scene_updated), render_setting(_render_setting), scene_change_time(scene_change_time) {}
		TaskParams(const TaskParams&) = default;
		TaskParams& operator=(const TaskParams&) = default;
	};

	struct ExtraTaskResults {
		int task_frame_index;
		float task_render_time;
		int max_accumulate_frame;
	};

	class RenderModule {
	public:
		RenderModule();
		~RenderModule();
		RenderModule(const RenderModule&) = delete;
		RenderModule(const RenderModule&&) = delete;
		RenderModule& operator=(const RenderModule&) = delete;

		friend class GuiModule;

		// note: these function would be called by the main thread.
		void EditorViewFetchResult(SceneResource &scene_resource, const int scene_updated, const RenderSetting &render_setting, const int frame_width, const int frame_height, const uint32_t clicked_primitive_index, bool draw_selected_effect, ExtraTaskResults *extra_task_results = nullptr);
		
		GLuint EditorViewGetTexture(const std::string &name);

		void EditorViewLaunchTask(const TaskParams& task_params);

		template<typename T>
		void EditorViewReadPixel(const std::string& name, uint32_t px, uint32_t py, T *ptr) {
			px = glm::clamp((int)px, 0, m_editor_view_width - 1);
			py = glm::clamp((int)py, 0, m_editor_view_height - 1);
			
			auto pixel_index = py * m_editor_view_width + px;
			if (name == aov_name_primitive_index) {
				auto primitive_index_image = editor_view_primitive_index_image.GetMemPtr();
				CUDA_CHECK(cudaMemcpyAsync(ptr, &primitive_index_image[pixel_index], sizeof(T), cudaMemcpyDeviceToHost, stream_main));
				CUDA_CHECK(cudaStreamSynchronize(stream_main));
			}
			else {
				auto iter = editor_view_image_resources.find(name);
				if (iter != editor_view_image_resources.end()) {
					auto aov_image = iter->second.GetDevicePtr();
					CUDA_CHECK(cudaMemcpyAsync(ptr, &aov_image[pixel_index], sizeof(T), cudaMemcpyDeviceToHost, stream_main));
					CUDA_CHECK(cudaStreamSynchronize(stream_main));
				}
			}
		}

		void PathTracingFetchResult(SceneResource &scene_resource, const int scene_updated, const RenderSetting &render_setting, const int frame_width, const int frame_height, ExtraTaskResults *extra_task_results = nullptr);

		GLuint PathTracingGetTexture(const std::string &name);

		glm::vec4* PathTracingGetDevicePointer(const std::string &name, int &image_width, int &image_height);

		void PathTracingLaunchTask(const TaskParams& task_params);
	
	private:
		std::atomic_bool should_exit;

		cudaStream_t stream_main;

		// these are used by editor view thread.
		cudaStream_t stream_editor_view;
		int m_editor_view_width, m_editor_view_height;
		std::unordered_map<std::string, DuskImageResource> editor_view_image_resources;
		DuskDeviceMemory<uint32_t> editor_view_primitive_index_image;

		// editor view thread async resources.
		std::thread editor_view_thread;

		bool editor_view_image_fetch_over_time;

		std::condition_variable editor_view_task_queue_cv;
		std::mutex editor_view_task_queue_mutex;
		std::list<TaskParams> editor_view_task_queue;

		std::condition_variable editor_view_result_queue_cv;
		std::mutex editor_view_result_queue_mutex;
		std::list<DuskDeviceMemory<glm::vec4>> editor_view_result_queue_albedo;
		std::list<DuskDeviceMemory<uint32_t>> editor_view_result_queue_primitive_index;
		std::list<ExtraTaskResults> editor_view_result_queue_extra;

		std::atomic_int editor_view_frame_index;
		RayCounterData editor_view_ray_counter_data;
		ShadingRayData editor_view_shading_ray_data;
		HitData editor_view_hit_data;
		ShadowRayData editor_view_shadow_ray_data;

		// these are used by rendering thread.
		cudaStream_t stream_path_tracing;
		int m_path_tracing_width, m_path_tracing_height;
		std::unordered_map<std::string, DuskImageResource> path_tracing_image_resources;

		// rendering thread async resources.
		std::thread path_tracing_thread;

		bool path_tracing_image_fetch_over_time;

		std::condition_variable path_tracing_task_queue_cv;
		std::mutex path_tracing_task_queue_mutex;
		std::list<TaskParams> path_tracing_task_queue;

		std::condition_variable path_tracing_result_queue_cv;
		std::mutex path_tracing_result_queue_mutex;
		std::list<DuskDeviceMemory<glm::vec4>> path_tracing_result_queue_beauty;
		std::list<ExtraTaskResults> path_tracing_result_queue_extra;

	    std::atomic_int	path_tracing_frame_index;
		RayCounterData path_tracing_ray_counter_data;
		ShadingRayData path_tracing_shading_ray_data;
		HitData path_tracing_hit_data;
		ShadowRayData path_tracing_shadow_ray_data;

		void EditorViewExecuteTask(TaskParams& task_param);

		void PathTracingExecuteTask(TaskParams& task_param);
	};
};