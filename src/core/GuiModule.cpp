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
		m_window(window), 
		m_width(width), 
		m_height(height), 
		m_module_render(module_render), 
		m_module_scene(module_scene), 
		trackball(module_scene->GetCamera(EDITOR_CAMERA_INDEX)), 
		clicked_pixel_primitive_index(EMPTY_UINT32),
		draw_selected_effect(true),
		show_primitive_list(true),
		primitive_scale_upper(20.0f),
		primitive_scale_lower(0.05f),
		primitive_move_speed(0.1f)
	{
		glfwSetWindowUserPointer(m_window, this);
		glfwSetFramebufferSizeCallback(m_window, FramebufferSizeCallback);
		glfwSetKeyCallback(m_window, KeyBoardCallBack);

		InitGUI();
	}

	GuiModule::~GuiModule() {
		ExitGUI();
	}

	bool GuiModule::UpdateScene()
	{
		auto& m_scene = (*m_module_scene);
		if (!m_scene.SceneChanged()) {
			return false;
		}

		// host data update.
		const bool rebuild_bounding_volume_hierarchy = 
			m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_CREATE) ||
			m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_DELETE) ||
			m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_GEOMETRY_CHANGE) ||
			m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_TRANSFORM_CHANGE);
		if (rebuild_bounding_volume_hierarchy) {
			std::unordered_map<uint32_t, uint32_t> dst_indices_map; // note: old index map to new index;

			ACBVHBuilder SceneBuilder(m_scene.primitive_instances.data(), (uint32_t)m_scene.primitive_instances.size(), m_scene.transforms.data(), m_scene.geometries.data());
			m_scene.top_nodes.clear();
			SceneBuilder.BuildSceneBVH(m_scene.top_nodes, nullptr, &clicked_pixel_primitive_index, &dst_indices_map);

			for (auto& item: m_scene.primitive_index_to_index_map) {
				item.second = dst_indices_map[item.second];
				assert(m_scene.primitive_instances.at(item.second).unique_index == item.first);
			}
		}

		const bool rebuild_light_list = 
			m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_SHAPE_LIGHT_CREATE) || 
			m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_SHAPE_LIGHT_DELETE) || 
			m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_SHAPE_LIGHT_CHANGE) || 
			rebuild_bounding_volume_hierarchy;
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
			m_scene.camera[EDITOR_CAMERA_INDEX].InitCameraRayTransfer(m_scene.GetHostSceneDataPointer());

			// restore rendering camera's width, height, aspect ratio.
			const int camera_width = m_scene.camera[RENDERING_CAMERA_INDEX].m_width, camera_height = m_scene.camera[RENDERING_CAMERA_INDEX].m_height;
			memcpy(&m_scene.camera[RENDERING_CAMERA_INDEX], &m_scene.camera[EDITOR_CAMERA_INDEX], sizeof(Camera));
			m_scene.camera[RENDERING_CAMERA_INDEX].m_width = camera_width, m_scene.camera[RENDERING_CAMERA_INDEX].m_height = camera_height;
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

			if (m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_IMAGE_TILE_CREATE) || m_scene.CheckSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_IMAGE_TILE_DELETE)) {
				FREE_GPU_RESOURCE(m_scene.image_tile_cache.image_tiles_device);
				m_scene.image_tile_cache.image_tiles_host = nullptr;
				FREE_GPU_RESOURCE(scene_device_data.image_tile_cache);

				m_scene.image_tile_cache.image_tiles_host = m_scene.image_texture_tiles.data();
				UPLOAD_TO_GPU(m_scene.image_tile_cache.image_tiles_device, m_scene.image_texture_tiles.data(), sizeof(ImageTile) * m_scene.image_texture_tiles.size());

				UPLOAD_TO_GPU(scene_device_data.image_tile_cache, &(m_scene.image_tile_cache), sizeof(ImageTileCache));
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
		return true;
	}

	bool GuiModule::UpdateCamera(int index)
	{
		auto& m_scene = (*m_module_scene);

		// re-upload one of the camera.
		UpdateDeviceData([&]() {
			auto& scene_resource = m_scene.scene_resource;
			TRANSFER_TO_GPU(&scene_resource.scene.camera[index], &m_scene.camera[index], sizeof(Camera));
		});
		return true;
	}

	void GuiModule::RenderImages()
	{
		ImGuiIO& io = ImGui::GetIO();

		// TODO: should not consider unused scene assests.
		bool scene_updated = false;
		if (UpdateScene()) {
			scene_updated = true;
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
			bool path_tracing_scene_updated = false;
			if (rendering_camera.m_width != (int)draw_region_size.x || rendering_camera.m_height != (int)draw_region_size.y)
			{
				rendering_camera.m_width = (int)draw_region_size.x, rendering_camera.m_height = (int)draw_region_size.y;
				rendering_camera.SetAspectRatio(float(rendering_camera.m_width) / float(rendering_camera.m_height));
				UpdateCamera(RENDERING_CAMERA_INDEX);
				
				path_tracing_scene_updated = true;
			}
			path_tracing_scene_updated = path_tracing_scene_updated || scene_updated;

			// render path tracing view based on current size.
			auto& scene_resource = m_module_scene->scene_resource;
			auto& render_setting = m_module_scene->render_setting;
			static ExtraTaskResults path_tracing_extra_task_results = {};
			m_module_render->PathTracingFetchResult(scene_resource, path_tracing_scene_updated, render_setting, (int)draw_region_size.x, (int)draw_region_size.y, &path_tracing_extra_task_results);

			// image button style.
			ImGui::PushID(1);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

			// draw image button.
			auto image_texture_handle = m_module_render->PathTracingGetTexture(aov_name_post_processing);
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

			auto& m_scene = *m_module_scene;

			auto& editor_camera = m_scene.GetCamera(EDITOR_CAMERA_INDEX);
			bool editor_view_scene_updated = false;
			if (editor_camera.m_width != (int)draw_region_size.x || editor_camera.m_height != (int)draw_region_size.y)
			{
				editor_camera.m_width = (int)draw_region_size.x, editor_camera.m_height = (int)draw_region_size.y;
				editor_camera.SetAspectRatio(float(editor_camera.m_width) / float(editor_camera.m_height));
				UpdateCamera(EDITOR_CAMERA_INDEX);

				editor_view_scene_updated = true;
			}
			editor_view_scene_updated = editor_view_scene_updated || scene_updated;

			// render editor view based on current size.
			auto& scene_resource = m_scene.scene_resource;
			auto& render_setting = m_scene.render_setting;
			static ExtraTaskResults editor_view_extra_task_results = {};
			m_module_render->EditorViewFetchResult(scene_resource, editor_view_scene_updated, render_setting, (int)draw_region_size.x, (int)draw_region_size.y, clicked_pixel_primitive_index, draw_selected_effect, &editor_view_extra_task_results);

			// image button style.
			ImGui::PushID(1);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

			// draw image button.
			auto image_texture_handle = m_module_render->EditorViewGetTexture(aov_name_post_processing);
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

					m_scene.AddSceneFlag(SceneModule::SCENE_CAMERA_CHANGE);
				}
			}
			if (is_hovered) {
				if (glm::abs(io.MouseWheel) > 0.0f) {
					auto& cam = *(trackball.cam);
					auto offset = io.MouseWheel;
					auto fov = cam.GetFov();
					fov -= glm::radians(offset);
					cam.SetFov(glm::clamp(fov, glm::radians(1.0f), glm::radians(90.0f)));
					m_scene.AddSceneFlag(SceneModule::SCENE_CAMERA_CHANGE);
				}
				if (ImGui::IsKeyDown(ImGuiKey_W)) {
					trackball.CameraMoveForward();
					m_scene.AddSceneFlag(SceneModule::SCENE_CAMERA_CHANGE);
				}
				else if (ImGui::IsKeyDown(ImGuiKey_S)) {
					trackball.CameraMoveBackward();
					m_scene.AddSceneFlag(SceneModule::SCENE_CAMERA_CHANGE);
				}
				else if (ImGui::IsKeyDown(ImGuiKey_A)) {
					trackball.CameraMoveLeft();
					m_scene.AddSceneFlag(SceneModule::SCENE_CAMERA_CHANGE);
				}
				else if (ImGui::IsKeyDown(ImGuiKey_D)) {
					trackball.CameraMoveRight();
					m_scene.AddSceneFlag(SceneModule::SCENE_CAMERA_CHANGE);
				}
				else {

				}
			}

			ImGui::PopStyleVar();
			ImGui::PopID();

			ImGui::End();
		}
	}

	void GuiModule::RenderMainMenu() {
		ImGui::Begin("Main Menu");
		
		auto& m_renderer = *m_module_render;
		auto& m_scene = *m_module_scene;

		if (ImGui::CollapsingHeader("Render Setting"))
		{
			auto& render_setting = m_scene.render_setting;
			const int accumulated_frame_count = render_setting.max_frame_count > 0 ? glm::min(m_renderer.path_tracing_frame_index, render_setting.max_frame_count) : m_renderer.path_tracing_frame_index;

			ImGui::Text("Accumulate Frame Count: %d", accumulated_frame_count);
			ImGui::ProgressBar(render_setting.max_frame_count > 0 ? float(accumulated_frame_count) / float(render_setting.max_frame_count) : 1.0f);
			if (render_setting.max_frame_count > 0 && accumulated_frame_count == render_setting.max_frame_count) {
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(250, 4, 4, 255));
				ImGui::Text("Render Finish!");
				ImGui::PopStyleColor();
			}

			static int sampler_index = 0;
			static const char* sampler_names[3] = { "PCG", "Halton", "Sobol" };
			if (ImGui::BeginCombo("Sampler", sampler_names[sampler_index])) {
				for (int sampler_type = PCG_SAMPLER; sampler_type <= SOBOL_SAMPLER; ++sampler_type) {
					if (ImGui::Selectable(sampler_names[sampler_type])) {
						sampler_index = sampler_type;
						render_setting.sampler_type = sampler_type;
						m_scene.AddSceneFlag(SceneModule::SCENE_RENDER_SETTING_CHANGE);
					}
				}
				ImGui::EndCombo();
			}

			ImGui::Separator();
			ImGui::Checkbox("Enable Selected Effect", &draw_selected_effect);
			if (ImGui::Checkbox("Distant Light", &render_setting.enable_distant_light)) {
				m_scene.AddSceneFlag(SceneModule::SCENE_RENDER_SETTING_CHANGE);
			}
			if (ImGui::Checkbox("Shape Light", (bool*)(&render_setting.enable_shape_light))) {
				m_scene.AddSceneFlag(SceneModule::SCENE_RENDER_SETTING_CHANGE);
			}
			if (ImGui::Checkbox("Env Light", &render_setting.enable_env_light)) {
				m_scene.AddSceneFlag(SceneModule::SCENE_RENDER_SETTING_CHANGE);
			}
			if (ImGui::Checkbox("Volume Scatter", &render_setting.enable_volume_scattering)) {
				m_scene.AddSceneFlag(SceneModule::SCENE_RENDER_SETTING_CHANGE);
			}
			if (ImGui::Checkbox("Russian Roulette", &render_setting.enable_russian_roulette)) {
				m_scene.AddSceneFlag(SceneModule::SCENE_RENDER_SETTING_CHANGE);
			}

			ImGui::Separator();
			if (ImGui::InputInt("Sample Count", &render_setting.ssp)) {
				render_setting.ssp = glm::clamp(render_setting.ssp, 1, 16);
				m_scene.AddSceneFlag(SceneModule::SCENE_RENDER_SETTING_CHANGE);
			}
			if (ImGui::InputInt("Max Accumulate Frame Count", &render_setting.max_frame_count)) {
				render_setting.max_frame_count = glm::max(render_setting.max_frame_count, 0);
				m_scene.AddSceneFlag(SceneModule::SCENE_RENDER_SETTING_CHANGE);
			}
			if (ImGui::InputInt("Ray Depth", &render_setting.ray_depth)) {
				render_setting.ray_depth = glm::clamp(render_setting.ray_depth, 1, 12);
				m_scene.AddSceneFlag(SceneModule::SCENE_RENDER_SETTING_CHANGE);
			}
			if (ImGui::InputFloat("Exposure", &render_setting.exposure)) {
				render_setting.exposure = glm::max(0.0f, render_setting.exposure);
				m_scene.AddSceneFlag(SceneModule::SCENE_RENDER_SETTING_CHANGE);
			}
			if (ImGui::InputFloat("Gamma", &render_setting.gamma)) {
				render_setting.gamma = glm::max(0.0f, render_setting.gamma);
				m_scene.AddSceneFlag(SceneModule::SCENE_RENDER_SETTING_CHANGE);
			}
		}

		if (ImGui::CollapsingHeader("Primitive")) {
			ImGui::Checkbox("Show Primitive List", &show_primitive_list);
		}

		ImGui::End();
	}
	
	void GuiModule::RenderObjectList()
	{
		if (!show_primitive_list) {
			return;
		}

		auto& m_scene = *m_module_scene;
		enum ObjectType {
			Directional_Light = 0, Primitive_Instance = 1, Volume = 2
		};
		ObjectType object_type_to_render = Primitive_Instance;
		static int selected_directional_index = EMPTY_UINT32;
		static int selected_primitive_instance_index = EMPTY_UINT32;
		static int selected_volume_index = EMPTY_UINT32;

		selected_primitive_instance_index = clicked_pixel_primitive_index;

		ImGui::Begin("Object List");
		const float window_x_size = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
		ImGui::BeginChild("left pane", ImVec2(window_x_size * 0.25f, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
		for (int directional_light_index = 0; directional_light_index < m_scene.distant_lights.size(); ++directional_light_index) {
			if (ImGui::Selectable(m_scene.distant_light_names[directional_light_index].c_str(), directional_light_index == selected_directional_index)) {
				selected_directional_index = directional_light_index;
				object_type_to_render = Directional_Light;
			}
		}

		for (int primitive_index = 0; primitive_index < m_scene.primitive_instances.size(); primitive_index++) {
			const std::string primitive_name = std::string("primitive_") + std::to_string(primitive_index);
			if (ImGui::Selectable(primitive_name.c_str(), primitive_index == selected_primitive_instance_index)) {
				selected_primitive_instance_index = primitive_index;
				object_type_to_render = Primitive_Instance;

				clicked_pixel_primitive_index = primitive_index;
			}
		}

		for (int volume_index = 0; volume_index < m_scene.volumes.size(); volume_index++) {
			if (ImGui::Selectable(m_scene.volume_names[volume_index].c_str(), volume_index == selected_volume_index)) {
				selected_volume_index = volume_index;
				object_type_to_render = Volume;
			}
		}

		ImGui::EndChild();

		ImGui::SameLine();

		// Right
		ImGui::BeginGroup();
		ImGui::BeginChild("Attribute editor", ImVec2(0, -ImGui::GetFrameHeight()), ImGuiChildFlags_Border); // Leave room for 1 line below us

		const bool selected_primitive_index_valid = selected_primitive_instance_index < m_scene.primitive_instances.size();
		ImGui::Text("Selected primitive index: %d", selected_primitive_instance_index);
		if (selected_primitive_index_valid) {
			// TODO: use selectable list.
			auto& selected_primitive_instance = m_scene.primitive_instances.at(selected_primitive_instance_index);
			if (ImGui::InputInt("Geometry index", (int*)(&selected_primitive_instance.geometry_idx))) {
				selected_primitive_instance.geometry_idx = glm::clamp(selected_primitive_instance.geometry_idx, 0u, (uint32_t)m_scene.geometries.size() - 1);
				UpdateDeviceData([&]() {
					m_scene.UpdatePrimitiveInstance(selected_primitive_instance_index);
				});
				m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_GEOMETRY_CHANGE);
			}
			if (ImGui::InputInt("Transform index", (int*)(&selected_primitive_instance.transform_idx))) {
				selected_primitive_instance.transform_idx = glm::clamp(selected_primitive_instance.transform_idx, 0u, (uint32_t)m_scene.transform_states.size() - 1);
				UpdateDeviceData([&]() {
					m_scene.UpdatePrimitiveInstance(selected_primitive_instance_index);
				});
				m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_TRANSFORM_CHANGE);
			}

			const auto original_material_index = selected_primitive_instance.material_idx;
			if (ImGui::InputInt("Material index", (int*)(&selected_primitive_instance.material_idx))) {
				selected_primitive_instance.material_idx = glm::clamp(selected_primitive_instance.material_idx, 0u, (uint32_t)m_scene.materials.size() - 1);
				UpdateDeviceData([&]() {
					m_scene.UpdatePrimitiveInstance(selected_primitive_instance_index);
				});
				m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_MATERIAL_CHANGE);
				if (m_scene.materials[selected_primitive_instance.material_idx].material_type == LIGHT_MTL || m_scene.materials[original_material_index].material_type == LIGHT_MTL) {
					m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_SHAPE_LIGHT_CHANGE);
				}
			}

			if (ImGui::InputInt("Inner volume index", (int*)(&selected_primitive_instance.inner_volume_idx))) {
				selected_primitive_instance.inner_volume_idx = glm::clamp(selected_primitive_instance.inner_volume_idx, -1, (int)m_scene.volumes.size() - 1);
				UpdateDeviceData([&]() {
					m_scene.UpdatePrimitiveInstance(selected_primitive_instance_index);
				});
				m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_VOLUME_CHANGE);
			}

			if (ImGui::Checkbox("Treat as volume boundary", (bool*)(&selected_primitive_instance.treat_as_boundary))) {
				UpdateDeviceData([&]() {
					m_scene.UpdatePrimitiveInstance(selected_primitive_instance_index);
				});
				m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_VOLUME_CHANGE);
			}
		}
		ImGui::Separator();
		
		
		if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_None))
		{
			if (ImGui::BeginTabItem("Geometry")) {
				ImGui::Text("Geometry name: %s", selected_primitive_index_valid ? m_scene.geometry_names[m_scene.primitive_instances.at(selected_primitive_instance_index).geometry_idx].c_str() : "");
				ImGui::Text("Geometry index: %d", selected_primitive_index_valid ? m_scene.primitive_instances.at(selected_primitive_instance_index).geometry_idx : EMPTY_UINT32);
				ImGui::Separator();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Transform")) {
				ImGui::Text("Transform name: %s", selected_primitive_index_valid ? m_scene.transform_names[m_scene.primitive_instances.at(selected_primitive_instance_index).transform_idx].c_str() : "");
				ImGui::Text("Transform index: %d", selected_primitive_index_valid ? m_scene.primitive_instances.at(selected_primitive_instance_index).transform_idx : EMPTY_UINT32);
				ImGui::Separator();
				if (ImGui::InputFloat("Max Scale", &primitive_scale_upper)) {
					primitive_scale_upper = glm::clamp(primitive_scale_upper, 2.0f * primitive_scale_lower, 1000.0f);
				}
				if (ImGui::InputFloat("Move Speed", &primitive_move_speed)) {
					primitive_move_speed = glm::max(+0.0f, primitive_move_speed);
				}

				auto set_instance_transform_flag = [&m_scene](const bool primitive_is_light) {
					m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_TRANSFORM_CHANGE);
					m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_TRANSFORM_CHANGE);
					if (primitive_is_light) {
						m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_SHAPE_LIGHT_CHANGE);
					}
				};
				auto draw_arrow_buttons = [&](const char* title,
					const char* arrow_sub,
					const char* arrow_add,
					glm::vec3 &primitive_translate,
					const uint32_t transform_index,
					const uint32_t i, 
					const bool primitive_is_light)->void
				{
					ImGui::Text(title);
					ImGui::SameLine();
					ImGui::PushButtonRepeat(true);
					if (ImGui::ArrowButton(arrow_sub, ImGuiDir_Left)) {
						primitive_translate[i] -= primitive_move_speed;
						UpdateDeviceData([&]() {
							m_scene.UpdateTransform(transform_index);
						});
						set_instance_transform_flag(primitive_is_light);
					}
					ImGui::SameLine();
					if (ImGui::ArrowButton(arrow_add, ImGuiDir_Right)) {
						primitive_translate[i] += primitive_move_speed;
						UpdateDeviceData([&]() {
							m_scene.UpdateTransform(transform_index);
							});
						set_instance_transform_flag(primitive_is_light);
					}
					ImGui::PopButtonRepeat();
				};
				auto draw_scale_slider = [&](const char* title, 
					glm::vec3& primitive_scale, 
					const uint32_t transform_index, 
					const uint32_t i, 
					const bool primitive_is_light) {
					if (ImGui::SliderFloat(title, &primitive_scale[i], primitive_scale_lower, primitive_scale_upper)) {
						UpdateDeviceData([&]() {
							m_scene.UpdateTransform(transform_index);
						});
						set_instance_transform_flag(primitive_is_light);
					}
				};
				auto draw_rotate_slider = [&](const char* title, 
					glm::vec3& primitive_rotate,
					const uint32_t transform_index,
					const uint32_t i,
					const bool primitive_is_light) {
					if (ImGui::SliderFloat(title, &primitive_rotate[i], 0.0f, 360.0f)) {
						UpdateDeviceData([&]() {
							m_scene.UpdateTransform(transform_index);
						});
						set_instance_transform_flag(primitive_is_light);
					}
				};

				if (selected_primitive_index_valid)
				{
					const auto transform_index = m_scene.primitive_instances.at(selected_primitive_instance_index).transform_idx;
					const auto material_index = m_scene.primitive_instances.at(selected_primitive_instance_index).material_idx;
					const bool primitive_is_light = m_scene.materials[material_index].material_type == LIGHT_MTL;
					
					auto& transform_state = m_scene.transform_states[transform_index];
					if (ImGui::TreeNode("Translate"))
					{
						glm::vec3& prim_translate = transform_state.translate_xyz;
						draw_arrow_buttons("Translate X", "X-", "X+", prim_translate, transform_index, 0, primitive_is_light);
						draw_arrow_buttons("Translate Y", "Y-", "Y+", prim_translate, transform_index, 1, primitive_is_light);
						draw_arrow_buttons("Translate Z", "Z-", "Z+", prim_translate, transform_index, 2, primitive_is_light);
						if (ImGui::InputFloat3("Translate", (float*)(&prim_translate))) {
							UpdateDeviceData([&]() {
								m_scene.UpdateTransform(transform_index);
							});
							set_instance_transform_flag(primitive_is_light);
						}
						ImGui::TreePop();
					}
					if (ImGui::TreeNode("Scale"))
					{
						glm::vec3& prim_scale = transform_state.scale_xyz;
						draw_scale_slider("X scale", prim_scale, transform_index, 0, primitive_is_light);
						draw_scale_slider("Y scale", prim_scale, transform_index, 1, primitive_is_light);
						draw_scale_slider("Z scale", prim_scale, transform_index, 2, primitive_is_light);
						if (ImGui::InputFloat3("Scale", (float*)(&prim_scale))) {
							prim_scale = glm::clamp(prim_scale, glm::vec3(primitive_scale_lower), glm::vec3(primitive_scale_upper));
							UpdateDeviceData([&]() {
								m_scene.UpdateTransform(transform_index);
							});
							set_instance_transform_flag(primitive_is_light);
						}
						ImGui::TreePop();
					}
					if (ImGui::TreeNode("Rotate"))
					{
						glm::vec3& prim_rotate = transform_state.rotate_xyz;
						draw_rotate_slider("X Rotate", prim_rotate, transform_index, 0, primitive_is_light);
						draw_rotate_slider("Y Rotate", prim_rotate, transform_index, 1, primitive_is_light);
						draw_rotate_slider("Z Rotate", prim_rotate, transform_index, 2, primitive_is_light);
						if (ImGui::InputFloat3("Rotate", (float*)(&prim_rotate))) {
							prim_rotate = glm::clamp(prim_rotate, glm::vec3(0.0f), glm::vec3(360.0f));
							UpdateDeviceData([&]() {
								m_scene.UpdateTransform(transform_index);
							});
							set_instance_transform_flag(primitive_is_light);
						}
						ImGui::TreePop();
					}
					ImGui::Separator();
					
					auto draw_transform_edit_board = [&](const uint32_t transform_index) {
						if (ImGui::BeginPopupContextItem()) {
							auto& primitive_transform_state = m_scene.transform_states.at(transform_index);

							auto& translate = primitive_transform_state.translate_xyz;
							draw_arrow_buttons("Translate X", "X-", "X+", translate, transform_index, 0, primitive_is_light);
							draw_arrow_buttons("Translate Y", "Y-", "Y+", translate, transform_index, 1, primitive_is_light);
							draw_arrow_buttons("Translate Z", "Z-", "Z+", translate, transform_index, 2, primitive_is_light);
							if (ImGui::InputFloat3("Translate", (float*)(&translate))) {
								UpdateDeviceData([&]() {
									m_scene.UpdateTransform(transform_index);
								});
								set_instance_transform_flag(primitive_is_light);
							}

							auto& scale = primitive_transform_state.scale_xyz;
							draw_scale_slider("X scale", scale, transform_index, 0, primitive_is_light);
							draw_scale_slider("Y scale", scale, transform_index, 1, primitive_is_light);
							draw_scale_slider("Z scale", scale, transform_index, 2, primitive_is_light);
							if (ImGui::InputFloat3("Scale", (float*)(&scale))) {
								scale = glm::clamp(scale, glm::vec3(primitive_scale_lower), glm::vec3(primitive_scale_upper));
								UpdateDeviceData([&]() {
									m_scene.UpdateTransform(transform_index);
								});
								set_instance_transform_flag(primitive_is_light);
							}

							auto &rotate = primitive_transform_state.rotate_xyz;
							draw_rotate_slider("X Rotate", rotate, transform_index, 0, primitive_is_light);
							draw_rotate_slider("Y Rotate", rotate, transform_index, 1, primitive_is_light);
							draw_rotate_slider("Z Rotate", rotate, transform_index, 2, primitive_is_light);
							if (ImGui::InputFloat3("Rotate", (float*)(&rotate))) {
								rotate = glm::clamp(rotate, glm::vec3(0.0f), glm::vec3(360.0f));
								UpdateDeviceData([&]() {
									m_scene.UpdateTransform(transform_index);
								});
								set_instance_transform_flag(primitive_is_light);
							}
							ImGui::EndPopup();
						}
					};
					std::function<void(const uint32_t)> draw_transform_node = [&](const uint32_t transform_index) {
						const auto& transform_name = m_scene.transform_names.at(transform_index);
						const auto parent_transform_index = m_scene.transform_states.at(transform_index).parent_transform_index;
						if (ImGui::TreeNode(transform_name.c_str())) {
							draw_transform_edit_board(transform_index);
							if (parent_transform_index != EMPTY_UINT32) {
								draw_transform_node(parent_transform_index);
							}
							ImGui::TreePop();
						}
					};

					ImGui::Text("Here shows the reference relationship of transforms, you can right click the tree node to see its detailed information :)");
					if (ImGui::TreeNode("Hierarchy")) {
						
						draw_transform_node(transform_index);
						ImGui::TreePop();
					}
				}
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Material")) {
				ImGui::Text("Material name: %s", selected_primitive_index_valid ? m_scene.material_names[m_scene.primitive_instances.at(selected_primitive_instance_index).material_idx].c_str() : "");
				ImGui::Text("Material index: %d", selected_primitive_index_valid ? m_scene.primitive_instances.at(selected_primitive_instance_index).material_idx : EMPTY_UINT32);
				ImGui::Separator();
				if (selected_primitive_index_valid) {
					const auto material_index = m_scene.primitive_instances.at(selected_primitive_instance_index).material_idx;
					auto& selected_material = m_scene.materials.at(material_index);
					static const char* material_type_names[] = { "Default material", "Light material" };

					const auto original_material_type = selected_material.material_type;
					if (ImGui::BeginCombo("Material type", material_type_names[selected_material.material_type])) {
						if (ImGui::Selectable(material_type_names[0]) && original_material_type != DEFAULT_MTL) {
							selected_material.InitDefaultMtl();
							UpdateDeviceData([&]() {
								m_scene.UpdateMaterial(material_index);
							});
							m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_MATERIAL_CHANGE);
							m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_SHAPE_LIGHT_CHANGE);
						}
						if (ImGui::Selectable(material_type_names[1]) && original_material_type != LIGHT_MTL) {
							selected_material.InitLightMtl();
							UpdateDeviceData([&]() {
								m_scene.UpdateMaterial(material_index);
							});
							m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_MATERIAL_CHANGE);
							m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_SHAPE_LIGHT_CHANGE);
						}
						ImGui::EndCombo();
					}

					auto draw_texture_selector = [&](const char * selector_name, uint32_t * material_texture_index) {
						assert(material_texture_index != nullptr);
						auto& textures = m_scene.textures;
						auto& texture_names = m_scene.texture_names;
						auto& selected_texture_index = *material_texture_index;
						if (ImGui::BeginCombo(selector_name, selected_texture_index == EMPTY_UINT32 ? "NULL" : texture_names[selected_texture_index].c_str())) {
							if (ImGui::Selectable("Null")) {
								if (selected_texture_index != EMPTY_UINT32) {
									selected_texture_index = EMPTY_UINT32;
									UpdateDeviceData([&]() {
										m_scene.UpdateMaterial(material_index);
									});
									m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_MATERIAL_CHANGE);
								}
							}
							for (int texture_index = 0; texture_index < (int)textures.size(); ++texture_index) {
								if (ImGui::Selectable(texture_names[texture_index].c_str())) {
									if (texture_index == selected_texture_index) {
										continue;
									}
									selected_texture_index = texture_index;
									UpdateDeviceData([&]() {
										m_scene.UpdateMaterial(material_index);
										});
									m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_MATERIAL_CHANGE);
								}
							}
							ImGui::EndCombo();
						}
					};

					ImGui::Separator();
					if (selected_material.material_type == DEFAULT_MTL) {
						auto& selected_default_mtl = selected_material.default_mtl;
						if (ImGui::ColorEdit3("Diffuse albedo", (float*)(&selected_default_mtl.diffuse_albedo), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR)) {
							UpdateDeviceData([&]() {
								m_scene.UpdateMaterial(material_index);
							});
							m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_MATERIAL_CHANGE);
						}
						ImGui::SameLine(); 
						if(ImGui::Button("Diffuse albedo texture")) {
							ImGui::OpenPopup("Diffuse_albedo_popup");
						}
						if (ImGui::BeginPopup("Diffuse_albedo_popup")) {
							draw_texture_selector("Diffuse albedo texture selector", &selected_default_mtl.diffuse_albedo_tex);
							ImGui::EndPopup();
						}


						if (ImGui::SliderFloat("Metalness", &selected_default_mtl.metalness, 0.0f, 1.0f)) {
							UpdateDeviceData([&]() {
								m_scene.UpdateMaterial(material_index);
							});
							m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_MATERIAL_CHANGE);
						}
						ImGui::SameLine();
						if (ImGui::Button("Metalness texture")) {
							ImGui::OpenPopup("Metalness_popup");
						}
						if (ImGui::BeginPopup("Metalness_popup")) {
							draw_texture_selector("Metalness texture selector", &selected_default_mtl.metalness_tex);
							ImGui::EndPopup();
						}

						ImGui::Separator();
						if (ImGui::SliderFloat("Specular weight", &selected_default_mtl.specular_weight, 0.0f, 1.0f)) {
							UpdateDeviceData([&]() {
								m_scene.UpdateMaterial(material_index);
							});
							m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_MATERIAL_CHANGE);
						}
						ImGui::SameLine();
						if (ImGui::Button("Specular weight texture")) {
							ImGui::OpenPopup("Specular_weight_popup");
						}
						if (ImGui::BeginPopup("Specular_weight_popup")) {
							draw_texture_selector("Specular weight texture selector", &selected_default_mtl.specular_weight_tex);
							ImGui::EndPopup();
						}

						if (ImGui::ColorEdit3("Specular albedo", (float*)(&selected_default_mtl.specular_albedo), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR)) {
							UpdateDeviceData([&]() {
								m_scene.UpdateMaterial(material_index);
							});
							m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_MATERIAL_CHANGE);
						}
						ImGui::SameLine();
						if (ImGui::Button("Specular albedo texture")) {
							ImGui::OpenPopup("Specular_albedo_popup");
						}
						if (ImGui::BeginPopup("Specular_albedo_popup")) {
							draw_texture_selector("Specular albedo texture selector", &selected_default_mtl.specular_albedo_tex);
							ImGui::EndPopup();
						}

						ImGui::Separator();
						if (ImGui::InputFloat("IOR", &selected_default_mtl.ior_n)) {
							selected_default_mtl.ior_n = glm::max(0.0f, selected_default_mtl.ior_n);
							UpdateDeviceData([&]() {
								m_scene.UpdateMaterial(material_index);
							});
							m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_MATERIAL_CHANGE);
						}
						if (ImGui::InputInt("IOR priority", (int*)&selected_default_mtl.ior_priority)) {
							selected_default_mtl.ior_priority = glm::max(selected_default_mtl.ior_priority, 0u);
							UpdateDeviceData([&]() {
								m_scene.UpdateMaterial(material_index);
							});
							m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_MATERIAL_CHANGE);
						}

						ImGui::Separator();
						if (ImGui::SliderFloat("Roughness x", &selected_default_mtl.alpha_x, 0.0f, 1.0f)) {
							UpdateDeviceData([&]() {
								m_scene.UpdateMaterial(material_index);
							});
							m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_MATERIAL_CHANGE);
						}
						ImGui::SameLine();
						if (ImGui::Button("Roughness x texture")) {
							ImGui::OpenPopup("Roughness_x_popup");
						}
						if (ImGui::BeginPopup("Roughness_x_popup")) {
							draw_texture_selector("Roughness x texture selector", &selected_default_mtl.alpha_x_tex);
							ImGui::EndPopup();
						}

						if (ImGui::SliderFloat("Roughness y", &selected_default_mtl.alpha_y, 0.0f, 1.0f)) {
							UpdateDeviceData([&]() {
								m_scene.UpdateMaterial(material_index);
							});
							m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_MATERIAL_CHANGE);
						}
						ImGui::SameLine();
						if (ImGui::Button("Roughness y texture")) {
							ImGui::OpenPopup("Roughness_y_popup");
						}
						if (ImGui::BeginPopup("Roughness_y_popup")) {
							draw_texture_selector("Roughness y texture selector", &selected_default_mtl.alpha_y_tex);
							ImGui::EndPopup();
						}

						ImGui::Separator();
						if (ImGui::SliderFloat("Transmission weight", &selected_default_mtl.transmission_weight, 0.0f, 1.0f)) {
							UpdateDeviceData([&]() {
								m_scene.UpdateMaterial(material_index);
							});
							m_scene.AddSceneFlag(SceneModule::SCENE_CHANGE_FLAG::SCENE_INSTANCE_MATERIAL_CHANGE);
						}
						ImGui::SameLine();
						if (ImGui::Button("Transmission weight texture")) {
							ImGui::OpenPopup("Transmission_weight_popup");
						}
						if (ImGui::BeginPopup("Transmission_weight_popup")) {
							draw_texture_selector("Transmission weight texture selector", &selected_default_mtl.transmission_weight_tex);
							ImGui::EndPopup();
						}

						ImGui::Separator();
						draw_texture_selector("Normal texture selector", &selected_default_mtl.normal_mapping_tex);
						draw_texture_selector("Bump texture selector", &selected_default_mtl.bump_mapping_tex);
					}
					else if (selected_material.material_type == LIGHT_MTL) {
					
					}
					else {
					
					}
				}
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::EndChild();
		ImGui::EndGroup();
		
		ImGui::End();
	}

	void GuiModule::Draw() {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGui::DockSpaceOverViewport();

		ImGuiIO& io = ImGui::GetIO();
		
		RenderImages();
		ImGui::ShowDemoWindow();
		RenderMainMenu();
		RenderObjectList();
		

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
