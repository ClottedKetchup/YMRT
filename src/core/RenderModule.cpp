#include "src/core/RenderModule.h"

namespace YMRT{

#define MAX_WAIT_TIME 10
#define CV_WAIT_DURATION 1

#define MAX_TASK_QUEUE_COUNT 32

	extern "C" void ImguiTestingAov(const Scene & scene, glm::vec4 * beauty, uint32_t * primitive_index, int width, int height, cudaStream_t & stream);

	extern "C" void ImguiTestingRendering(const Scene & scene, glm::vec4 * beauty, int width, int height, cudaStream_t & stream);

	extern "C" void AllocateRayCounter(RayCounterData & ray_counter_data);

	extern "C" void ReleaseRayCounter(RayCounterData & ray_counter_data);

	extern "C" void AllocateShadingRayData(ShadingRayData & shading_ray_data);

	extern "C" void ReleaseShadingRayData(ShadingRayData & shading_ray_data);

	extern "C" void AllocateHitData(HitData & hit_data);

	extern "C" void ReleaseHitData(HitData & hit_data);

	extern "C" void AllocateShadowRayData(ShadowRayData & shadow_ray_data);

	extern "C" void ReleaseShadowRayData(ShadowRayData & shadow_ray_data);

	extern "C" void RayGenerationEditorView(int current_frame_index, const RenderSetting & render_setting, Scene * scene_device, RayCounter * ray_counter_device, uint32_t next_tile_index, uint32_t tile_count_this_batch, uint32_t width, uint32_t height, uint32_t tile_count_x, uint32_t tile_count_y, ShadingRayData & shading_ray_data, cudaStream_t & stream);
	
	extern "C" void RayTraceEditorView(int current_frame_index, const RenderSetting & render_setting, Scene * scene_device, RayCounter * ray_counter_device, RayCounter * ray_counter_host, uint32_t width, uint32_t height, glm::vec4 * beauty, uint32_t * primitive_index, ShadingRayData & shading_ray_data, HitData & hit_data, cudaStream_t & stream);
	
	extern "C" void RayShadingEditorView(int current_frame_index, const RenderSetting & render_setting, Scene * scene_device, RayCounter * ray_counter_device, RayCounter * ray_counter_host, uint32_t width, uint32_t height, glm::vec4 * beauty, uint32_t * primitive_index, ShadingRayData & shading_ray_data, HitData & hit_data, ShadowRayData & shadow_ray_data, cudaStream_t & stream);
	
	extern "C" void RayGenerationPathTracing(int current_frame_index, const RenderSetting & render_setting, Scene * scene_device, HaltonEnumerator * halton_enumerator_device, RayCounter * ray_counter_device, uint32_t next_tile_index, uint32_t tile_count_this_batch, uint32_t width, uint32_t height, uint32_t tile_count_x, uint32_t tile_count_y, ShadingRayData & shading_ray_data, cudaStream_t & stream);

	extern "C" void RayTracePathTracing(int current_frame_index, const RenderSetting & render_setting, Scene * scene_device, RayCounter * ray_counter_device, RayCounter * ray_counter_host, uint32_t width, uint32_t height, glm::vec4 * noise_image, ShadingRayData & shading_ray_data, HitData & hit_data, cudaStream_t & stream);

	extern "C" void RayShadingPathTracing(int current_frame_index, const RenderSetting & render_setting, Scene * scene_device, RayCounter * ray_counter_device, RayCounter * ray_counter_host, uint32_t width, uint32_t height, glm::vec4 * noise_image, ShadingRayData & shading_ray_data, HitData & hit_data, ShadowRayData & shadow_ray_data, cudaStream_t & stream);

	extern "C" void AccumulateImagePathTracing(const glm::vec4 * noise_image, glm::vec4 * accumulated_image, uint32_t width, uint32_t height, int frame_index, const int max_accumulate_frame, cudaStream_t & stream);

