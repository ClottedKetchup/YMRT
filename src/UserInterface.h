#pragma once

#include <memory>
#include <string>

#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"

#include "Renderer.h"
#include "SceneManager.h"
#include "Camera.h"

namespace YumeRT
{
	class Trackball
	{
	public:
		int mouse_button = -1;
		float prev_pos_x = -1.0f;
		float prev_pos_y = -1.0f;
		float move_speed = 0.1f;
		float rotate_speed = 0.5f;
		bool start_tracking = false;
		Camera camera;

		static inline glm::vec3 CalculTrackBallVec(float xpos, float ypos, float width, float height)
		{
			glm::vec2 ndc = {
			2.f * (xpos / width) - 1.f,
			2.f * (ypos / height) - 1.f
			};
			ndc.y = -ndc.y;
			float r2 = ndc.x * ndc.x + ndc.y * ndc.y;
			float z = 0.0f;
			if (r2 < 1.0f)
			{
				z = glm::sqrt(1.0f - r2);
			}
			else
			{
				ndc /= glm::sqrt(r2);
			}
			return glm::vec3(ndc.x, ndc.y, z);
		}

		inline void CameraMoveForward()
		{
			camera.MoveForward(move_speed);
		}

		inline void CameraMoveBackward()
		{
			camera.MoveBackward(move_speed);
		}

		inline void CameraMoveLeft()
		{
			camera.MoveLeft(move_speed);
		}

		inline void CameraMoveRight()
		{
			camera.MoveRight(move_speed);
		}

		inline void Rotate(float angle /* radians */, const glm::vec3 &axis /* normalized */)
		{
			glm::mat4 rm = Matrix_R(axis, angle * rotate_speed);
			glm::vec4 local_dir_z = rm * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
			glm::vec3 world_dir_z = glm::vec3(camera.CameraToWorld(local_dir_z));

			if (glm::abs(glm::dot(world_dir_z, WORLD_UP)) < 0.999f)
			{
				camera.SetDir(world_dir_z);
			}
		}
	};

