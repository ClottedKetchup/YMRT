#include "src/core/RenderModule.h"

namespace YumeRT{

#define MAX_WAIT_TIME 10
#define CV_WAIT_DURATION 1

	extern "C" void ImguiTestingAov(const Scene & scene, glm::vec4 * beauty, uint32_t * primitive_index, int width, int height, cudaStream_t & stream);

	extern "C" void ImguiTestingRendering(const Scene & scene, glm::vec4 * beauty, int width, int height, cudaStream_t & stream);

	extern "C" void AllocateRayCounter(RayCounterData & ray_counter_data);

	extern "C" void ReleaseRayCounter(RayCounterData & ray_counter_data);

	extern "C" void AllocateShadingRayData(ShadingRayData & shading_ray_data);

	extern "C" void ReleaseShadingRayData(ShadingRayData & shading_ray_data);

	extern "C" void AllocateShadowRayData(ShadowRayData & shadow_ray_data);

	extern "C" void ReleaseShadowRayData(ShadowRayData & shadow_ray_data);
	
	RenderModule::RenderModule(): 
		m_editor_view_width(16), m_editor_view_height(16), editor_view_primitive_index_image(), 
		editor_view_image_fetch_over_time(true),
		m_path_tracing_width(16), m_path_tracing_height(16), 
		accumulated_image_count(0), 
		should_exit{ false }
	{
		int min_priority = 0, max_priority = 0;
		CUDA_CHECK(cudaDeviceGetStreamPriorityRange(&min_priority, &max_priority));
		if (min_priority == 0 && max_priority == 0) {
			CUDA_CHECK(cudaStreamCreate(&stream_main));
			CUDA_CHECK(cudaStreamCreate(&stream_editor_view));
			CUDA_CHECK(cudaStreamCreate(&stream_path_tracing));
		}
		else {
			CUDA_CHECK(cudaStreamCreateWithPriority(&stream_main, cudaStreamDefault, max_priority));
			CUDA_CHECK(cudaStreamCreateWithPriority(&stream_editor_view, cudaStreamDefault, max_priority));
			CUDA_CHECK(cudaStreamCreateWithPriority(&stream_path_tracing, cudaStreamDefault, min_priority));
		}

		AllocateRayCounter(editor_view_ray_counter_data);
		AllocateShadingRayData(editor_view_shading_ray_data);
		AllocateShadowRayData(editor_view_shadow_ray_data);

		editor_view_primitive_index_image.Resize(m_editor_view_width, m_editor_view_height);
		editor_view_image_resources[aov_name_beauty] = DuskImageResource(m_editor_view_width, m_editor_view_height);
		for (auto& item : editor_view_image_resources) {
			item.second.Init();
		}

		AllocateRayCounter(path_tracing_ray_counter_data);
		AllocateShadingRayData(path_tracing_shading_ray_data);
		AllocateShadowRayData(path_tracing_shadow_ray_data);

		path_tracing_image_resources[aov_name_beauty] = DuskImageResource(m_path_tracing_width, m_path_tracing_height);
		for (auto& item : path_tracing_image_resources) {
			item.second.Init();
		}

		editor_view_thread = std::thread([&]() 
		{
			while(true) 
			{
				std::unique_lock<std::mutex> editor_view_task_queue_lk(editor_view_task_queue_mutex);
				editor_view_task_queue_cv.wait(editor_view_task_queue_lk, [&]() {
					return !editor_view_task_queue.empty() || should_exit.load();
				});

				if (should_exit.load()) {
					break;
				}
				else {
					// fetch task and rendering.
					assert(!editor_view_task_queue.empty());
					TaskParams task_param = editor_view_task_queue.back();
					editor_view_task_queue.pop_back();

					// unlock task queue.
					editor_view_task_queue_lk.unlock();

					EditorViewExecuteTask(task_param);
				}
			} // rendering loop.
		});

		path_tracing_thread = std::thread([&]()
		{
			while (true)
			{
				std::unique_lock<std::mutex> path_tracing_task_queue_lk(path_tracing_task_queue_mutex);
				path_tracing_task_queue_cv.wait(path_tracing_task_queue_lk, [&]() {
					return !path_tracing_task_queue.empty() || should_exit;
				});

				if (should_exit.load()) {
					break;
				}
				else {
					// fetch task and rendering.
					assert(!path_tracing_task_queue.empty());
					TaskParams task_param = path_tracing_task_queue.back();
					path_tracing_task_queue.pop_back();

					// unlock task queue.
					path_tracing_task_queue_lk.unlock();

					PathTracingExecuteTask(task_param);
				}
			} // rendering loop.
		});
	}