	extern "C" void PostProcessingEditorView(glm::vec4 * image, uint32_t * primitive_index, uint32_t width, uint32_t height, uint32_t clicked_primitive_index, float gamma, float exposure, bool draw_selected_effect, cudaStream_t & stream);

	extern "C" void PostProcessingPathTracing(glm::vec4 * image, uint32_t width, uint32_t height, float gamma, float exposure, cudaStream_t & stream);

	RenderModule::RenderModule(): 
		m_editor_view_width(16), m_editor_view_height(16), editor_view_primitive_index_image(), 
		editor_view_image_fetch_over_time(true), path_tracing_image_fetch_over_time(true),
		m_path_tracing_width(16), m_path_tracing_height(16), 
		editor_view_frame_index{ 1 }, path_tracing_frame_index{ 1 },
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
		AllocateHitData(editor_view_hit_data);
		AllocateShadowRayData(editor_view_shadow_ray_data);

		editor_view_primitive_index_image.Resize(m_editor_view_width, m_editor_view_height);
		editor_view_image_resources[aov_name_beauty] = DuskImageResource(m_editor_view_width, m_editor_view_height);
		editor_view_image_resources[aov_name_normal] = DuskImageResource(m_editor_view_width, m_editor_view_height);
		editor_view_image_resources[aov_name_post_processing] = DuskImageResource(m_editor_view_width, m_editor_view_height);
		for (auto& item : editor_view_image_resources) {
			item.second.Init();
		}

		AllocateRayCounter(path_tracing_ray_counter_data);
		AllocateShadingRayData(path_tracing_shading_ray_data);
		AllocateHitData(path_tracing_hit_data);
		AllocateShadowRayData(path_tracing_shadow_ray_data);

		path_tracing_image_resources[aov_name_beauty] = DuskImageResource(m_path_tracing_width, m_path_tracing_height);
		path_tracing_image_resources[aov_name_accumulate] = DuskImageResource(m_path_tracing_width, m_path_tracing_height);
		path_tracing_image_resources[aov_name_post_processing] = DuskImageResource(m_path_tracing_width, m_path_tracing_height);
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
		ReleaseHitData(editor_view_hit_data);
		ReleaseShadowRayData(editor_view_shadow_ray_data);

		ReleaseRayCounter(path_tracing_ray_counter_data);
		ReleaseShadingRayData(path_tracing_shading_ray_data);
		ReleaseHitData(path_tracing_hit_data);
		ReleaseShadowRayData(path_tracing_shadow_ray_data);

		CUDA_CHECK(cudaStreamDestroy(stream_editor_view));
		CUDA_CHECK(cudaStreamDestroy(stream_path_tracing));
		CUDA_CHECK(cudaStreamDestroy(stream_main));

		editor_view_task_queue.clear();
		editor_view_result_queue_albedo.clear();
		editor_view_result_queue_primitive_index.clear();
		editor_view_result_queue_extra.clear();
		for (auto& item : editor_view_image_resources) {
			item.second.Destroy();
		}
		editor_view_image_resources.clear(); // note: primitive index image will auto release.

