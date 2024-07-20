#include "src/core/GuiModule.h"

namespace YumeRT {
	static void KeyBoardCallBack(GLFWwindow* window, int key, int scancode, int action, int mods) {
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, true);
		}
	}

	static void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
	{
		GuiModule &module_gui = *((GuiModule*)glfwGetWindowUserPointer(window));
		module_gui.SetWindowWidth(width);
		module_gui.SetWindowHeight(height);
	}

	GuiModule::GuiModule(GLFWwindow* window,
		int width, int height,
		std::shared_ptr<SceneModule> &module_scene,
		std::shared_ptr<RenderModule> &module_render) :
	m_window(window), m_width(width), m_height(height), m_module_render(module_render), m_module_scene(module_scene), trackball(module_scene->GetCamera(EDITOR_CAMERA_INDEX))
	{
		glfwSetWindowUserPointer(m_window, this);
		glfwSetFramebufferSizeCallback(m_window, FramebufferSizeCallback);
		glfwSetKeyCallback(m_window, KeyBoardCallBack);

		InitGUI();
	}

	GuiModule::~GuiModule() {
		ExitGUI();
	}

	void GuiModule::UpdateScene()
	{
		auto& m_renderer = (*m_module_render);
		auto& m_scene = (*m_module_scene);
		if (!m_scene.SceneChanged()) {
			return;
		}

		// host data update.
		if (m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_CAMERA_CHANGE)) {
			// restore rendering camera's width, height, aspect ratio.
			const int camera_width = m_scene.camera[RENDERING_CAMERA_INDEX].m_width, 
				camera_height = m_scene.camera[RENDERING_CAMERA_INDEX].m_height;
			memcpy(&m_scene.camera[RENDERING_CAMERA_INDEX], &m_scene.camera[EDITOR_CAMERA_INDEX], sizeof(Camera));
			m_scene.camera[RENDERING_CAMERA_INDEX].m_width = camera_width, 
				m_scene.camera[RENDERING_CAMERA_INDEX].m_height = camera_height;
			m_scene.camera[RENDERING_CAMERA_INDEX].SetAspectRatio(float(camera_width) / float(camera_height));
		}

		// device data update and task clear: need lock.
		{
			auto& scene_resource = m_scene.scene_resource;
			std::scoped_lock lk(scene_resource.scene_mutex,
				m_renderer.editor_view_task_queue_mutex, m_renderer.editor_view_result_queue_mutex,
				m_renderer.path_tracing_task_queue_mutex, m_renderer.path_tracing_result_queue_mutex);

			++scene_resource.scene_change_time;

			m_renderer.editor_view_task_queue.clear();
			m_renderer.editor_view_result_queue.clear();
			m_renderer.path_tracing_task_queue.clear();
			m_renderer.path_tracing_result_queue.clear();

			if (m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_CAMERA_CHANGE)) {
				auto& scene_device_data = scene_resource.scene;
				if (scene_device_data.camera == nullptr) {
					scene_device_data.camera_count = CAMERA_COUNT;
					UPLOAD_TO_GPU(scene_device_data.camera, m_scene.camera, sizeof(Camera) * CAMERA_COUNT);
				}
				else {
					TRANSFER_TO_GPU(scene_device_data.camera, m_scene.camera, sizeof(Camera) * CAMERA_COUNT);
				}
			}
		}

		// reset scene flag.
		m_scene.ResetSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_NONE_CHANGE);
	}

	void GuiModule::UpdateCamera(int index)
	{
		auto& m_renderer = (*m_module_render);
		auto& m_scene = (*m_module_scene);

		// re-upload one of the camera.
		{
			auto& scene_resource = m_scene.scene_resource;
			assert(scene_resource.scene.camera != nullptr && scene_resource.scene.camera_count != 0);

			std::scoped_lock lk(scene_resource.scene_mutex,
				m_renderer.editor_view_task_queue_mutex, m_renderer.editor_view_result_queue_mutex,
				m_renderer.path_tracing_task_queue_mutex, m_renderer.path_tracing_result_queue_mutex);

			++scene_resource.scene_change_time;

			m_renderer.editor_view_task_queue.clear();
			m_renderer.editor_view_result_queue.clear();
			m_renderer.path_tracing_task_queue.clear();
			m_renderer.path_tracing_result_queue.clear();

			TRANSFER_TO_GPU(&scene_resource.scene.camera[index], &m_scene.camera[index], sizeof(Camera));
		}
	}

	void GuiModule::RenderImages()
	{
		ImGuiIO& io = ImGui::GetIO();

		UpdateScene();

		// render editor view. such an window implementation can be found in imgui custom rendering example.
		{
			ImGui::Begin("Editor view");

			// specify the render region.
			ImVec2 draw_region_size = ImGui::GetContentRegionAvail();
			ImVec2 draw_region_p0 = ImGui::GetCursorScreenPos();
			ImVec2 draw_region_p1 = ImVec2(draw_region_p0.x + draw_region_size.x, draw_region_p0.y + draw_region_size.y);

			draw_region_size.x = glm::max(64.0f, draw_region_size.x);
			draw_region_size.y = glm::max(64.0f, draw_region_size.y);

			const float mouse_x_pos = glm::max(0.0f, glm::min(io.MousePos.x - draw_region_p0.x, draw_region_size.x));
			const float mouse_y_pos = glm::max(0.0f, glm::min(io.MousePos.y - draw_region_p0.y, draw_region_size.y));

			auto &editor_camera = m_module_scene->GetCamera(EDITOR_CAMERA_INDEX);
			if (editor_camera.m_width != (int)draw_region_size.x || editor_camera.m_height != (int)draw_region_size.y)
			{
				editor_camera.m_width = (int)draw_region_size.x, editor_camera.m_height = (int)draw_region_size.y;
				editor_camera.SetAspectRatio(float(editor_camera.m_width) / float(editor_camera.m_height));
				UpdateCamera(EDITOR_CAMERA_INDEX);
			}

			// render editor view based on current size.
			auto &scene_resource = m_module_scene->scene_resource;
			m_module_render->EditorViewFetchResult(scene_resource, (int)draw_region_size.x, (int)draw_region_size.y);

			// image button style.
			ImGui::PushID(1);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

			// draw image button.
			auto image_texture_handle = m_module_render->EditorViewGetTexture(aov_name_beauty);
			ImGui::ImageButton((void*)image_texture_handle, draw_region_size, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));

			const bool is_hovered = ImGui::IsItemHovered();
			const bool is_active = ImGui::IsItemActive();
			if (is_active && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				trackball.prev_pos_x = mouse_x_pos;
				trackball.prev_pos_y = mouse_y_pos;
			}
			if (is_active && ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				glm::vec3 pre_vec = trackball.CalculTrackBallVec(
					trackball.prev_pos_x,
					trackball.prev_pos_y,
					(int)draw_region_size.x,
					(int)draw_region_size.y);

				glm::vec3 cur_vec = trackball.CalculTrackBallVec(
					mouse_x_pos,
					mouse_y_pos,
					(int)draw_region_size.x,
					(int)draw_region_size.y);

				if (glm::abs(glm::dot(pre_vec, cur_vec)) < 0.999f)
				{
					double axis[3];
					DoubleCross(pre_vec, cur_vec, axis);

					const float angle = glm::acos(glm::min(glm::dot(pre_vec, cur_vec), 1.0f));
					trackball.Rotate(angle, glm::vec3(axis[0], axis[1], axis[2]));
					trackball.prev_pos_x = float(mouse_x_pos);
					trackball.prev_pos_y = float(mouse_y_pos);
				}

				m_module_scene->AddSceneFlag(SceneModule::SCENE_CAMERA_CHANGE);
			}

			ImGui::PopStyleVar();
			ImGui::PopID();

			ImGui::End();
		}

		 // render path tracing view.
		{
			ImGui::Begin("Path tracing view");

			// specify the render region.
			ImVec2 draw_region_size = ImGui::GetContentRegionAvail();
			ImVec2 draw_region_p0 = ImGui::GetCursorScreenPos();
			ImVec2 draw_region_p1 = ImVec2(draw_region_p0.x + draw_region_size.x, draw_region_p0.y + draw_region_size.y);

			draw_region_size.x = glm::max(64.0f, draw_region_size.x);
			draw_region_size.y = glm::max(64.0f, draw_region_size.y);

			const float mouse_x_pos = glm::max(0.0f, glm::min(io.MousePos.x - draw_region_p0.x, draw_region_size.x));
			const float mouse_y_pos = glm::max(0.0f, glm::min(io.MousePos.y - draw_region_p0.y, draw_region_size.y));

			auto &rendering_camera = m_module_scene->GetCamera(RENDERING_CAMERA_INDEX);
			if (rendering_camera.m_width != (int)draw_region_size.x || rendering_camera.m_height != (int)draw_region_size.y)
			{
				rendering_camera.m_width = (int)draw_region_size.x, rendering_camera.m_height = (int)draw_region_size.y;
				rendering_camera.SetAspectRatio(float(rendering_camera.m_width) / float(rendering_camera.m_height));
				UpdateCamera(RENDERING_CAMERA_INDEX);
			}

			// render path tracing view based on current size.
			auto &scene_resource = m_module_scene->scene_resource;
			m_module_render->PathTracingFetchResult(scene_resource, (int)draw_region_size.x, (int)draw_region_size.y);

			// image button style.
			ImGui::PushID(1);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

			// draw image button.
			auto image_texture_handle = m_module_render->PathTracingGetTexture(aov_name_beauty);
			ImGui::ImageButton((void*)image_texture_handle, draw_region_size, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));

			ImGui::PopStyleVar();
			ImGui::PopID();

			ImGui::End();
		}
	}
	

	void GuiModule::Draw() {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGui::DockSpaceOverViewport();

		ImGuiIO& io = ImGui::GetIO();
		
		{
			RenderImages();
		}

		{
			ImGui::ShowDemoWindow();
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Multi-viewport: for OpenGL, need to restore opengl context.
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow *current_opengl_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(current_opengl_context);
		}
	}

	void GuiModule::InitGUI() {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		// use invisible button to do this. No, use image button.
		// io.ConfigWindowsMoveFromTitleBarOnly = true;

		ImGui::StyleColorsClassic();
		ImGuiStyle& imgui_style = ImGui::GetStyle();

		imgui_style.WindowRounding = 4;
		imgui_style.ChildRounding = 4;
		imgui_style.FrameRounding = 4;
		imgui_style.PopupRounding = 4;
		imgui_style.ScrollbarRounding = 8;
		imgui_style.GrabRounding = 4;
		imgui_style.LogSliderDeadzone = 4;
		imgui_style.TabRounding = 4;

		ImVec4* colors = imgui_style.Colors;
		colors[ImGuiCol_TextDisabled] = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);
		colors[ImGuiCol_Border] = ImVec4(0.54f, 0.30f, 0.30f, 0.50f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.36f, 0.36f, 0.36f, 0.39f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.44f, 0.32f, 0.31f, 0.40f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.22f, 0.21f, 0.69f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.07f, 0.07f, 0.10f, 0.83f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.36f, 0.26f, 0.27f, 0.87f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.38f, 0.26f, 0.26f, 0.20f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.31f, 0.31f, 0.34f, 0.80f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.40f, 0.39f, 0.39f, 0.60f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.93f, 0.93f, 0.98f, 0.30f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.80f, 0.43f, 0.40f, 0.40f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.80f, 0.40f, 0.39f, 0.60f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.80f, 0.43f, 0.39f, 0.60f);
		colors[ImGuiCol_Button] = ImVec4(0.43f, 0.43f, 0.43f, 0.62f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.99f, 0.46f, 0.42f, 0.79f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.57f, 0.46f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.74f, 0.74f, 0.74f, 0.45f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.97f, 0.57f, 0.40f, 0.80f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.99f, 0.24f, 0.19f, 0.80f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.90f, 0.56f, 0.49f, 1.00f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.90f, 0.74f, 0.70f, 1.00f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 0.83f, 0.78f, 0.60f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 0.79f, 0.78f, 0.90f);
		colors[ImGuiCol_Tab] = ImVec4(0.41f, 0.41f, 0.41f, 0.79f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.96f, 0.96f, 0.97f, 0.80f);
		colors[ImGuiCol_TabActive] = ImVec4(1.00f, 0.20f, 0.00f, 0.84f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.43f, 0.40f, 0.40f, 0.82f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.82f, 0.17f, 0.17f, 0.84f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.90f, 0.42f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.12f, 0.00f, 0.96f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.10f, 0.00f, 1.00f);
		colors[ImGuiCol_TableHeaderBg] = ImVec4(0.73f, 0.22f, 0.22f, 1.00f);
		colors[ImGuiCol_TableBorderStrong] = ImVec4(0.45f, 0.31f, 0.44f, 1.00f);
		colors[ImGuiCol_TableBorderLight] = ImVec4(0.43f, 0.24f, 0.41f, 1.00f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(1.00f, 0.20f, 0.00f, 0.35f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 0.00f, 0.70f, 0.90f);
		colors[ImGuiCol_NavHighlight] = ImVec4(0.90f, 0.45f, 0.45f, 0.80f);

		ImGui_ImplGlfw_InitForOpenGL(m_window, true);
		ImGui_ImplOpenGL3_Init("#version 450");
	}

	void GuiModule::ExitGUI() {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
};