	RenderModule::~RenderModule() 
	{
		should_exit.store(true);
		editor_view_task_queue_cv.notify_one();
		if (editor_view_thread.joinable()) {
			editor_view_thread.join();
		}
		path_tracing_task_queue_cv.notify_one();
		if (path_tracing_thread.joinable()) {
			path_tracing_thread.join();
		}

		ReleaseRayCounter(editor_view_ray_counter_data);
		ReleaseShadingRayData(editor_view_shading_ray_data);
		ReleaseShadowRayData(editor_view_shadow_ray_data);

		ReleaseRayCounter(path_tracing_ray_counter_data);
		ReleaseShadingRayData(path_tracing_shading_ray_data);
		ReleaseShadowRayData(path_tracing_shadow_ray_data);

		CUDA_CHECK(cudaStreamDestroy(stream_editor_view));
		CUDA_CHECK(cudaStreamDestroy(stream_path_tracing));
		CUDA_CHECK(cudaStreamDestroy(stream_main));

		editor_view_task_queue.clear();
		editor_view_result_queue_albedo.clear();
		editor_view_result_queue_primitive_index.clear();
		for (auto& item : editor_view_image_resources) {
			item.second.Destroy();
		}
		editor_view_image_resources.clear(); // note: primitive index image will auto release.

		path_tracing_task_queue.clear();
		path_tracing_result_queue_beauty.clear();
		for (auto& item : path_tracing_image_resources) {
			item.second.Destroy();
		}
		path_tracing_image_resources.clear();
	}

	void RenderModule::EditorViewFetchResult(SceneResource &scene_resource, const RenderSetting &render_setting, const int frame_width, const int frame_height)
	{
		EditorViewLaunchTask(TaskParams(&scene_resource, frame_width, frame_height, render_setting, scene_resource.scene_change_time));

		if (m_editor_view_width != frame_width || m_editor_view_height != frame_height) {
			m_editor_view_width = frame_width, m_editor_view_height = frame_height;
			for (auto& resource : editor_view_image_resources) {
				resource.second.Destroy();
				resource.second.ResetSize(frame_width, frame_height);
				resource.second.Init();
			}
			editor_view_primitive_index_image.Resize(frame_width, frame_height);
		}

		// execute the rendering kernel.
		auto iter = editor_view_image_resources.find(aov_name_beauty); 
		if (iter != editor_view_image_resources.end()) {
			auto& albedo_image = iter->second;
			auto& primitive_image = editor_view_primitive_index_image;
			
			std::unique_lock<std::mutex> editor_view_result_queue_lk(editor_view_result_queue_mutex);

			auto lock_strat_time = std::chrono::steady_clock::now();
			while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lock_strat_time).count() < MAX_WAIT_TIME) {
				editor_view_result_queue_cv.wait_for(editor_view_result_queue_lk, std::chrono::milliseconds(CV_WAIT_DURATION));
				if (!editor_view_result_queue_albedo.empty()) {
					break;
				}
			}
			
