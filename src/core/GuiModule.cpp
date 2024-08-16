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
	m_window(window), m_width(width), m_height(height), 
		m_module_render(module_render), m_module_scene(module_scene), 
		trackball(module_scene->GetCamera(EDITOR_CAMERA_INDEX)), clicked_pixel_primitive_index(EMPTY_UINT32)
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
		auto& m_scene = (*m_module_scene);
		if (!m_scene.SceneChanged()) {
			return;
		}

		// host data update.
		const bool rebuild_bounding_volume_hierarchy = 
			m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_CREATE) ||
			m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_DELETE) ||
			m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_GEOMETRY_CHANGE) ||
			m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_TRANSFORM_CHANGE);
		if (rebuild_bounding_volume_hierarchy) {
			ACBVHBuilder SceneBuilder(m_scene.primitive_instances.data(), (uint32_t)m_scene.primitive_instances.size(), m_scene.transforms.data(), m_scene.geometries.data());
			m_scene.top_nodes.clear();
			SceneBuilder.BuildSceneBVH(m_scene.top_nodes, nullptr, nullptr);
		}

		const bool rebuild_light_list = 
			m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_SHAPE_LIGHT_CREATE) || 
			m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_SHAPE_LIGHT_DELETE) || 
			m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_SHAPE_LIGHT_CHANGE);
		if (rebuild_light_list) {
			m_scene.shape_lights.clear();
			m_scene.shape_light_sample_table.clear();
			m_scene.LoadShapeLight();
		}

		const bool init_sampler_data = (m_scene.scene_change_flag == SceneModule::SCENE_CHANGE_FLAG::SCENE_INIT);
		if (init_sampler_data) {
			m_scene.InitHaltonPermuteTable();
		}

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
		UpdateDeviceData([&]() {
			auto& scene_resource = m_scene.scene_resource;
			auto& scene_device_data = scene_resource.scene;
			if (m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_GEOMETRY_CREATE) || m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_GEOMETRY_DELETE)) {
				FREE_GPU_RESOURCE(scene_device_data.geometries);
				scene_device_data.geometry_count = (uint32_t)m_scene.geometries.size();
				UPLOAD_TO_GPU(scene_device_data.geometries, m_scene.geometries.data(), sizeof(GeometryData) * m_scene.geometries.size());
			}

			if (m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_TRANSFORM_CREATE) || m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_TRANSFORM_DELETE)) {
				FREE_GPU_RESOURCE(scene_device_data.transforms);
				FREE_GPU_RESOURCE(scene_device_data.i_transforms);
				scene_device_data.transform_count = (uint32_t)m_scene.transforms.size();
				UPLOAD_TO_GPU(scene_device_data.transforms, m_scene.transforms.data(), sizeof(glm::mat4) * m_scene.transforms.size());
				UPLOAD_TO_GPU(scene_device_data.i_transforms, m_scene.i_transforms.data(), sizeof(glm::mat4) * m_scene.i_transforms.size());
			}

			if (rebuild_bounding_volume_hierarchy) {
				FREE_GPU_RESOURCE(scene_device_data.top_nodes);
				scene_device_data.top_node_count = (uint32_t)m_scene.top_nodes.size();
				UPLOAD_TO_GPU(scene_device_data.top_nodes, m_scene.top_nodes.data(), sizeof(TopNode) * m_scene.top_nodes.size());

				FREE_GPU_RESOURCE(scene_device_data.primitive_instances);
				scene_device_data.primitive_instance_count = (uint32_t)m_scene.primitive_instances.size();
				UPLOAD_TO_GPU(scene_device_data.primitive_instances, m_scene.primitive_instances.data(), sizeof(PrimitiveInstance) * m_scene.primitive_instances.size());
			}

			if (m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_MATERIAL_CREATE) || m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_MATERIAL_DELETE)) {
				FREE_GPU_RESOURCE(scene_device_data.materials);
				scene_device_data.material_count = (uint32_t)m_scene.materials.size();
				UPLOAD_TO_GPU(scene_device_data.materials, m_scene.materials.data(), sizeof(Material) * m_scene.materials.size());
			}

			if (m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_TEXTURE_CREATE) || m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_TEXTURE_DELETE)) {
				FREE_GPU_RESOURCE(scene_device_data.textures);
				scene_device_data.texture_count = (uint32_t)m_scene.textures.size();
				UPLOAD_TO_GPU(scene_device_data.textures, m_scene.textures.data(), sizeof(Texture) * m_scene.textures.size());
			}

			if (m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_DISTANT_LIGHT_CREATE) || m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_DISTANT_LIGHT_DELETE)) {
				FREE_GPU_RESOURCE(scene_device_data.distant_lights);
				scene_device_data.distant_light_count = (uint32_t)m_scene.distant_lights.size();
				UPLOAD_TO_GPU(scene_device_data.distant_lights, m_scene.distant_lights.data(), sizeof(DistantLight) * m_scene.distant_lights.size());
			}

			if (rebuild_light_list) {
				FREE_GPU_RESOURCE(scene_device_data.shape_lights);
				FREE_GPU_RESOURCE(scene_device_data.shape_light_sample_table);
				scene_device_data.shape_light_count = (uint32_t)m_scene.shape_lights.size();
				UPLOAD_TO_GPU(scene_device_data.shape_lights, m_scene.shape_lights.data(), sizeof(ShapeLight) * m_scene.shape_lights.size());
				UPLOAD_TO_GPU(scene_device_data.shape_light_sample_table, m_scene.shape_light_sample_table.data(), sizeof(float) * m_scene.shape_light_sample_table.size());
			}

			if (m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_VOLUME_CREATE) || m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_VOLUME_DELETE)) {
				FREE_GPU_RESOURCE(scene_device_data.volumes);
				scene_device_data.volume_count = (uint32_t)m_scene.volumes.size();
				UPLOAD_TO_GPU(scene_device_data.volumes, m_scene.volumes.data(), sizeof(Volume) * m_scene.volumes.size());
			}

			if (init_sampler_data) {
				UPLOAD_TO_GPU(scene_device_data.sampler_data.halton_permute_table, m_scene.permute_table.data(), sizeof(uint32_t)* m_scene.permute_table.size());
				UPLOAD_TO_GPU(scene_device_data.sampler_data.sobol_matrices, sobol_matrices32, sizeof(uint32_t) * 4096u * 32u);
			}

			if (m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_RENDER_SETTING_CHANGE)) {
				if (scene_device_data.render_setting == nullptr) {
					UPLOAD_TO_GPU(scene_device_data.render_setting, &m_scene.render_setting, sizeof(RenderSetting));
				}
				else {
					TRANSFER_TO_GPU(scene_device_data.render_setting, &m_scene.render_setting, sizeof(RenderSetting));
				}
			}

			if (m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_CAMERA_CHANGE)) {
				if (scene_device_data.camera == nullptr) {
					scene_device_data.camera_count = CAMERA_COUNT;
					UPLOAD_TO_GPU(scene_device_data.camera, m_scene.camera, sizeof(Camera) * CAMERA_COUNT);
				}
				else {
					TRANSFER_TO_GPU(scene_device_data.camera, m_scene.camera, sizeof(Camera) * CAMERA_COUNT);
				}
			}
		});
		
		// reset scene flag.
		m_scene.ResetSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_NONE_CHANGE);
	}

	void GuiModule::UpdateCamera(int index)
	{
		auto& m_scene = (*m_module_scene);

		// re-upload one of the camera.
		UpdateDeviceData([&]() {
			auto& scene_resource = m_scene.scene_resource;
			TRANSFER_TO_GPU(&scene_resource.scene.camera[index], &m_scene.camera[index], sizeof(Camera));
		});
	}

	void GuiModule::RenderImages()
	{
		ImGuiIO& io = ImGui::GetIO();

		UpdateScene();

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
			auto& scene_resource = m_module_scene->scene_resource;
			auto& render_setting = m_module_scene->render_setting;
			m_module_render->PathTracingFetchResult(scene_resource, render_setting, (int)draw_region_size.x, (int)draw_region_size.y);

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

			auto& editor_camera = m_module_scene->GetCamera(EDITOR_CAMERA_INDEX);
			if (editor_camera.m_width != (int)draw_region_size.x || editor_camera.m_height != (int)draw_region_size.y)
			{
				editor_camera.m_width = (int)draw_region_size.x, editor_camera.m_height = (int)draw_region_size.y;
				editor_camera.SetAspectRatio(float(editor_camera.m_width) / float(editor_camera.m_height));
				UpdateCamera(EDITOR_CAMERA_INDEX);
			}

			// render editor view based on current size.
			auto& scene_resource = m_module_scene->scene_resource;
			auto& render_setting = m_module_scene->render_setting;
			m_module_render->EditorViewFetchResult(scene_resource, render_setting, (int)draw_region_size.x, (int)draw_region_size.y);

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
				if (!m_module_render->editor_view_image_fetch_over_time) {
					auto read_px = (uint32_t)mouse_x_pos;
					auto read_py = (uint32_t)glm::max(draw_region_size.y - 1.0 - mouse_y_pos, 0.0);
					m_module_render->EditorViewReadPixel<uint32_t>(aov_name_primitive_index, read_px, read_py, &clicked_pixel_primitive_index);
				}
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
			if (is_hovered) {
				if (glm::abs(io.MouseWheel) > 0.0f) {
					auto& cam = *(trackball.cam);
					auto offset = io.MouseWheel;
					auto fov = cam.GetFov();
					fov -= glm::radians(offset);
					cam.SetFov(glm::clamp(fov, glm::radians(1.0f), glm::radians(90.0f)));
					m_module_scene->AddSceneFlag(SceneModule::SCENE_CAMERA_CHANGE);
				}
				if (ImGui::IsKeyDown(ImGuiKey_W)) {
					trackball.CameraMoveForward();
					m_module_scene->AddSceneFlag(SceneModule::SCENE_CAMERA_CHANGE);
				}
				else if (ImGui::IsKeyDown(ImGuiKey_S)) {
					trackball.CameraMoveBackward();
					m_module_scene->AddSceneFlag(SceneModule::SCENE_CAMERA_CHANGE);
				}
				else if (ImGui::IsKeyDown(ImGuiKey_A)) {
					trackball.CameraMoveLeft();
					m_module_scene->AddSceneFlag(SceneModule::SCENE_CAMERA_CHANGE);
				}
				else if (ImGui::IsKeyDown(ImGuiKey_D)) {
					trackball.CameraMoveRight();
					m_module_scene->AddSceneFlag(SceneModule::SCENE_CAMERA_CHANGE);
				}
				else {

				}
			}

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