	inline void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
	inline void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	inline void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
	inline void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset);

	class UserInterface
	{
	public:
		friend inline void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
		friend inline void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
		friend inline void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
		friend inline void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset);

		inline UserInterface(const UserInterface&) = delete;
		inline UserInterface(const UserInterface&&) = delete;
		inline UserInterface& operator=(const UserInterface&) = delete;
		inline UserInterface(GLFWwindow *window,
			uint32_t width,
			uint32_t height,
			std::shared_ptr<Renderer> & ptr_renderer,
			std::shared_ptr<SceneManager> & ptr_scene_manager) :
			rt_renderer(ptr_renderer), scene_manager(ptr_scene_manager), m_width(width), m_height(height) 
		{
			printf("Gui module init...\n");
			glfwSetWindowUserPointer(window, this);
			SetCallBack(window);
			InitUI(window);
		}
		inline ~UserInterface() 
		{
			printf("Gui module exit...\n");
			DestroyResources();
		}

		inline void DestroyResources();

		inline void SetCallBack(GLFWwindow *window);

		inline void ProcessInput(GLFWwindow *window);

		inline void SetCamera(const glm::vec3 &from, const glm::vec3 &look, float fov, float aspect)
		{
			trackball.camera.SetFov(fov);
			trackball.camera.SetAspectRatio(aspect);
			trackball.camera.SetPosition(from);
			trackball.camera.SetDir(from - look);
		}

		inline void SetCamera(const Camera& camera)
		{
			trackball.camera = camera;
		}

		inline Camera& GetCamera() { return trackball.camera; }

		inline uint32_t GetWidth() { return m_width; }

		inline uint32_t GetHeight() { return m_height; }

		inline uint32_t GetDisplayAovIdx() { return m_display_aov_idx; }

		inline uint32_t GetSelectedPrimIdx() { return m_click_prim_idx; }

		inline void SetSelectedPrimIdx(uint32_t idx) { m_click_prim_idx = idx; }

		inline void RenderUI();

	private:
		
		Trackball trackball;
		std::shared_ptr<Renderer> rt_renderer = nullptr;
		std::shared_ptr<SceneManager> scene_manager = nullptr;
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_display_aov_idx = 0;
		bool m_mouse_on_GUI = false;
		bool m_lock_camera = false;

		uint32_t m_click_pixel_x = 0;
		uint32_t m_click_pixel_y = 0;
		uint32_t m_click_prim_idx = EMPTY_UINT32;
		glm::vec4 m_click_color = glm::vec4(0.0f);
		
		bool m_show_move_control_menu = false;
		bool m_show_geometry_list = false;
		bool m_show_material_list = false;
		bool m_show_render_menu = false;
		bool m_show_camera_menu = false;
		bool m_show_light_menu = false;
		bool m_show_volume_menu = false;
		bool m_show_texture_menu = false;

		inline void InitUI(GLFWwindow *window);

		inline void RenderMainMenu();

		inline void RenderOverlay();

		inline void RenderRTMenu();

		inline void RenderCameraMenu();

		inline void RenderGeometryList();

		inline void RenderMaterialList();

		inline void RenderLightList();

		inline void RenderVolumeList();

		inline void RenderTextureMenu();

		inline void RenderInstanceMenu();

		inline void ExitUI();
	};

	inline void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
	{
		UserInterface &user_interface = *((UserInterface*)glfwGetWindowUserPointer(window));

		user_interface.m_width = glm::max(width, 1);
		user_interface.m_height = glm::max(height, 1);
		user_interface.trackball.camera.SetAspectRatio(float(width) / float(height));
	}

	inline void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
	{
		double x_pos, y_pos;
		glfwGetCursorPos(window, &x_pos, &y_pos);

		UserInterface &user_interface = *((UserInterface*)glfwGetWindowUserPointer(window));

		if (button == GLFW_MOUSE_BUTTON_LEFT)
		{
			if (action == GLFW_PRESS)
			{
				if (!user_interface.m_mouse_on_GUI)
				{
					user_interface.m_click_pixel_x = (uint32_t)(x_pos - 0.5);
					user_interface.m_click_pixel_y = user_interface.m_height - (uint32_t)(y_pos - 0.5) - 1;

					// read pixel from buffer
					user_interface.rt_renderer->ReadPixel(user_interface.m_click_pixel_x,
						user_interface.m_click_pixel_y,
						user_interface.m_display_aov_idx,
																			      &user_interface.m_click_color,
																			      &user_interface.m_click_prim_idx);
				}

				user_interface.trackball.mouse_button = button;
				user_interface.trackball.start_tracking = true;
				user_interface.trackball.prev_pos_x = x_pos;
				user_interface.trackball.prev_pos_y = y_pos;
			}
			else if (action == GLFW_RELEASE)
			{
				user_interface.trackball.mouse_button = -1;
				user_interface.trackball.start_tracking = false;
			}
		}
	}

	inline void CursorPosCallback(GLFWwindow * window, double xpos, double ypos)
	{
		UserInterface &user_interface = *((UserInterface*)glfwGetWindowUserPointer(window));

		if (user_interface.m_mouse_on_GUI)
		{
			return;
		}
		if (!user_interface.m_lock_camera && user_interface.trackball.start_tracking)
		{
			double x_pos, y_pos;
			glfwGetCursorPos(window, &x_pos, &y_pos);

			glm::vec3 pre_vec = Trackball::CalculTrackBallVec(
				user_interface.trackball.prev_pos_x,
				user_interface.trackball.prev_pos_y,
				user_interface.m_width,
				user_interface.m_height);

			glm::vec3 cur_vec = Trackball::CalculTrackBallVec(
				float(x_pos), 
				float(y_pos), 
				user_interface.m_width,
				user_interface.m_height);

			if (glm::abs(glm::dot(pre_vec, cur_vec)) > 0.9999f) { return; }
			glm::vec3 axis = glm::cross(pre_vec, cur_vec);

			float angle = glm::acos(glm::min(glm::dot(pre_vec, cur_vec), 1.0f));

			user_interface.trackball.Rotate(angle, axis);
			user_interface.trackball.prev_pos_x = float(xpos);
			user_interface.trackball.prev_pos_y = float(ypos);
		}
	}

	inline void ScrollCallback(GLFWwindow * window, double xoffset, double yoffset)
	{
		UserInterface &user_interface = *((UserInterface*)glfwGetWindowUserPointer(window));

		if (user_interface.m_lock_camera) { return; }
		float fov = user_interface.trackball.camera.GetFov();
		fov -= glm::radians(yoffset);
		user_interface.trackball.camera.SetFov(glm::clamp(fov, glm::radians(1.0f), glm::radians(90.0f)));
	}

	inline void UserInterface::DestroyResources()
	{
		ExitUI();
		rt_renderer = nullptr;
		scene_manager = nullptr;
	}

	inline void UserInterface::SetCallBack(GLFWwindow *window)
	{
		glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
		glfwSetMouseButtonCallback(window, MouseButtonCallback);
		glfwSetCursorPosCallback(window, CursorPosCallback);
		glfwSetScrollCallback(window, ScrollCallback);
	}

	inline void UserInterface::ProcessInput(GLFWwindow * window)
	{
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, true);
		}
		else if (!m_lock_camera && glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		{
			trackball.CameraMoveForward();
		}
		else if (!m_lock_camera && glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		{
			trackball.CameraMoveBackward();
		}
		else if (!m_lock_camera && glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		{
			trackball.CameraMoveLeft();
		}
		else if (!m_lock_camera && glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		{
			trackball.CameraMoveRight();
		}
	}

	inline void UserInterface::InitUI(GLFWwindow *window)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		ImGui::StyleColorsClassic();
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 450");
	}

	inline void UserInterface::RenderOverlay()
	{
		// TODO: you should maintain each windows' size and pos?

		ImGuiWindowFlags windowFlags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav;

		const ImGuiViewport *viewport = ImGui::GetMainViewport();
		ImVec2 workPos = viewport->WorkPos;
		ImVec2 workSize = viewport->WorkSize;
		ImVec2 windowPos = ImVec2(workPos.x + 5.0f, workPos.y + 5.0f);
		ImGui::SetNextWindowBgAlpha(0.35f);
		ImGui::SetNextWindowPos(windowPos);

		assert(rt_renderer != nullptr);
		ImGui::Begin("OverLay", nullptr, windowFlags);
		ImGui::Text("Statistics");
		ImGui::Text("Rendering Time: %.3f ms", rt_renderer->GetRenderingTime());
		ImGui::Text("Top BVH Building Time: %.3f ms", scene_manager->GetTopBVHTime());
		ImGui::Text("Bottom BVH Building Time: %.3f ms", scene_manager->GetBottomBVHTime());
		ImGui::End();
	}

	inline void UserInterface::RenderRTMenu()
	{
		if (!m_show_render_menu) { return; }
		ImGui::Begin("Render Setting");
		assert(rt_renderer != nullptr);
		RenderSetting &render_setting = rt_renderer->GetRenderSetting();
		bool &highlight_flag = rt_renderer->GetObjectHighlightFlag();

		int accumulate_frame_count = render_setting.max_frame_count > 0 ? glm::min(rt_renderer->GetFrameCount(), render_setting.max_frame_count) : rt_renderer->GetFrameCount();
		ImGui::Text("Accumulate Frame Count: %d", accumulate_frame_count);
		ImGui::ProgressBar(render_setting.max_frame_count > 0? float(accumulate_frame_count) / float(render_setting.max_frame_count) : 1.0f);
		if (render_setting.max_frame_count > 0 && accumulate_frame_count / render_setting.max_frame_count == 1)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(250, 4, 4, 255));
			ImGui::Text("Render Finish!");
			ImGui::PopStyleColor();
		}

		static int sampler_idx = 0;
		static const char* sampler_names[3] = { "PCG", "Halton", "Sobol"};
		if (ImGui::BeginCombo("Sampler", sampler_names[sampler_idx]))
		{
			for (int sampler_type = PCG_SAMPLER; sampler_type <= SOBOL_SAMPLER; ++sampler_type)
			{
				if (ImGui::Selectable(sampler_names[sampler_type])) 
				{ 
					sampler_idx = sampler_type;
					render_setting.sampler_type = sampler_type;

					rt_renderer->SetRenderSettingChange(true);
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Separator();
		ImGui::Checkbox("Disable Selected Effect", &highlight_flag);
		if (ImGui::Checkbox("Distant Light", &render_setting.enable_distant_light))
		{
			rt_renderer->SetRenderSettingChange(true);
		}
		if (ImGui::Checkbox("Env Light", &render_setting.enable_env_light))
		{
			rt_renderer->SetRenderSettingChange(true);
		}
		if (ImGui::Checkbox("Volume Scatter", &render_setting.enable_volume_scattering))
		{
			rt_renderer->SetRenderSettingChange(true);
		}
		if (ImGui::Checkbox("Russian Roulette", &render_setting.enable_russian_roulette))
		{
			rt_renderer->SetRenderSettingChange(true);
		}

		ImGui::Separator();
		if (ImGui::InputInt("Sample Count", &render_setting.ssp))
		{
			render_setting.ssp = glm::clamp(render_setting.ssp, 1, 16);
			rt_renderer->SetRenderSettingChange(true);
		}
		if (ImGui::InputInt("Max Accumulate Frame Count", &render_setting.max_frame_count))
		{
			rt_renderer->SetRenderSettingChange(true);
		}
		if (ImGui::InputInt("Ray Depth", &render_setting.ray_depth))
		{
			render_setting.ray_depth = glm::clamp(render_setting.ray_depth, 1, 10);
			rt_renderer->SetRenderSettingChange(true);
		}
		if (ImGui::InputFloat("Exposure", &render_setting.exposure))
		{
			render_setting.exposure = glm::max(0.0f, render_setting.exposure);
			rt_renderer->SetRenderSettingChange(true);
		}
		if (ImGui::InputFloat("Gamma", &render_setting.gamma))
		{
			render_setting.gamma = glm::max(0.0f, render_setting.gamma);
			rt_renderer->SetRenderSettingChange(true);
		}
		ImGui::End();
	}

	inline void UserInterface::RenderCameraMenu()
	{
		if (!m_show_camera_menu) { return; }
		ImGui::Begin("Camera Setting");
		if (ImGui::Checkbox("Lock Camera", &m_lock_camera)) {}
		if (ImGui::InputFloat("Move Speed", &trackball.move_speed))
		{}
		if (ImGui::InputFloat("Rotate Speed", &trackball.rotate_speed))
		{}
		ImGui::End();
	}

	inline void UserInterface::RenderGeometryList()
	{
		if (!m_show_geometry_list) { return; }
		ImGui::Begin("Geometry List");

		const std::vector<GeometryData> &geometries = scene_manager->GetGeometries();
		const std::vector<uint32_t> &geometry_counters = scene_manager->GetGeometryCounters();
		const std::vector<std::string> &geometry_names = scene_manager->GetGeometryNames();

		ImGui::Text("Add Geometry Shape:");
		static const uint32_t basic_geometry_count = 2;
		static const char *geometry_type_names[basic_geometry_count] =
		{
			"Cube", "Sphere"
		};
		static uint32_t selected_geometry_type = 0;

		if (ImGui::BeginCombo("Type", geometry_type_names[selected_geometry_type]))
		{
			for (uint32_t geo_type = 0; geo_type < basic_geometry_count; ++geo_type)
			{
				if (ImGui::Selectable(geometry_type_names[geo_type])) { selected_geometry_type = geo_type; }
			}
			ImGui::EndCombo();
		}
		ImGui::Separator();

		// show different create menu according to selected_geometry_type...
		
		static char geometry_name_buf[32];
		if (selected_geometry_type == 0) // create a cube
		{
			ImGui::InputText("Cube Name", geometry_name_buf, 32);
			if (ImGui::Button("Create Cube", ImGui::GetItemRectSize()))
			{
				scene_manager->AddGeometry(std::string(geometry_name_buf), GeometryData().InitCube());
			}
		}
		else if (selected_geometry_type == 1) // create a sphere
		{
			ImGui::InputText("Sphere Name", geometry_name_buf, 32);
			static float sphere_radius = 1.0;
			if (ImGui::InputFloat("Radius", &sphere_radius))
			{
				sphere_radius = glm::max(0.0001f, sphere_radius);
			}

			if (ImGui::Button("Create Sphere", ImGui::GetItemRectSize()))
			{
				 scene_manager->AddGeometry(std::string(geometry_name_buf), GeometryData().InitSphere(sphere_radius));
			}
		}
		ImGui::Separator();

		ImGui::Text("Total Geometry Count: %d", (uint32_t)geometries.size());
		ImGui::Text("Geometries:");

		static uint32_t display_geometry_info_idx = EMPTY_UINT32;
		display_geometry_info_idx = glm::clamp(display_geometry_info_idx, 0u, (uint32_t)geometries.size() - 1);

		if (ImGui::BeginListBox(""))
		{
			ImVec2 button_size = ImGui::GetItemRectSize();
			for (uint32_t geo_idx = 0; geo_idx < (uint32_t)geometries.size(); ++geo_idx)
			{
				bool button_press = false;
				if (geo_idx == display_geometry_info_idx)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(250, 4, 4, 122));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(250, 4, 4, 255));
					button_press = ImGui::Button(geometry_names[geo_idx].c_str(), button_size);
					ImGui::PopStyleColor();
					ImGui::PopStyleColor();
				}
				else
				{
					button_press = ImGui::Button(geometry_names[geo_idx].c_str(), button_size);
				}

				if (button_press) { display_geometry_info_idx = geo_idx; }
			}
			ImGui::EndListBox();
		}

		ImGui::Separator();
		if (!geometries.empty() && display_geometry_info_idx != EMPTY_UINT32)
		{
			ImGui::Text("Geometry Index: %d", display_geometry_info_idx);
			ImGui::Text("Geometry Type: %s", geometries[display_geometry_info_idx].geometry_type == TRIANGLE_MESH ? "Mesh" : "Sphere");
			ImGui::Text("Reference Instance Count: %d", geometry_counters[display_geometry_info_idx]);
			if (ImGui::Button("Delete The Selected"))
			{
				const uint32_t prim_count = scene_manager->GetPrimInstances().size();
				display_geometry_info_idx = scene_manager->RemoveGeometry(display_geometry_info_idx);
				if (prim_count != scene_manager->GetPrimInstances().size())
				{
					m_click_prim_idx = EMPTY_UINT32;
				}
			}
		}

		ImGui::End();
	}

	inline void UserInterface::RenderMaterialList()
	{
		if (!m_show_material_list) { return; }
		ImGui::Begin("Material List");

		const std::vector<Material> &materials = scene_manager->GetMaterials();
		const std::vector<uint32_t> &material_counters = scene_manager->GetMaterialCounters();
		const std::vector<std::string> &material_names = scene_manager->GetMaterialNames();

		const PrimitiveInstance *prim_ptr = scene_manager->GetPrimitiveInstance(m_click_prim_idx);
		const uint32_t selected_prim_material_idx = prim_ptr != nullptr ? (*prim_ptr).material_idx : EMPTY_UINT32;

		ImGui::Text("Total Material Count: %d", (uint32_t)materials.size());
		ImGui::Text("Materials:");

		static uint32_t highlight_material_idx = EMPTY_UINT32;
		highlight_material_idx = glm::clamp(highlight_material_idx, 0u, (uint32_t)materials.size() - 1);
		ImVec2 button_size;
		if (ImGui::BeginListBox(""))
		{
			button_size = ImGui::GetItemRectSize();
			for (uint32_t mtl_idx = 0; mtl_idx < (uint32_t)materials.size(); ++mtl_idx)
			{
				bool button_press = false;
				if (mtl_idx == highlight_material_idx && mtl_idx == selected_prim_material_idx)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(215, 92, 92, 151));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(250, 127, 127, 250));
					button_press = ImGui::Button(material_names[mtl_idx].c_str(), button_size);
					ImGui::PopStyleColor();
					ImGui::PopStyleColor();
				}
				else if (mtl_idx == highlight_material_idx)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(250, 4, 4, 122));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(250, 4, 4, 255));
					button_press = ImGui::Button(material_names[mtl_idx].c_str(), button_size);
					ImGui::PopStyleColor();
					ImGui::PopStyleColor();
				}
				else if (mtl_idx == selected_prim_material_idx)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(180, 180, 180, 180));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(250, 250, 250, 250));
					button_press = ImGui::Button(material_names[mtl_idx].c_str(), button_size);
					ImGui::PopStyleColor();
					ImGui::PopStyleColor();
				}
				else
				{
					button_press = ImGui::Button(material_names[mtl_idx].c_str(), button_size);
				}

				if (button_press) { highlight_material_idx = mtl_idx; }

				if (ImGui::BeginPopupContextItem())
				{
					if (ImGui::Button("Assign To Prim"))
					{
						scene_manager->AssignMaterialToPrim(m_click_prim_idx, mtl_idx);
					}
					if (ImGui::Button("Close", ImGui::GetItemRectSize()))
					{
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				}
			}
			ImGui::EndListBox();
		}
		
		static char material_name_buf[32];
		ImGui::InputText("Material Name", material_name_buf, 32);
		if (ImGui::Button("Add", ImVec2(button_size.x * 0.48f, button_size.y)))
		{
			highlight_material_idx =  scene_manager->AddSurfaceMaterial(std::string(material_name_buf));
		}
		ImGui::SameLine();
		if (ImGui::Button("Remove", ImVec2(button_size.x * 0.48f, button_size.y)))
		{
			highlight_material_idx = scene_manager->RemoveMaterial(highlight_material_idx);
		}

		ImGui::Separator();
		if (!materials.empty() && highlight_material_idx != EMPTY_UINT32)
		{
			ImGui::Text("Material Index: %d", highlight_material_idx);
			ImGui::Text("Reference Instance Count: %d", material_counters[highlight_material_idx]);
		}

		ImGui::Separator();
		Material *mtl_ptr = scene_manager->GetMaterial(highlight_material_idx);
		if (mtl_ptr != nullptr)
		{
			Material &highlight_material = (*mtl_ptr);
			ImGui::Text("Material Attribute");

			static const char* material_type_names[] = { "surface material", "light material" };
			if (ImGui::BeginCombo("Material Type", material_type_names[highlight_material.material_type]))
			{
				for (uint32_t i = MATERIAL_TYPE::DEFAULT_MTL; i <= MATERIAL_TYPE::LIGHT_MTL; ++i)
				{
					if (ImGui::Selectable(material_type_names[i])) 
					{ 
						highlight_material.material_type = (MATERIAL_TYPE)i;
						scene_manager->UpdateMaterial(highlight_material_idx);
					}
				}
				ImGui::EndCombo();
			}

			if (highlight_material.material_type == DEFAULT_MTL)
			{
				ImGui::Separator();
				if (ImGui::ColorEdit3("Diffuse Albedo", (float*)(&highlight_material.default_mtl.diffuse_albedo), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR))
				{
					scene_manager->UpdateMaterial(highlight_material_idx);
				}
				if (ImGui::SliderFloat("Metalness", &highlight_material.default_mtl.metalness, 0.0f, 1.0f))
				{
					scene_manager->UpdateMaterial(highlight_material_idx);
				}

				ImGui::Separator();
				if (ImGui::SliderFloat("Specular Weight", &highlight_material.default_mtl.specular_weight, 0.0f, 1.0f))
				{
					scene_manager->UpdateMaterial(highlight_material_idx);
				}
				if (ImGui::ColorEdit3("Specular Albedo", (float*)(&highlight_material.default_mtl.specular_albedo), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR))
				{
					scene_manager->UpdateMaterial(highlight_material_idx);
				}
				if (ImGui::InputFloat("IOR", &highlight_material.default_mtl.ior_n))
				{
					highlight_material.default_mtl.ior_n = glm::max(0.0f, highlight_material.default_mtl.ior_n);
					scene_manager->UpdateMaterial(highlight_material_idx);
				}
				if (ImGui::InputInt("IOR Priority", (int*)&highlight_material.default_mtl.ior_priority))
				{
					highlight_material.default_mtl.ior_priority = glm::max(highlight_material.default_mtl.ior_priority, 0u);
					scene_manager->UpdateMaterial(highlight_material_idx);
				}
				if (ImGui::SliderFloat("Roughness X", &highlight_material.default_mtl.alpha_x, 0.0f, 1.0f))
				{
					scene_manager->UpdateMaterial(highlight_material_idx);
				}
				if (ImGui::SliderFloat("Roughness Y", &highlight_material.default_mtl.alpha_y, 0.0f, 1.0f))
				{
					scene_manager->UpdateMaterial(highlight_material_idx);
				}

				ImGui::Separator();
				if (ImGui::SliderFloat("Transmission Weight", &highlight_material.default_mtl.transmission_weight, 0.0f, 1.0f))
				{
					scene_manager->UpdateMaterial(highlight_material_idx);
				}
			}
			else if (highlight_material.material_type == LIGHT_MTL)
			{
				ImGui::Separator();
				if (ImGui::ColorEdit3("Light Color", (float*)(&highlight_material.light_mtl.light_color), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR))
				{
					scene_manager->UpdateMaterial(highlight_material_idx);
				}
				if (ImGui::InputFloat("Intensity", &highlight_material.light_mtl.intensity))
				{
					highlight_material.light_mtl.intensity = glm::max(0.0f, highlight_material.light_mtl.intensity);
					scene_manager->UpdateMaterial(highlight_material_idx);
				}
			}
			else
			{
			
			}
		}

		ImGui::End();
	}

	inline void UserInterface::RenderLightList()
	{
		if (!m_show_light_menu) { return; }
		ImGui::Begin("Light List");
		const std::vector<DistantLight> &distant_lights = scene_manager->GetDistantLights();
		const std::vector<std::string> &distant_light_names = scene_manager->GetDistantLightNames();
		
		ImGui::Text("Total DistantLight Count: %d", (uint32_t)distant_lights.size());
		ImGui::Text("DistantLights:");

		static uint32_t highlight_distant_light_idx = EMPTY_UINT32;
		highlight_distant_light_idx = glm::clamp(highlight_distant_light_idx, 0u, (uint32_t)distant_lights.size() - 1);
		ImVec2 button_size;
		if (ImGui::BeginListBox(""))
		{
			button_size = ImGui::GetItemRectSize();
			for (uint32_t distant_light_idx = 0; distant_light_idx < (uint32_t)distant_lights.size(); ++distant_light_idx)
			{
				bool button_press = false;
				if (distant_light_idx == highlight_distant_light_idx)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(250, 4, 4, 122));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(250, 4, 4, 255));
					button_press = ImGui::Button(distant_light_names[distant_light_idx].c_str(), button_size);
					ImGui::PopStyleColor();
					ImGui::PopStyleColor();
				}
				else
				{
					button_press = ImGui::Button(distant_light_names[distant_light_idx].c_str(), button_size);
				}

				if (button_press) { highlight_distant_light_idx = distant_light_idx; }
			}
			ImGui::EndListBox();
		}

		static char distant_light_name_buf[32];
		ImGui::InputText("Distant Light Name", distant_light_name_buf, 32);
		if (ImGui::Button("Add", ImVec2(button_size.x * 0.48f, button_size.y)))
		{
			uint32_t instance_transform_idx = scene_manager->AddTransform();
			highlight_distant_light_idx = scene_manager->AddDistantLight(std::string(distant_light_name_buf), instance_transform_idx);
		}
		ImGui::SameLine();
		if (ImGui::Button("Remove", ImVec2(button_size.x * 0.48f, button_size.y)))
		{
			highlight_distant_light_idx = scene_manager->RemoveDistantLight(highlight_distant_light_idx);
		}

		ImGui::Separator();
		DistantLight *distant_light_ptr = scene_manager->GetDistantLight(highlight_distant_light_idx);
		if (distant_light_ptr != nullptr)
		{
			DistantLight &highlight_distant_light = (*distant_light_ptr);
			ImGui::Text("Distant Light Attribute");
			if (ImGui::ColorEdit3("Light Color", (float*)(&highlight_distant_light.light_color), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR))
			{
				scene_manager->UpdateDistantLight(highlight_distant_light_idx);
			}
			if (ImGui::InputFloat("Intensity", &highlight_distant_light.intensity))
			{
				highlight_distant_light.intensity = glm::max(0.0f, highlight_distant_light.intensity);
				scene_manager->UpdateDistantLight(highlight_distant_light_idx);
			}
			if (ImGui::SliderFloat("Max Scatter Angle", &highlight_distant_light.theta_max, 0.01f, 89.99f))
			{
				highlight_distant_light.cos_theta_max = glm::cos(glm::radians(highlight_distant_light.theta_max));
				scene_manager->UpdateDistantLight(highlight_distant_light_idx);
			}
			// TODO : distant_light transform
			const uint32_t transform_idx = highlight_distant_light.transform_idx;
			TransformState *transform_state_ptr = scene_manager->GetTransformState(transform_idx);
			if (transform_state_ptr != nullptr)
			{
				TransformState &transform_state = *transform_state_ptr;
				if (ImGui::SliderFloat("Rotate X", &transform_state.R.x, 0.0f, 360.0f))
				{
					scene_manager->UpdateDistantLightTransform(highlight_distant_light_idx);
				}
				if (ImGui::SliderFloat("Rotate Y", &transform_state.R.y, 0.0f, 360.0f))
				{
					scene_manager->UpdateDistantLightTransform(highlight_distant_light_idx);
				}
				if (ImGui::SliderFloat("Rotate Z", &transform_state.R.z, 0.0f, 360.0f))
				{
					scene_manager->UpdateDistantLightTransform(highlight_distant_light_idx);
				}
			}
		}
		ImGui::End();
	}

	inline void UserInterface::RenderVolumeList()
	{
		if (!m_show_volume_menu) { return; }
		ImGui::Begin("Volume List");

		const std::vector<Volume> &volumes = scene_manager->GetVolumes();
		const std::vector<std::string> &volume_names = scene_manager->GetVolumeNames();

		ImGui::Text("Total Volume Count: %d", (uint32_t)volumes.size());
		ImGui::Text("Volumes:");

		static uint32_t highlight_volume_idx = EMPTY_UINT32;
		highlight_volume_idx = glm::clamp(highlight_volume_idx, 0u, (uint32_t)volumes.size() - 1);
		ImVec2 button_size;
		if (ImGui::BeginListBox(""))
		{
			button_size = ImGui::GetItemRectSize();
			for (uint32_t vol_idx = 0; vol_idx < (uint32_t)volumes.size(); ++vol_idx)
			{
				bool button_press = false;
				if (vol_idx == highlight_volume_idx)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(250, 4, 4, 122));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(250, 4, 4, 255));
					button_press = ImGui::Button(volume_names[vol_idx].c_str(), button_size);
					ImGui::PopStyleColor();
					ImGui::PopStyleColor();
				}
				else
				{
					button_press = ImGui::Button(volume_names[vol_idx].c_str(), button_size);
				}

				if (button_press) { highlight_volume_idx = vol_idx; }
			}
			ImGui::EndListBox();
		}

		static char volume_name_buf[32];
		ImGui::InputText("Volume Name", volume_name_buf, 32);
		if (ImGui::Button("Add", ImVec2(button_size.x * 0.48f, button_size.y)))
		{
			uint32_t vol_transform_idx = scene_manager->AddTransform();
			highlight_volume_idx = scene_manager->AddVolume(std::string(volume_name_buf), vol_transform_idx);
		}
		ImGui::SameLine();
		if (ImGui::Button("Remove", ImVec2(button_size.x * 0.48f, button_size.y)))
		{
			highlight_volume_idx = scene_manager->RemoveVolume(highlight_volume_idx);
		}

		ImGui::Separator();
		Volume *volume_ptr = scene_manager->GetVolume(highlight_volume_idx);
		if (volume_ptr != nullptr)
		{
			Volume &volume = (*volume_ptr);
			ImGui::Text("Volume Attribute");
			ImGui::Text("Density Texture: %d", volume.density_texture_idx);
			if (ImGui::ColorEdit3("SigmaT", (float*)(&volume.sigma_t), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR))
			{
				scene_manager->UpdateVolume(highlight_volume_idx);
			}
			if (ImGui::ColorEdit3("Albedo ", (float*)(&volume.albedo), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR))
			{
				volume.albedo = glm::min(glm::vec3(1.0f), volume.albedo);
				scene_manager->UpdateVolume(highlight_volume_idx);
			}
			if (ImGui::SliderFloat("Anisotropy", &volume.g, -0.9999f, 0.9999f)) 
			{
				scene_manager->UpdateVolume(highlight_volume_idx);
			}

			const uint32_t transform_idx = volume.transform_idx;
			TransformState *transform_state_ptr = scene_manager->GetTransformState(transform_idx);
			if (transform_state_ptr != nullptr)
			{
				TransformState &transform_state = *transform_state_ptr;
				if (ImGui::SliderFloat("Rotate X", &transform_state.R.x, 0.0f, 360.0f))
				{
					scene_manager->UpdateVolumeTransform(highlight_volume_idx);
				}
				if (ImGui::SliderFloat("Rotate Y", &transform_state.R.y, 0.0f, 360.0f))
				{
					scene_manager->UpdateVolumeTransform(highlight_volume_idx);
				}
				if (ImGui::SliderFloat("Rotate Z", &transform_state.R.z, 0.0f, 360.0f))
				{
					scene_manager->UpdateVolumeTransform(highlight_volume_idx);
				}
				if (ImGui::InputFloat3("Translate", (float*)(&transform_state.T)))
				{
					scene_manager->UpdateVolumeTransform(highlight_volume_idx);
				}
				if (ImGui::InputFloat3("Scale", (float*)(&transform_state.S)))
				{
					transform_state.S = glm::vec3(glm::max(0.0001f, transform_state.S.x), glm::max(0.0001f, transform_state.S.y), glm::max(0.0001f, transform_state.S.z));
					scene_manager->UpdateVolumeTransform(highlight_volume_idx);
				}
			}
		}
		
		ImGui::End();
	}

	inline void UserInterface::RenderTextureMenu()
	{
		if (!m_show_texture_menu) { return; }
		ImGui::Begin("Texture List");

		const std::vector<Texture> &textures = scene_manager->GetTextures();
		const std::vector<std::string> &texture_names = scene_manager->GetTextureNames();

		ImGui::Text("Total Texture Count: %d", (uint32_t)textures.size());
		ImGui::Text("Textures:");

		static uint32_t highlight_tex_idx = EMPTY_UINT32;
		highlight_tex_idx = glm::clamp(highlight_tex_idx, 0u, (uint32_t)textures.size() - 1);
		ImVec2 button_size;
		if (ImGui::BeginListBox(""))
		{
			button_size = ImGui::GetItemRectSize();
			for (uint32_t tex_idx = 0; tex_idx < (uint32_t)textures.size(); ++tex_idx)
			{
				bool button_press = false;
				if (tex_idx == highlight_tex_idx)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(250, 4, 4, 122));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(250, 4, 4, 255));
					button_press = ImGui::Button(texture_names[tex_idx].c_str(), button_size);
					ImGui::PopStyleColor();
					ImGui::PopStyleColor();
				}
				else
				{
					button_press = ImGui::Button(texture_names[tex_idx].c_str(), button_size);
				}

				if (button_press) { highlight_tex_idx = tex_idx; }
				
				if (ImGui::BeginPopupContextItem()) 
				{
					ImGui::Text("Texture Attributes:");
					Texture &texture = *(scene_manager->GetTexture(tex_idx));

					ImGui::Text("Texture index: %d", tex_idx);
					ImGui::Text("Texture type: %s", texture_type_names[texture.texture_type]);
					if (texture.texture_type == IMAGE_TEXTURE)
					{
						ImageTexture& image_tex = texture.image_texture;
						ImGui::Text("Resolution: [%d, %d]", image_tex.width, image_tex.height);

						static const std::vector<std::string> &warp_modes{ std::string("repeat"), std::string("clamp") };
						if (ImGui::BeginCombo("Warp Mode", warp_modes[image_tex.warp_mode].c_str()))
						{
							for (int mode = WARP_MODE_REPEAT; mode <= WARP_MODE_CLAMP; ++mode)
							{
								if (ImGui::Selectable(warp_modes[mode].c_str())) {
									image_tex.warp_mode = glm::clamp(mode, (int)WARP_MODE_REPEAT, (int)WARP_MODE_CLAMP);
									scene_manager->UpdateTexture(tex_idx);
								}
							}
							ImGui::EndCombo();
						}
						if (ImGui::InputFloat("u scale", &image_tex.u_scale))
						{
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("v scale", &image_tex.v_scale))
						{
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("u offset", &image_tex.u_offset))
						{
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("v offset", &image_tex.v_offset))
						{
							scene_manager->UpdateTexture(tex_idx);
						}
					}
					else if (texture.texture_type == CONSTANT_TEXTURE_FLOAT)
					{
						ConstantTextureFloat& const_float_tex = texture.constant_texture_float;
						if (ImGui::InputFloat("value", &const_float_tex.value)) 
						{
							scene_manager->UpdateTexture(tex_idx);
						}
					}
					else if (texture.texture_type == CONSTANT_TEXTURE_RGB)
					{
						ConstantTextureRGB& constant_rgb_tex = texture.constant_texture_rgb;
						if (ImGui::ColorEdit3("color", (float*)(&constant_rgb_tex.color), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR))
						{
							scene_manager->UpdateTexture(tex_idx);
						}
					}
					else if (texture.texture_type == SOLID_TEXTURE_CHECKERBOARD)
					{
						CheckerBoardTexture &checkerboard = texture.checker_board_texture;

						// be careful! not to create cycle in recursive texture
						int max_allow_idx = glm::max((int)tex_idx - 1, 0);
						if (ImGui::InputInt("tex black", (int*)&checkerboard.child_texture_indices.texture_black)) 
						{
							checkerboard.child_texture_indices.texture_black = glm::clamp(checkerboard.child_texture_indices.texture_black, 0u, (uint32_t)max_allow_idx);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputInt("tex white", (int*)&checkerboard.child_texture_indices.texture_white))
						{
							checkerboard.child_texture_indices.texture_white = glm::clamp(checkerboard.child_texture_indices.texture_white, 0u, (uint32_t)max_allow_idx);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("frequency", &checkerboard.frequency)) 
						{
							checkerboard.frequency = glm::max(0.0001f, checkerboard.frequency);
							scene_manager->UpdateTexture(tex_idx);
						}
					}
					else if (texture.texture_type == SOLID_TEXTURE_NOISE)
					{
						NoiseTexture &perlin_noise_tex = texture.noise_texture;

						// be careful! not to create cycle in recursive texture
						int max_allow_idx = glm::max((int)tex_idx - 1, 0);
						if (ImGui::InputInt("tex black", (int*)&perlin_noise_tex.child_texture_indices.texture_black))
						{
							perlin_noise_tex.child_texture_indices.texture_black = glm::clamp(perlin_noise_tex.child_texture_indices.texture_black, 0u, (uint32_t)max_allow_idx);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputInt("tex white", (int*)&perlin_noise_tex.child_texture_indices.texture_white))
						{
							perlin_noise_tex.child_texture_indices.texture_white = glm::clamp(perlin_noise_tex.child_texture_indices.texture_white, 0u, (uint32_t)max_allow_idx);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::Checkbox("normalized", &perlin_noise_tex.normalized))
						{
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("frequency", &perlin_noise_tex.frequency))
						{
							perlin_noise_tex.frequency = glm::max(0.0001f, perlin_noise_tex.frequency);
							scene_manager->UpdateTexture(tex_idx);
						}
					}
					else if (texture.texture_type == SOLID_TEXTURE_FBM) 
					{
						NoiseTextureFBM& noise_texture_fbm = texture.noise_texture_fbm;

						int max_allow_idx = glm::max((int)tex_idx - 1, 0);
						if (ImGui::InputInt("tex black", (int*)&noise_texture_fbm.child_texture_indices.texture_black))
						{
							noise_texture_fbm.child_texture_indices.texture_black = glm::clamp(noise_texture_fbm.child_texture_indices.texture_black, 0u, (uint32_t)max_allow_idx);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputInt("tex white", (int*)&noise_texture_fbm.child_texture_indices.texture_white))
						{
							noise_texture_fbm.child_texture_indices.texture_white = glm::clamp(noise_texture_fbm.child_texture_indices.texture_white, 0u, (uint32_t)max_allow_idx);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("amplitude", &noise_texture_fbm.amplitude))
						{
							noise_texture_fbm.amplitude = glm::max(noise_texture_fbm.amplitude, 0.0f);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("offset", &noise_texture_fbm.offset))
						{
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("frequency", &noise_texture_fbm.frequency))
						{
							noise_texture_fbm.frequency = glm::max(0.0001f, noise_texture_fbm.frequency);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("lacunarity", &noise_texture_fbm.lacunarity)) 
						{
							noise_texture_fbm.lacunarity = glm::max(0.0001f, noise_texture_fbm.lacunarity);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("gain", &noise_texture_fbm.gain))
						{
							noise_texture_fbm.gain = glm::max(0.0001f, noise_texture_fbm.gain);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputInt("octave", &noise_texture_fbm.layer_count))
						{
							noise_texture_fbm.layer_count = glm::max(noise_texture_fbm.layer_count, 1);
							scene_manager->UpdateTexture(tex_idx);
						}
					}
					else if (texture.texture_type == SOLID_TEXTURE_TURBULENCE)
					{
						NoiseTextureTurbulence& noise_texture_turbulence = texture.noise_texture_turbulence;

						int max_allow_idx = glm::max((int)tex_idx - 1, 0);
						if (ImGui::InputInt("tex black", (int*)&noise_texture_turbulence.child_texture_indices.texture_black))
						{
							noise_texture_turbulence.child_texture_indices.texture_black = glm::clamp(noise_texture_turbulence.child_texture_indices.texture_black, 0u, (uint32_t)max_allow_idx);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputInt("tex white", (int*)&noise_texture_turbulence.child_texture_indices.texture_white))
						{
							noise_texture_turbulence.child_texture_indices.texture_white = glm::clamp(noise_texture_turbulence.child_texture_indices.texture_white, 0u, (uint32_t)max_allow_idx);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("amplitude", &noise_texture_turbulence.amplitude))
						{
							noise_texture_turbulence.amplitude = glm::max(noise_texture_turbulence.amplitude, 0.0f);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("offset", &noise_texture_turbulence.offset))
						{
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("frequency", &noise_texture_turbulence.frequency))
						{
							noise_texture_turbulence.frequency = glm::max(0.0001f, noise_texture_turbulence.frequency);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("lacunarity", &noise_texture_turbulence.lacunarity))
						{
							noise_texture_turbulence.lacunarity = glm::max(0.0001f, noise_texture_turbulence.lacunarity);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("gain", &noise_texture_turbulence.gain))
						{
							noise_texture_turbulence.gain = glm::max(0.0001f, noise_texture_turbulence.gain);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputInt("octave", &noise_texture_turbulence.layer_count))
						{
							noise_texture_turbulence.layer_count = glm::max(noise_texture_turbulence.layer_count, 1);
							scene_manager->UpdateTexture(tex_idx);
						}
					}
					else if (texture.texture_type == SOLID_TEXTURE_MARBLE)
					{
						NoiseTextureMarble& noise_texture_marble = texture.noise_texture_marble;

						int max_allow_idx = glm::max((int)tex_idx - 1, 0);
						if (ImGui::InputInt("tex black", (int*)&noise_texture_marble.child_texture_indices.texture_black))
						{
							noise_texture_marble.child_texture_indices.texture_black = glm::clamp(noise_texture_marble.child_texture_indices.texture_black, 0u, (uint32_t)max_allow_idx);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputInt("tex white", (int*)&noise_texture_marble.child_texture_indices.texture_white))
						{
							noise_texture_marble.child_texture_indices.texture_white = glm::clamp(noise_texture_marble.child_texture_indices.texture_white, 0u, (uint32_t)max_allow_idx);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("variation", &noise_texture_marble.variation))
						{
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::Checkbox("x turbulence", &noise_texture_marble.x_turb))
						{
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::Checkbox("y turbulence", &noise_texture_marble.y_turb))
						{
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::Checkbox("z turbulence", &noise_texture_marble.z_turb))
						{
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("frequency", &noise_texture_marble.frequency))
						{
							noise_texture_marble.frequency = glm::max(0.0001f, noise_texture_marble.frequency);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("lacunarity", &noise_texture_marble.lacunarity))
						{
							noise_texture_marble.lacunarity = glm::max(0.0001f, noise_texture_marble.lacunarity);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("gain", &noise_texture_marble.gain))
						{
							noise_texture_marble.gain = glm::max(0.0001f, noise_texture_marble.gain);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputInt("octave", &noise_texture_marble.layer_count))
						{
							noise_texture_marble.layer_count = glm::max(noise_texture_marble.layer_count, 1);
							scene_manager->UpdateTexture(tex_idx);
						}
					}
					else if (texture.texture_type == SOLID_TEXTURE_WOOD)
					{
						NoiseTextureWood& noise_texture_wood = texture.noise_texture_wood;

						int max_allow_idx = glm::max((int)tex_idx - 1, 0);
						if (ImGui::InputInt("tex black", (int*)&noise_texture_wood.child_texture_indices.texture_black))
						{
							noise_texture_wood.child_texture_indices.texture_black = glm::clamp(noise_texture_wood.child_texture_indices.texture_black, 0u, (uint32_t)max_allow_idx);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputInt("tex white", (int*)&noise_texture_wood.child_texture_indices.texture_white))
						{
							noise_texture_wood.child_texture_indices.texture_white = glm::clamp(noise_texture_wood.child_texture_indices.texture_white, 0u, (uint32_t)max_allow_idx);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("variation", &noise_texture_wood.variation))
						{
							noise_texture_wood.variation = glm::max(0.f, noise_texture_wood.variation);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("frequency", &noise_texture_wood.frequency))
						{
							noise_texture_wood.frequency = glm::max(0.0001f, noise_texture_wood.frequency);
							scene_manager->UpdateTexture(tex_idx);
						}
					}
					else if (texture.texture_type == SOLID_TEXTURE_POLKA_DOT)
					{
						NoiseTexturePolkaDot& noise_texture_polka_dot = texture.noise_texture_polka_dot;

						int max_allow_idx = glm::max((int)tex_idx - 1, 0);
						if (ImGui::InputInt("tex black", (int*)&noise_texture_polka_dot.child_texture_indices.texture_black))
						{
							noise_texture_polka_dot.child_texture_indices.texture_black = glm::clamp(noise_texture_polka_dot.child_texture_indices.texture_black, 0u, (uint32_t)max_allow_idx);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputInt("tex white", (int*)&noise_texture_polka_dot.child_texture_indices.texture_white))
						{
							noise_texture_polka_dot.child_texture_indices.texture_white = glm::clamp(noise_texture_polka_dot.child_texture_indices.texture_white, 0u, (uint32_t)max_allow_idx);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("radius", &noise_texture_polka_dot.radius))
						{
							noise_texture_polka_dot.radius = glm::clamp(noise_texture_polka_dot.radius, 0.0f, 0.5f);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("frequency", &noise_texture_polka_dot.frequency))
						{
							noise_texture_polka_dot.frequency = glm::max(0.0001f, noise_texture_polka_dot.frequency);
							scene_manager->UpdateTexture(tex_idx);
						}
					}
					else if (texture.texture_type == SOLID_TEXTURE_WAVE)
					{
						NoiseTextureWave& noise_texture_wave = texture.noise_texture_wave;

						int max_allow_idx = glm::max((int)tex_idx - 1, 0);
						if (ImGui::InputInt("tex black", (int*)&noise_texture_wave.child_texture_indices.texture_black))
						{
							noise_texture_wave.child_texture_indices.texture_black = glm::clamp(noise_texture_wave.child_texture_indices.texture_black, 0u, (uint32_t)max_allow_idx);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputInt("tex white", (int*)&noise_texture_wave.child_texture_indices.texture_white))
						{
							noise_texture_wave.child_texture_indices.texture_white = glm::clamp(noise_texture_wave.child_texture_indices.texture_white, 0u, (uint32_t)max_allow_idx);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("frequency0", &noise_texture_wave.freq_0))
						{
							noise_texture_wave.freq_0 = glm::max(0.0001f, noise_texture_wave.freq_0);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("lacunarity0", &noise_texture_wave.lacunarity_0))
						{
							noise_texture_wave.lacunarity_0 = glm::max(0.0001f, noise_texture_wave.lacunarity_0);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("gain0", &noise_texture_wave.gain_0))
						{
							noise_texture_wave.gain_0 = glm::max(0.0001f, noise_texture_wave.gain_0);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputInt("octave0", &noise_texture_wave.layer_count_0))
						{
							noise_texture_wave.layer_count_0 = glm::max(noise_texture_wave.layer_count_0, 1);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("frequency1", &noise_texture_wave.freq_1))
						{
							noise_texture_wave.freq_1 = glm::max(0.0001f, noise_texture_wave.freq_1);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("lacunarity1", &noise_texture_wave.lacunarity_1))
						{
							noise_texture_wave.lacunarity_1 = glm::max(0.0001f, noise_texture_wave.lacunarity_1);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputFloat("gain1", &noise_texture_wave.gain_1))
						{
							noise_texture_wave.gain_1 = glm::max(0.0001f, noise_texture_wave.gain_1);
							scene_manager->UpdateTexture(tex_idx);
						}
						if (ImGui::InputInt("octave1", &noise_texture_wave.layer_count_1))
						{
							noise_texture_wave.layer_count_1 = glm::max(noise_texture_wave.layer_count_1, 1);
							scene_manager->UpdateTexture(tex_idx);
						}
					}
					else
					{
						
					}
					
					if (ImGui::Button("Close")) 
					{
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				}
			}
			ImGui::EndListBox();
		}

		ImGui::End();
	}

	inline void UserInterface::RenderInstanceMenu()
	{
		if (!m_show_move_control_menu) { return; }
		ImGui::Begin("Control Menu");
		ImGui::Text("Total Primitive Instance Count: %d", (uint32_t)scene_manager->GetPrimInstances().size());
		ImGui::Text("Highlighted Instance ID: %d", m_click_prim_idx == EMPTY_UINT32 ? (int)(-1) : (uint32_t)m_click_prim_idx);

		PrimitiveInstance *prim_ptr = scene_manager->GetPrimitiveInstance(m_click_prim_idx);
		if (prim_ptr != nullptr)
		{
			PrimitiveInstance &prim_inst = (*prim_ptr);
			if (ImGui::InputInt("Internal Volume Index:", &prim_inst.inner_volume_idx))
			{
				scene_manager->UpdatePrimVolume(m_click_prim_idx);
			}
			bool treat_as_boundary = (bool)prim_inst.treat_as_boundary;
			if (ImGui::Checkbox("Treat as Volume Boundary", &treat_as_boundary))
			{
				prim_inst.treat_as_boundary = treat_as_boundary ? 1 : 0;
				scene_manager->UpdatePrimVolume(m_click_prim_idx);
			}
			ImGui::Text("Instance Geometry: %s", scene_manager->GetGeometryNames()[prim_inst.geometry_idx].c_str());
			ImGui::Text("Instance Material: %s", scene_manager->GetMaterialNames()[prim_inst.material_idx].c_str());
		}
		
		static float scale_upper = 10.0f;
		static float scale_lower = 0.005f;
		if (ImGui::InputFloat("Max Scale", &scale_upper)) 
		{
			scale_upper = glm::clamp(scale_upper, 2.0f * scale_lower, 1000.0f);
		}

		static float move_speed = 0.1f;
		if (ImGui::InputFloat("Move Speed", &move_speed))
		{
			move_speed = glm::max(+0.0f, move_speed);
		}
		ImGui::Separator();

		const std::vector<PrimitiveInstance> &prim_instances = scene_manager->GetPrimInstances();
		if (ImGui::CollapsingHeader("Instances")) 
		{
			ImVec2 button_size;
			if (ImGui::BeginListBox(""))
			{
				button_size = ImGui::GetItemRectSize();
				for (uint32_t instance_idx = 0; instance_idx < (uint32_t)prim_instances.size(); ++instance_idx)
				{
					bool button_press = false;
					const std::string instance_name = std::string("instance_") + std::to_string(instance_idx);
					if (instance_idx == m_click_prim_idx)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(215, 92, 92, 151));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(250, 127, 127, 250));
						button_press = ImGui::Button(instance_name.c_str(), button_size);
						ImGui::PopStyleColor();
						ImGui::PopStyleColor();
					}
					else
					{
						button_press = ImGui::Button(instance_name.c_str(), button_size);
					}

					if (button_press) { m_click_prim_idx = instance_idx; }
				}
				ImGui::EndListBox();
			}
		}
		if (ImGui::CollapsingHeader("Instance Transform"))
		{
			if (m_click_prim_idx != EMPTY_UINT32)
			{
				if (ImGui::TreeNode("Translate"))
				{
					glm::vec3 *translate_ptr = scene_manager->GetPrimTransformStateT(m_click_prim_idx);
					assert(translate_ptr != nullptr);
					
					if (translate_ptr != nullptr)
					{
						glm::vec3 &prim_translate = (*translate_ptr);
						auto draw_arrow_buttons = [&](const char *title,
																			const char *arrow_sub,
																			const char *arrow_add,
																			uint32_t i)->void
						{
							ImGui::Text(title);
							ImGui::SameLine();
							if (ImGui::ArrowButton(arrow_sub, ImGuiDir_Left))
							{
								prim_translate[i] -= move_speed;
								scene_manager->UpdatePrimTransform(m_click_prim_idx);
							}
							ImGui::SameLine();
							if (ImGui::ArrowButton(arrow_add, ImGuiDir_Right))
							{
								prim_translate[i] += move_speed;
								scene_manager->UpdatePrimTransform(m_click_prim_idx);
							}
						};

						draw_arrow_buttons("Translate X", "X-", "X+", 0);
						draw_arrow_buttons("Translate Y", "Y-", "Y+", 1);
						draw_arrow_buttons("Translate Z", "Z-", "Z+", 2);
						if (ImGui::InputFloat3("Translate", (float*)(&prim_translate)))
						{
							scene_manager->UpdatePrimTransform(m_click_prim_idx);
						}
					}
					ImGui::TreePop();
				}
				if (ImGui::TreeNode("Scale"))
				{
					glm::vec3 *scale_ptr = scene_manager->GetPrimTransformStateS(m_click_prim_idx);
					assert(scale_ptr != nullptr);
					
					if (scale_ptr != nullptr)
					{
						glm::vec3 &prim_scale = (*scale_ptr);
						auto draw_scale_slider = [&](const char *title, uint32_t i)->void
						{
							if (ImGui::SliderFloat(title, &prim_scale[i], scale_lower, scale_upper))
							{
								scene_manager->UpdatePrimTransform(m_click_prim_idx);
							}
						};

						draw_scale_slider("X scale", 0);
						draw_scale_slider("Y scale", 1);
						draw_scale_slider("Z scale", 2);
						if (ImGui::InputFloat3("Scale", (float*)(&prim_scale)))
						{
							prim_scale = glm::clamp(prim_scale, glm::vec3(scale_lower), glm::vec3(scale_upper));
							scene_manager->UpdatePrimTransform(m_click_prim_idx);
						}
					}
					ImGui::TreePop();
				}
				if (ImGui::TreeNode("Rotate"))
				{
					glm::vec3 *rotate_ptr = scene_manager->GetPrimTransformStateR(m_click_prim_idx);
					assert(rotate_ptr != nullptr);

					if (rotate_ptr != nullptr)
					{
						glm::vec3 &prim_rotate = (*rotate_ptr);
						auto draw_rotate_slider = [&](const char *title, uint32_t i)->void
						{
							if (ImGui::SliderFloat(title, &prim_rotate[i], 0.0f, 360.0f))
							{
								scene_manager->UpdatePrimTransform(m_click_prim_idx);
							}
						};

						draw_rotate_slider("X Rotate", 0);
						draw_rotate_slider("Y Rotate", 1);
						draw_rotate_slider("Z Rotate", 2);
						if (ImGui::InputFloat3("Rotate", (float*)(&prim_rotate)))
						{
							prim_rotate = glm::clamp(prim_rotate, glm::vec3(0.0f), glm::vec3(360.0f));
							scene_manager->UpdatePrimTransform(m_click_prim_idx);
						}
					}
					ImGui::TreePop();
				}
			}
		}
		if (ImGui::CollapsingHeader("Instance Manager"))
		{
			static int instance_geometry_idx = 0;
			static int instance_material_idx = 0;
			static glm::vec3 instance_transform_T(0.0f);
			static glm::vec3 instance_transform_S(1.0f);
			static glm::vec3 instance_transform_R(0.0f);

			const std::vector<GeometryData> &geometries = scene_manager->GetGeometries();
			if (ImGui::InputInt("Instance Shape Index", &instance_geometry_idx))
			{
				instance_geometry_idx = glm::clamp(instance_geometry_idx, 0, (int)geometries.size() - 1);
			}
			const std::vector<Material>&materials = scene_manager->GetMaterials();
			if (ImGui::InputInt("Instance Material Index", &instance_material_idx))
			{
				instance_material_idx = glm::clamp(instance_material_idx, 0, (int)materials.size() - 1);
			}
			if (ImGui::InputFloat3("Instance Translate", (float*)(&instance_transform_T)))
			{
				// no clamp for translate
			}
			if (ImGui::InputFloat3("Instance Scale", (float*)(&instance_transform_S)))
			{
				instance_transform_S = glm::clamp(instance_transform_S, glm::vec3(scale_lower), glm::vec3(scale_upper));
			}
			if (ImGui::InputFloat3("Instance Rotate", (float*)(&instance_transform_R)))
			{
				instance_transform_R = glm::clamp(instance_transform_R, glm::vec3(0.0f), glm::vec3(360.0f));
			}
			auto button_size = ImGui::GetItemRectSize();
			if (ImGui::Button("Add Instance", ImVec2(button_size.x * 0.5f, button_size.y)))
			{
				uint32_t instance_transform_idx = scene_manager->AddTransform(instance_transform_T, instance_transform_S, instance_transform_R);
				
				// add a new instance add assign its id to m_click_prim_idx
				m_click_prim_idx = scene_manager->AddPrimInstance(instance_geometry_idx, instance_transform_idx, instance_material_idx);
			}
			ImGui::SameLine();
			if (ImGui::Button("Delete Instance", ImVec2(button_size.x * 0.5f, button_size.y)))
			{
				if (m_click_prim_idx != EMPTY_UINT32)
				{
					m_click_prim_idx = scene_manager->RemovePrimInstance(m_click_prim_idx);
				}
			}
		}
		ImGui::End();
	}

	inline void UserInterface::RenderMainMenu()
	{
		const ImGuiViewport *viewport = ImGui::GetMainViewport();
		ImVec2 workPos = viewport->WorkPos;

		ImGui::SetNextWindowPos(ImVec2(workPos.x + 5.0f, workPos.y + 90.0f));

		static ImVec4 color;
		color.x = m_click_color.x;
		color.y = m_click_color.y;
		color.z = m_click_color.z;
		color.w = m_click_color.w;

		ImGui::Begin("Main Menu");

		ImGui::Text("Click Pixel Data");
		ImGui::Text("Pos: [%d, %d]", m_click_pixel_x, m_click_pixel_y);
		ImGui::Text("Col: [%.3f, %.3f, %.3f]", color.x, color.y, color.z);
		ImGui::SameLine();
		ImGui::ColorButton("Click color", color);

		// if window is hover, mute the camera's rotate
		m_mouse_on_GUI = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || 
										 ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);
		const std::vector<std::string> &aov_names = rt_renderer->GetAovStrings();
		if (ImGui::BeginCombo("Display", aov_names[m_display_aov_idx].c_str()))
		{
			for (uint32_t aov_idx = 0; aov_idx < (uint32_t)aov_names.size(); ++aov_idx)
			{
				if (ImGui::Selectable(aov_names[aov_idx].c_str())) { m_display_aov_idx = aov_idx; }
			}
			ImGui::EndCombo();
		}

		if (ImGui::CollapsingHeader("Render Setting"))
		{
			ImGui::Checkbox("Show Render Setting", &m_show_render_menu);
		}

		if (ImGui::CollapsingHeader("Camera Setting"))
		{
			ImGui::Checkbox("Show Camera Setting", &m_show_camera_menu);
		}

		if (ImGui::CollapsingHeader("Geometries"))
		{
			ImGui::Checkbox("Show Geometry List", &m_show_geometry_list);
		}

		if (ImGui::CollapsingHeader("Materials"))
		{
			ImGui::Checkbox("Show Material List", &m_show_material_list);
		}

		if (ImGui::CollapsingHeader("Instances"))
		{
			ImGui::Checkbox("Show Control Board", &m_show_move_control_menu);
		}

		if (ImGui::CollapsingHeader("Light"))
		{
			ImGui::Checkbox("Show Light List", &m_show_light_menu);
		}

		if (ImGui::CollapsingHeader("Volume"))
		{
			ImGui::Checkbox("Show Volume List", &m_show_volume_menu);
		}

		if (ImGui::CollapsingHeader("Texture"))
		{
			ImGui::Checkbox("Show Texture List", &m_show_texture_menu);
		}

		ImGui::End();
	}

	inline void UserInterface::ExitUI()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	inline void UserInterface::RenderUI()
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGuiIO &io = ImGui::GetIO();

		RenderOverlay();
		RenderMainMenu();
		RenderRTMenu();
		RenderCameraMenu();
		RenderInstanceMenu();
		RenderGeometryList();
		RenderMaterialList();
		RenderLightList();
		RenderVolumeList();
		RenderTextureMenu();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
}