		path_tracing_task_queue.clear();
		path_tracing_result_queue_beauty.clear();
		path_tracing_result_queue_extra.clear();
		for (auto& item : path_tracing_image_resources) {
			item.second.Destroy();
		}
		path_tracing_image_resources.clear();
	}

	void RenderModule::EditorViewFetchResult(SceneResource &scene_resource, const int scene_updated, const RenderSetting &render_setting, const int frame_width, const int frame_height, const uint32_t clicked_primitive_index, bool draw_selected_effect, ExtraTaskResults *extra_task_results)
	{
		EditorViewLaunchTask(TaskParams(&scene_resource, frame_width, frame_height, scene_updated, render_setting, scene_resource.scene_change_time));

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
		auto iter_beauty = editor_view_image_resources.find(aov_name_beauty); 
		auto iter_postprocessing = editor_view_image_resources.find(aov_name_post_processing);
		if (iter_beauty != editor_view_image_resources.end() && iter_postprocessing != editor_view_image_resources.end()) {
			auto& beauty_image = iter_beauty->second;
			auto& postprocessing_image = iter_postprocessing->second;
			auto& primitive_image = editor_view_primitive_index_image;
			
			std::unique_lock<std::mutex> editor_view_result_queue_lk(editor_view_result_queue_mutex);

			bool fetch_success = editor_view_result_queue_cv.wait_for(editor_view_result_queue_lk,
				std::chrono::milliseconds(MAX_WAIT_TIME),
				[&]() {return !editor_view_result_queue_albedo.empty(); });

			editor_view_image_fetch_over_time = !fetch_success;
			
			if (fetch_success) {
				assert(!editor_view_result_queue_primitive_index.empty() && !editor_view_result_queue_extra.empty());
				
				auto device_mem_albedo = std::move(editor_view_result_queue_albedo.back());
				editor_view_result_queue_albedo.pop_back();
				auto device_mem_primitive_index = std::move(editor_view_result_queue_primitive_index.back());
				editor_view_result_queue_primitive_index.pop_back();
				auto extra_frame_results = editor_view_result_queue_extra.back();
				editor_view_result_queue_extra.pop_back();

				editor_view_result_queue_lk.unlock();

				if (extra_task_results != nullptr) {
					*extra_task_results = extra_frame_results;
				}

				assert(device_mem_albedo.GetMemPtr() != nullptr && device_mem_primitive_index.GetMemPtr() != nullptr);

				assert(device_mem_albedo.width == beauty_image.image_width && device_mem_albedo.height == beauty_image.image_height);
				CUDA_CHECK(cudaMemcpyAsync(beauty_image.GetDevicePtr(), device_mem_albedo.GetMemPtr(), sizeof(glm::vec4) * frame_width * frame_height, cudaMemcpyDeviceToDevice, stream_main));
				assert(device_mem_primitive_index.width == primitive_image.width && device_mem_primitive_index.height == primitive_image.height);
				CUDA_CHECK(cudaMemcpyAsync(primitive_image.GetMemPtr(), device_mem_primitive_index.GetMemPtr(), sizeof(uint32_t) * frame_width * frame_height, cudaMemcpyDeviceToDevice, stream_main));
				
				CUDA_CHECK(cudaMemcpyAsync(postprocessing_image.GetDevicePtr(), beauty_image.GetDevicePtr(), sizeof(glm::vec4) * frame_width * frame_height, cudaMemcpyDeviceToDevice, stream_main));
				PostProcessingEditorView(postprocessing_image.GetDevicePtr(), primitive_image.GetMemPtr(), frame_width, frame_height, clicked_primitive_index, render_setting.gamma, render_setting.exposure, draw_selected_effect, stream_main);
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
			if (editor_view_task_queue.size() >= MAX_TASK_QUEUE_COUNT) {
				editor_view_task_queue.pop_back();
			}

			editor_view_task_queue.push_front(task_params);
		}
		editor_view_task_queue_cv.notify_one();
	}

	void RenderModule::PathTracingFetchResult(SceneResource &scene_resource,  const int scene_updated, const RenderSetting &render_setting, const int frame_width, const int frame_height, ExtraTaskResults *extra_task_results)
	{
		PathTracingLaunchTask(TaskParams(&scene_resource, frame_width, frame_height, scene_updated, render_setting, scene_resource.scene_change_time));

		if (m_path_tracing_width != frame_width || m_path_tracing_height != frame_height) {
			m_path_tracing_width = frame_width, m_path_tracing_height = frame_height;
			for (auto& resource : path_tracing_image_resources) {
				resource.second.Destroy();
				resource.second.ResetSize(frame_width, frame_height);
				resource.second.Init();
			}
		}

		auto iter_noise_image = path_tracing_image_resources.find(aov_name_beauty);
		auto iter_accumulate_image = path_tracing_image_resources.find(aov_name_accumulate);
		auto iter_postprocessing = path_tracing_image_resources.find(aov_name_post_processing);
		if (iter_noise_image != path_tracing_image_resources.end() && iter_accumulate_image != path_tracing_image_resources.end() && iter_postprocessing != path_tracing_image_resources.end()) {
			auto& noise_image = iter_noise_image->second;
			auto& accumulate_image = iter_accumulate_image->second;
			auto& postprocessing_image = iter_postprocessing->second;

			std::unique_lock<std::mutex> path_tracing_result_queue_lk(path_tracing_result_queue_mutex);

			bool fetch_success = path_tracing_result_queue_cv.wait_for(path_tracing_result_queue_lk,
				std::chrono::milliseconds(MAX_WAIT_TIME),
				[&]() {return !path_tracing_result_queue_beauty.empty(); });

			path_tracing_image_fetch_over_time = !fetch_success;

			if (fetch_success) {
				assert(!path_tracing_result_queue_extra.empty());

				// note: this memory should release after copy complete.
				auto device_mem = std::move(path_tracing_result_queue_beauty.back());
				path_tracing_result_queue_beauty.pop_back();
				auto extra_frame_results = path_tracing_result_queue_extra.back();
				path_tracing_result_queue_extra.pop_back();

				path_tracing_result_queue_lk.unlock();

				if (extra_task_results != nullptr) {
					*extra_task_results = extra_frame_results;
				}

				// note: execute function may pass empty frame when reach max accumulate count.
				if (device_mem.GetMemPtr() != nullptr) {
					assert(device_mem.width == frame_width && device_mem.height == frame_height);
					CUDA_CHECK(cudaMemcpyAsync(noise_image.GetDevicePtr(), device_mem.GetMemPtr(), sizeof(glm::vec4) * frame_width * frame_height, cudaMemcpyDeviceToDevice, stream_main));
					AccumulateImagePathTracing(noise_image.GetDevicePtr(), accumulate_image.GetDevicePtr(), frame_width, frame_height, extra_frame_results.task_frame_index, extra_frame_results.max_accumulate_frame, stream_main);
				}

				CUDA_CHECK(cudaMemcpyAsync(postprocessing_image.GetDevicePtr(), accumulate_image.GetDevicePtr(), sizeof(glm::vec4) * frame_width * frame_height, cudaMemcpyDeviceToDevice, stream_main));
				PostProcessingPathTracing(postprocessing_image.GetDevicePtr(), frame_width, frame_height, render_setting.gamma, render_setting.exposure, stream_main);
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

	glm::vec4* RenderModule::PathTracingGetDevicePointer(const std::string& name, int &image_width, int &image_height)
	{
		auto iter = path_tracing_image_resources.find(name);
		if (iter == path_tracing_image_resources.end()) {
			image_width = 0, image_height = 0;
			return nullptr;
		}
		image_width = iter->second.image_width, image_height = iter->second.image_height;
		return iter->second.GetDevicePtr();
	}

	void RenderModule::PathTracingLaunchTask(const TaskParams& task_params)
	{
		{
			std::unique_lock<std::mutex> path_tracing_task_queue_lk(path_tracing_task_queue_mutex);
			if (path_tracing_task_queue.size() >= MAX_TASK_QUEUE_COUNT) {
				path_tracing_task_queue.pop_back(); // note: drop the oldest task, tasks are identical when the scene and render size keep unchanged.
			}

			path_tracing_task_queue.push_front(task_params);
		}
		path_tracing_task_queue_cv.notify_one();
	}

	void RenderModule::EditorViewExecuteTask(TaskParams& task_param)
	{
		auto& scene_resource = *(task_param.scene_resource_ptr);

		if (task_param.scene_updated) {
			editor_view_frame_index.store(1);
		}

		DuskTimer editor_view_timer;

		editor_view_timer.Start();

		// note: allocate image buffers.
		glm::vec4 *albedo_ptr = nullptr; 
		uint32_t *primitive_index_ptr = nullptr;
		CUDA_CHECK(cudaMallocAsync(&albedo_ptr, sizeof(glm::vec4) * task_param.width * task_param.height, stream_editor_view));
		CUDA_CHECK(cudaMemsetAsync(albedo_ptr, 0, sizeof(glm::vec4) * task_param.width * task_param.height, stream_editor_view));
		CUDA_CHECK(cudaMallocAsync(&primitive_index_ptr, sizeof(uint32_t) * task_param.width * task_param.height, stream_editor_view));
		CUDA_CHECK(cudaMemsetAsync(primitive_index_ptr, 0xFFFFFFFF, sizeof(uint32_t) * task_param.width * task_param.height, stream_editor_view));
		CUDA_CHECK(cudaStreamSynchronize(stream_editor_view));
		DuskDeviceMemory<glm::vec4> device_mem_albedo(task_param.width, task_param.height, albedo_ptr);
		DuskDeviceMemory<uint32_t>  device_mem_primitive_indices(task_param.width, task_param.height, primitive_index_ptr);

		// note: allocate device scene ptrs.
		Scene *scene_device = nullptr;
		{
			std::shared_lock<std::shared_mutex> scene_read_lk(scene_resource.scene_mutex);
			if (scene_resource.scene_change_time != task_param.scene_change_time) {
				return;
			}
			
			CUDA_CHECK(cudaMallocAsync(&scene_device, sizeof(Scene), stream_editor_view));
			CUDA_CHECK(cudaMemcpyAsync(scene_device, &scene_resource.scene, sizeof(Scene), cudaMemcpyHostToDevice, stream_editor_view));
			CUDA_CHECK(cudaStreamSynchronize(stream_editor_view)); // note: have to make sure the gpu done the memory copy before unlock.
		}
		
		const RenderSetting& render_setting = task_param.render_setting;
		const int current_frame_index = editor_view_frame_index.load();
		const uint32_t tile_count_x = (task_param.width + TILE_X_RES - 1) / TILE_X_RES;
		const uint32_t tile_count_y = (task_param.height + TILE_Y_RES - 1) / TILE_Y_RES;
		const uint32_t total_tile_count = tile_count_x * tile_count_y;
		
		uint32_t next_tile_index = 0;

		auto& ray_counter_host = editor_view_ray_counter_data.ray_counter_host;
		auto& ray_counter_device = editor_view_ray_counter_data.ray_counter_device;

		// note: clear ray counter.
		ray_counter_host->hit_counter = 0;
		ray_counter_host->shading_ray_counter = 0;
		ray_counter_host->shadow_ray_counter = 0;
		CUDA_CHECK(cudaMemcpyAsync(ray_counter_device, ray_counter_host, sizeof(RayCounter), cudaMemcpyHostToDevice, stream_editor_view));
		CUDA_CHECK(cudaStreamSynchronize(stream_editor_view));

		while (ray_counter_host->shading_ray_counter > 0 || next_tile_index < total_tile_count) 
		{
			// stage: ray generation.
			const uint32_t tile_count_this_batch = (MAX_RAY_COUNT - ray_counter_host->shading_ray_counter) / TILE_PIXEL_COUNT;
			{
				std::shared_lock<std::shared_mutex> scene_read_lk(scene_resource.scene_mutex);
				if (scene_resource.scene_change_time != task_param.scene_change_time) {
					break;
				}
				RayGenerationEditorView(current_frame_index, render_setting, scene_device, ray_counter_device, next_tile_index, tile_count_this_batch, task_param.width, task_param.height, tile_count_x, tile_count_y, editor_view_shading_ray_data, stream_editor_view);
			}

			next_tile_index += tile_count_this_batch;
			ray_counter_host->shading_ray_counter += tile_count_this_batch * TILE_PIXEL_COUNT;
			CUDA_CHECK(cudaMemcpyAsync(ray_counter_device, ray_counter_host, sizeof(RayCounter), cudaMemcpyHostToDevice, stream_editor_view));
			CUDA_CHECK(cudaStreamSynchronize(stream_editor_view));

			// stage: ray trace.
			{
				std::shared_lock<std::shared_mutex> scene_read_lk(scene_resource.scene_mutex);
				if (scene_resource.scene_change_time != task_param.scene_change_time) {
					break;
				}
				RayTraceEditorView(current_frame_index, render_setting, scene_device, ray_counter_device, ray_counter_host, task_param.width, task_param.height, device_mem_albedo.GetMemPtr(), device_mem_primitive_indices.GetMemPtr(), editor_view_shading_ray_data, editor_view_hit_data, stream_editor_view);
			}

			CUDA_CHECK(cudaMemcpyAsync(ray_counter_host, ray_counter_device, sizeof(RayCounter), cudaMemcpyDeviceToHost, stream_editor_view));
			CUDA_CHECK(cudaStreamSynchronize(stream_editor_view));
			ray_counter_host->shading_ray_counter = 0;
			CUDA_CHECK(cudaMemcpyAsync(ray_counter_device, ray_counter_host, sizeof(RayCounter), cudaMemcpyHostToDevice, stream_editor_view));
			CUDA_CHECK(cudaStreamSynchronize(stream_editor_view));

			// stage: ray shading.
			{
				std::shared_lock<std::shared_mutex> scene_read_lk(scene_resource.scene_mutex);
				if (scene_resource.scene_change_time != task_param.scene_change_time) {
					break;
				}
				RayShadingEditorView(current_frame_index, render_setting, scene_device, ray_counter_device, ray_counter_host, task_param.width, task_param.height, device_mem_albedo.GetMemPtr(), device_mem_primitive_indices.GetMemPtr(), editor_view_shading_ray_data, editor_view_hit_data, editor_view_shadow_ray_data, stream_editor_view);
			}

			CUDA_CHECK(cudaMemcpyAsync(ray_counter_host, ray_counter_device, sizeof(RayCounter), cudaMemcpyDeviceToHost, stream_editor_view));
			CUDA_CHECK(cudaStreamSynchronize(stream_editor_view));
			ray_counter_host->hit_counter = 0;
			CUDA_CHECK(cudaMemcpyAsync(ray_counter_device, ray_counter_host, sizeof(RayCounter), cudaMemcpyHostToDevice, stream_editor_view));
			CUDA_CHECK(cudaStreamSynchronize(stream_editor_view));

			// TODO: stage ray shadow.
			{
			
			}
		}

		FREE_GPU_RESOURCE(scene_device);

		// stage: result output to queue.
		{
			std::shared_lock<std::shared_mutex> scene_read_lk(scene_resource.scene_mutex);
			if (scene_resource.scene_change_time != task_param.scene_change_time) {
				return;
			}

			{
				// push result to queue.
				std::unique_lock<std::mutex> editor_view_result_queue_lk(editor_view_result_queue_mutex);
				editor_view_result_queue_albedo.push_front(std::move(device_mem_albedo));
				editor_view_result_queue_primitive_index.push_front(std::move(device_mem_primitive_indices));
				editor_view_result_queue_extra.push_front({ current_frame_index, editor_view_timer.Stop(), render_setting.max_frame_count });
				editor_view_frame_index.fetch_add(1);
			}
			editor_view_result_queue_cv.notify_one();
		}
	}

	void RenderModule::PathTracingExecuteTask(TaskParams& task_param)
	{
		auto& scene_resource = *(task_param.scene_resource_ptr);

		if (task_param.scene_updated) {
			path_tracing_frame_index.store(1);
		}

		DuskTimer path_tracing_timer;

		path_tracing_timer.Start();

		const RenderSetting& render_setting = task_param.render_setting;
		const int current_frame_index = path_tracing_frame_index.load();
		const int accumulated_frame_count = current_frame_index - 1;

		// note: frame count reached the limit, skip the ray tracing work and push a black frame to keep the pipeline flowing.
		if (render_setting.max_frame_count > 0 && accumulated_frame_count >= render_setting.max_frame_count) 
		{
			{
				DuskDeviceMemory<glm::vec4> dummy_device_mem;
				assert(dummy_device_mem.GetMemPtr() == nullptr);

				std::shared_lock<std::shared_mutex> scene_read_lk(scene_resource.scene_mutex);
				if (scene_resource.scene_change_time != task_param.scene_change_time) 
				{
					return;
				}

				{
					std::unique_lock<std::mutex> path_tracing_result_queue_lk(path_tracing_result_queue_mutex);
					path_tracing_result_queue_beauty.push_front(std::move(dummy_device_mem));
					path_tracing_result_queue_extra.push_front({ current_frame_index, path_tracing_timer.Stop(), render_setting.max_frame_count });
					path_tracing_frame_index.fetch_add(1);
				}
				path_tracing_result_queue_cv.notify_one();
			}
			return;
		}

		// note: allocate image buffers.
		glm::vec4* noise_image = nullptr;
		CUDA_CHECK(cudaMallocAsync(&noise_image, sizeof(glm::vec4) * task_param.width * task_param.height, stream_path_tracing));
		CUDA_CHECK(cudaMemsetAsync(noise_image, 0, sizeof(glm::vec4) * task_param.width * task_param.height, stream_path_tracing));
		CUDA_CHECK(cudaStreamSynchronize(stream_path_tracing));
		DuskDeviceMemory<glm::vec4> device_mem_noise_image(task_param.width, task_param.height, noise_image);

		

		// note: allocate device scene ptrs.
		Scene *scene_device = nullptr;
		HaltonEnumerator *halton_enumerator_device = nullptr;
		{
			std::shared_lock<std::shared_mutex> scene_read_lk(scene_resource.scene_mutex);
			if (scene_resource.scene_change_time != task_param.scene_change_time) {
				return;
			}

			CUDA_CHECK(cudaMallocAsync(&scene_device, sizeof(Scene), stream_path_tracing));
			CUDA_CHECK(cudaMemcpyAsync(scene_device, &scene_resource.scene, sizeof(Scene), cudaMemcpyHostToDevice, stream_path_tracing));

			HaltonEnumerator halton_enumerator(task_param.width, task_param.height);

			CUDA_CHECK(cudaMallocAsync(&halton_enumerator_device, sizeof(HaltonEnumerator), stream_path_tracing));
			CUDA_CHECK(cudaMemcpyAsync(halton_enumerator_device, &halton_enumerator, sizeof(HaltonEnumerator), cudaMemcpyHostToDevice, stream_path_tracing));

			CUDA_CHECK(cudaStreamSynchronize(stream_path_tracing)); // note: have to make sure the gpu done the memory copy before unlock.
		}

		const uint32_t sample_per_pixel = render_setting.ssp;
		const uint32_t tile_count_x = (task_param.width + TILE_X_RES - 1) / TILE_X_RES;
		const uint32_t tile_count_y = (task_param.height + TILE_Y_RES - 1) / TILE_Y_RES;
		const uint32_t total_tile_count = tile_count_x * tile_count_y;

		uint32_t next_tile_index = 0;

		auto& ray_counter_host = path_tracing_ray_counter_data.ray_counter_host;
		auto& ray_counter_device = path_tracing_ray_counter_data.ray_counter_device;

		// note: clear ray counter.
		ray_counter_host->hit_counter = 0;
		ray_counter_host->shading_ray_counter = 0;
		ray_counter_host->shadow_ray_counter = 0;
		CUDA_CHECK(cudaMemcpyAsync(ray_counter_device, ray_counter_host, sizeof(RayCounter), cudaMemcpyHostToDevice, stream_path_tracing));
		CUDA_CHECK(cudaStreamSynchronize(stream_path_tracing));

		while (ray_counter_host->shading_ray_counter > 0 || next_tile_index < total_tile_count)
		{
			// stage: ray generation.
			const uint32_t tile_count_this_batch = (MAX_RAY_COUNT - ray_counter_host->shading_ray_counter) / (TILE_PIXEL_COUNT * sample_per_pixel);
			{
				std::shared_lock<std::shared_mutex> scene_read_lk(scene_resource.scene_mutex);
				if (scene_resource.scene_change_time != task_param.scene_change_time) {
					break;
				}
				RayGenerationPathTracing(current_frame_index, render_setting, scene_device, halton_enumerator_device, ray_counter_device, next_tile_index, tile_count_this_batch, task_param.width, task_param.height, tile_count_x, tile_count_y, path_tracing_shading_ray_data, stream_path_tracing);
			}

			next_tile_index += tile_count_this_batch;
			ray_counter_host->shading_ray_counter += tile_count_this_batch * sample_per_pixel * TILE_PIXEL_COUNT;
			CUDA_CHECK(cudaMemcpyAsync(ray_counter_device, ray_counter_host, sizeof(RayCounter), cudaMemcpyHostToDevice, stream_path_tracing));
			CUDA_CHECK(cudaStreamSynchronize(stream_path_tracing));

			// stage: ray trace.
			{
				std::shared_lock<std::shared_mutex> scene_read_lk(scene_resource.scene_mutex);
				if (scene_resource.scene_change_time != task_param.scene_change_time) {
					break;
				}
				RayTracePathTracing(current_frame_index, render_setting, scene_device, ray_counter_device, ray_counter_host, task_param.width, task_param.height, device_mem_noise_image.GetMemPtr(), path_tracing_shading_ray_data, path_tracing_hit_data, stream_path_tracing);
			}

			CUDA_CHECK(cudaMemcpyAsync(ray_counter_host, ray_counter_device, sizeof(RayCounter), cudaMemcpyDeviceToHost, stream_path_tracing));
			CUDA_CHECK(cudaStreamSynchronize(stream_path_tracing));
			ray_counter_host->shading_ray_counter = 0;
			CUDA_CHECK(cudaMemcpyAsync(ray_counter_device, ray_counter_host, sizeof(RayCounter), cudaMemcpyHostToDevice, stream_path_tracing));
			CUDA_CHECK(cudaStreamSynchronize(stream_path_tracing));

			// stage: ray shading.
			{
				std::shared_lock<std::shared_mutex> scene_read_lk(scene_resource.scene_mutex);
				if (scene_resource.scene_change_time != task_param.scene_change_time) {
					break;
				}
				RayShadingPathTracing(current_frame_index, render_setting, scene_device, ray_counter_device, ray_counter_host, task_param.width, task_param.height, device_mem_noise_image.GetMemPtr(), path_tracing_shading_ray_data, path_tracing_hit_data, path_tracing_shadow_ray_data, stream_path_tracing);
			}

			CUDA_CHECK(cudaMemcpyAsync(ray_counter_host, ray_counter_device, sizeof(RayCounter), cudaMemcpyDeviceToHost, stream_path_tracing));
			CUDA_CHECK(cudaStreamSynchronize(stream_path_tracing));
			ray_counter_host->hit_counter = 0;
			CUDA_CHECK(cudaMemcpyAsync(ray_counter_device, ray_counter_host, sizeof(RayCounter), cudaMemcpyHostToDevice, stream_path_tracing));
			CUDA_CHECK(cudaStreamSynchronize(stream_path_tracing));

			// TODO: stage ray shadow.
			{

			}
		}

		FREE_GPU_RESOURCE(scene_device);
		FREE_GPU_RESOURCE(halton_enumerator_device);

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
				path_tracing_result_queue_beauty.push_front(std::move(device_mem_noise_image));
				path_tracing_result_queue_extra.push_front({ current_frame_index, path_tracing_timer.Stop(), render_setting.max_frame_count });
				path_tracing_frame_index.fetch_add(1);
			}
			path_tracing_result_queue_cv.notify_one();
		}
	}
};