			editor_view_image_fetch_over_time = true;
			if (!editor_view_result_queue_albedo.empty()) {
				assert(!editor_view_result_queue_primitive_index.empty());

				editor_view_image_fetch_over_time = false;
				
				auto device_mem_albedo = std::move(editor_view_result_queue_albedo.back());
				editor_view_result_queue_albedo.pop_back();
				auto device_mem_primitive_index = std::move(editor_view_result_queue_primitive_index.back());
				editor_view_result_queue_primitive_index.pop_back();

				editor_view_result_queue_lk.unlock();

				assert(device_mem_albedo.width == albedo_image.image_width && device_mem_albedo.height == albedo_image.image_height);
				CUDA_CHECK(cudaMemcpyAsync(albedo_image.GetDevicePtr(), device_mem_albedo.GetMemPtr(), sizeof(glm::vec4) * frame_width * frame_height, cudaMemcpyDeviceToDevice, stream_main));
				assert(device_mem_primitive_index.width == primitive_image.width && device_mem_primitive_index.height == primitive_image.height);
				CUDA_CHECK(cudaMemcpyAsync(primitive_image.GetMemPtr(), device_mem_primitive_index.GetMemPtr(), sizeof(uint32_t) * frame_width * frame_height, cudaMemcpyDeviceToDevice, stream_main));
				
				CUDA_CHECK(cudaStreamSynchronize(stream_main));
			}
		}
		
		for (auto& resource : editor_view_image_resources) {
			resource.second.FillGLTexture();
		}
	}

	GLuint RenderModule::EditorViewGetTexture(const std::string& name)
	{
		// note: only the editor thread will use.
		auto iter = editor_view_image_resources.find(name);
		return iter != editor_view_image_resources.end() ? iter->second.GetGLTexture() : 0u;
	}

	void RenderModule::EditorViewLaunchTask(const TaskParams &task_params)
	{
		{
			std::unique_lock<std::mutex> editor_view_task_queue_lk(editor_view_task_queue_mutex);
			editor_view_task_queue.push_front(task_params);
		}
		editor_view_task_queue_cv.notify_one();
	}

	void RenderModule::PathTracingFetchResult(SceneResource &scene_resource, const RenderSetting &render_setting, const int frame_width, const int frame_height)
	{
		PathTracingLaunchTask(TaskParams(&scene_resource, frame_width, frame_height, render_setting, scene_resource.scene_change_time));

		if (m_path_tracing_width != frame_width || m_path_tracing_height != frame_height) {
			m_path_tracing_width = frame_width, m_path_tracing_height = frame_height;
			for (auto& resource : path_tracing_image_resources) {
				resource.second.Destroy();
				resource.second.ResetSize(frame_width, frame_height);
				resource.second.Init();
			}
		}

		auto iter = path_tracing_image_resources.find(aov_name_beauty);
		if (iter != path_tracing_image_resources.end()) {
			auto& image = iter->second;

			std::unique_lock<std::mutex> path_tracing_result_queue_lk(path_tracing_result_queue_mutex);
			
			auto lock_strat_time = std::chrono::steady_clock::now();
			while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lock_strat_time).count() < MAX_WAIT_TIME) {
				path_tracing_result_queue_cv.wait_for(path_tracing_result_queue_lk, std::chrono::milliseconds(CV_WAIT_DURATION));
				if (!path_tracing_result_queue_beauty.empty()) {
					break;
				}
			}

			if (!path_tracing_result_queue_beauty.empty()) {
				// note: this memory should release after copy complete.
				auto device_mem = std::move(path_tracing_result_queue_beauty.back());
				path_tracing_result_queue_beauty.pop_back();
				path_tracing_result_queue_lk.unlock();
				assert(device_mem.width == frame_width && device_mem.height == frame_height);
				CUDA_CHECK(cudaMemcpyAsync(image.GetDevicePtr(), device_mem.GetMemPtr(), sizeof(glm::vec4) * frame_width * frame_height, cudaMemcpyDeviceToDevice, stream_main));
				CUDA_CHECK(cudaStreamSynchronize(stream_main));
			}
		}

		for (auto& resource : path_tracing_image_resources) {
			resource.second.FillGLTexture();
		}
	}

	GLuint RenderModule::PathTracingGetTexture(const std::string& name)
	{
		auto iter = path_tracing_image_resources.find(name);
		return iter != path_tracing_image_resources.end() ? iter->second.GetGLTexture() : 0u;
	}

	void RenderModule::PathTracingLaunchTask(const TaskParams& task_params)
	{
		{
			std::unique_lock<std::mutex> path_tracing_task_queue_lk(path_tracing_task_queue_mutex);
			path_tracing_task_queue.push_front(task_params); 
		}
		path_tracing_task_queue_cv.notify_one();
	}

	void RenderModule::EditorViewExecuteTask(TaskParams& task_param)
	{
		auto& scene_resource = *(task_param.scene_resource_ptr);

		glm::vec4* albedo_ptr = nullptr;
		CUDA_CHECK(cudaMallocAsync(&albedo_ptr, sizeof(glm::vec4) * task_param.width * task_param.height, stream_editor_view));
		uint32_t* primitive_index_ptr = nullptr;
		CUDA_CHECK(cudaMallocAsync(&primitive_index_ptr, sizeof(uint32_t) * task_param.width * task_param.height, stream_editor_view));

		CUDA_CHECK(cudaStreamSynchronize(stream_editor_view));
		DuskDeviceMemory<glm::vec4> device_mem_albedo(task_param.width, task_param.height, albedo_ptr);
		DuskDeviceMemory<uint32_t>  device_mem_primitive_indices(task_param.width, task_param.height, primitive_index_ptr);

		// rendering block.
		{
			// lock scene.
			std::shared_lock<std::shared_mutex> scene_read_lk(scene_resource.scene_mutex);
			if (scene_resource.scene_change_time != task_param.scene_change_time) {
				return;
			}
			ImguiTestingAov(scene_resource.scene, device_mem_albedo.GetMemPtr(), device_mem_primitive_indices.GetMemPtr(), task_param.width, task_param.height, stream_editor_view);
		}

		// result output block.
		{
			// lock scene.
			std::shared_lock<std::shared_mutex> scene_read_lk(scene_resource.scene_mutex);
			if (scene_resource.scene_change_time != task_param.scene_change_time) {
				return;
			}

			{
				// push result to queue.
				std::unique_lock<std::mutex> editor_view_result_queue_lk(editor_view_result_queue_mutex);
				editor_view_result_queue_albedo.push_front(std::move(device_mem_albedo));
				editor_view_result_queue_primitive_index.push_front(std::move(device_mem_primitive_indices));
			}
			editor_view_result_queue_cv.notify_one();
		}
	}

	void RenderModule::PathTracingExecuteTask(TaskParams& task_param)
	{
		auto& scene_resource = *(task_param.scene_resource_ptr);
		glm::vec4* ptr = nullptr;
		CUDA_CHECK(cudaMallocAsync(&ptr, sizeof(glm::vec4) * task_param.width * task_param.height, stream_path_tracing));
		CUDA_CHECK(cudaStreamSynchronize(stream_path_tracing));
		DuskDeviceMemory<glm::vec4> device_mem(task_param.width, task_param.height, ptr);

		// rendering block.
		{
			// lock scene.
			std::shared_lock<std::shared_mutex> scene_read_lk(scene_resource.scene_mutex);
			if (scene_resource.scene_change_time != task_param.scene_change_time) {
				return;
			}
			ImguiTestingRendering(scene_resource.scene, device_mem.GetMemPtr(), task_param.width, task_param.height, stream_path_tracing);
		}

		// result output block.
		{
			// lock scene.
			std::shared_lock<std::shared_mutex> scene_read_lk(scene_resource.scene_mutex);
			if (scene_resource.scene_change_time != task_param.scene_change_time) {
				return;
			}

			{
				// push result to queue.
				std::unique_lock<std::mutex> path_tracing_result_queue_lk(path_tracing_result_queue_mutex);
				path_tracing_result_queue_beauty.push_front(std::move(device_mem));
			}
			path_tracing_result_queue_cv.notify_one();
		}
	}
};