#include "src/core/RenderModule.h"

namespace YumeRT{

	extern "C" void ImguiTestingAov(const Scene & scene, glm::vec4 * beauty, int width, int height, cudaStream_t & stream);

	extern "C" void ImguiTestingRendering(const Scene & scene, glm::vec4 * beauty, int width, int height, cudaStream_t & stream);
	
	RenderModule::RenderModule(): 
		m_editor_view_width(16), m_editor_view_height(16), 
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
			CUDA_CHECK(cudaStreamCreateWithPriority(&stream_path_tracing, cudaStreamDefault, max_priority - 1));
		}

		editor_view_image_resources[aov_name_beauty] = DuskImageResource(m_editor_view_width, m_editor_view_height);
		for (auto& item : editor_view_image_resources) {
			item.second.Init();
		}

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
					auto task_param = editor_view_task_queue.back();
					editor_view_task_queue.pop_back();

					// unlock task queue.
					editor_view_task_queue_lk.unlock();

					// pipeline.
					auto editor_view_rendering_function = [&]() {
						auto& scene_resource = (*task_param.scene_resource_ptr);
						glm::vec4 *ptr = nullptr;
						CUDA_CHECK(cudaMallocAsync(&ptr, sizeof(glm::vec4) * task_param.width * task_param.height, stream_editor_view));
						CUDA_CHECK(cudaStreamSynchronize(stream_editor_view));
						DuskDeviceMemory<glm::vec4> device_mem(task_param.width, task_param.height, ptr);

						// rendering block.
						{ 
							// lock scene.
							std::shared_lock<std::shared_mutex> scene_read_lk(scene_resource.scene_mutex);
							if (scene_resource.scene_change_time != task_param.scene_change_time) {
								return;
							}
							ImguiTestingAov(scene_resource.scene, device_mem.GetMemPtr(), task_param.width, task_param.height, stream_editor_view);
						}

						// result output block.
						{
							// lock scene.
							std::shared_lock<std::shared_mutex> scene_read_lk(scene_resource.scene_mutex);
							if (scene_resource.scene_change_time != task_param.scene_change_time) {
								return;
							}

							{
								std::unique_lock<std::mutex> editor_view_result_queue_lk(editor_view_result_queue_mutex);
								editor_view_result_queue.push_front(std::move(device_mem));
							}
							editor_view_result_queue_cv.notify_one();
						}
					}; // editor view thread rendering function.

					editor_view_rendering_function();
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
					auto task_param = path_tracing_task_queue.back();
					path_tracing_task_queue.pop_back();

					// unlock task queue.
					path_tracing_task_queue_lk.unlock();

					// pipeline.
					auto path_tracing_rendering_function = [&]() {
						auto& scene_resource = (*task_param.scene_resource_ptr);
						glm::vec4 *ptr = nullptr;
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
								std::unique_lock<std::mutex> path_tracing_result_queue_lk(path_tracing_result_queue_mutex);
								path_tracing_result_queue.push_front(std::move(device_mem));
							}
							path_tracing_result_queue_cv.notify_one();
						}
					}; // editor view thread rendering function.

					path_tracing_rendering_function();
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

		CUDA_CHECK(cudaStreamDestroy(stream_editor_view));
		CUDA_CHECK(cudaStreamDestroy(stream_path_tracing));
		CUDA_CHECK(cudaStreamDestroy(stream_main));

		editor_view_task_queue.clear();
		editor_view_result_queue.clear();
		for (auto& item : editor_view_image_resources) {
			item.second.Destroy();
		}
		editor_view_image_resources.clear();

		path_tracing_task_queue.clear();
		path_tracing_result_queue.clear();
		for (auto& item : path_tracing_image_resources) {
			item.second.Destroy();
		}
		path_tracing_image_resources.clear();
	}

	void RenderModule::EditorViewFetchResult(SceneResource &scene_resource, const int frame_width, const int frame_height)
	{
		EditorViewLaunchTask(TaskParams(&scene_resource, frame_width, frame_height, scene_resource.scene_change_time));

		if (m_editor_view_width != frame_width || m_editor_view_height != frame_height) {
			m_editor_view_width = frame_width, m_editor_view_height = frame_height;
			for (auto& resource : editor_view_image_resources) {
				resource.second.Destroy();
				resource.second.ResetSize(frame_width, frame_height);
				resource.second.Init();
			}
		}

		// execute the rendering kernel.
		auto iter = editor_view_image_resources.find(aov_name_beauty); 
		if (iter != editor_view_image_resources.end()) {
			auto &image = iter->second;
			
			std::unique_lock<std::mutex> editor_view_result_queue_lk(editor_view_result_queue_mutex);
			editor_view_result_queue_cv.wait(editor_view_result_queue_lk, [&]() {
				return !editor_view_result_queue.empty(); 
			});
			// note: this memory should release after copy complete.
			auto device_mem = std::move(editor_view_result_queue.back());
			editor_view_result_queue.pop_back();
			editor_view_result_queue_lk.unlock();
			assert(device_mem.width == frame_width && device_mem.height == frame_height);
			CUDA_CHECK(cudaMemcpyAsync(image.GetDevicePtr(), device_mem.GetMemPtr(), sizeof(glm::vec4) * frame_width * frame_height, cudaMemcpyDeviceToDevice, stream_main));
			CUDA_CHECK(cudaStreamSynchronize(stream_main));
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

	void RenderModule::PathTracingFetchResult(SceneResource &scene_resource, const int frame_width, const int frame_height)
	{
		PathTracingLaunchTask(TaskParams(&scene_resource, frame_width, frame_height, scene_resource.scene_change_time));

		if (m_path_tracing_width != frame_width || m_path_tracing_height != frame_height) {
			m_path_tracing_width = frame_width, m_path_tracing_height = frame_height;
			for (auto& resource : path_tracing_image_resources) {
				resource.second.Destroy();
				resource.second.ResetSize(frame_width, frame_height);
				resource.second.Init();
			}
		}

		auto iter = path_tracing_image_resources.find(aov_name_beauty);
		if (iter != path_tracing_image_resources.end()) 
		{
			auto& image = iter->second;

			std::unique_lock<std::mutex> path_tracing_result_queue_lk(path_tracing_result_queue_mutex);
			auto result_queue_not_empty = path_tracing_result_queue_cv.wait_for(path_tracing_result_queue_lk, std::chrono::milliseconds(10), [&]() {
				return !path_tracing_result_queue.empty();
			});

			assert(!path_tracing_result_queue.empty());
			if (result_queue_not_empty) {
				// note: this memory should release after copy complete.
				auto device_mem = std::move(path_tracing_result_queue.back());
				path_tracing_result_queue.pop_back();
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
};