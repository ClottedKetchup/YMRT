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

	void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

	void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

	void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);

	void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset);

	class UserInterface
	{
	public:
		friend void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

		friend void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

		friend void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);

		friend void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset);

		static std::shared_ptr<UserInterface> GetInstance()
		{
			static std::shared_ptr<UserInterface> ptr(new UserInterface());
			return ptr;
		}

		~UserInterface() {}

		void Init(GLFWwindow *window,
			uint32_t width,
			uint32_t height,
			std::shared_ptr<Renderer> & ptr_renderer,
			std::shared_ptr<SceneManager> & ptr_scene_manager);

		void DestroyResources();

		void SetCallBack(GLFWwindow *window);

		void ProcessInput(GLFWwindow *window);

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
		static Trackball trackball;
		static std::shared_ptr<Renderer> rt_renderer;
		static std::shared_ptr<SceneManager> scene_manager;
		static uint32_t m_width;
		static uint32_t m_height;
		static uint32_t m_display_aov_idx;
		static bool m_mouse_on_GUI;
		static uint32_t m_click_pixel_x, m_click_pixel_y;
		static glm::vec4 m_click_color;
		static uint32_t m_click_prim_idx;
		
		static bool m_show_move_control_menu;
		static bool m_show_geometry_list;
		static bool m_show_material_list;
		static bool m_show_render_menu;
		static bool m_show_camera_menu;
		static bool m_show_light_menu;

		UserInterface() {}

		inline void InitUI(GLFWwindow *window);

		inline void RenderMainMenu();

		inline void RenderOverlay();

		inline void RenderRTMenu();

		inline void RenderCameraMenu();

		inline void RenderGeometryList();

		inline void RenderMaterialList();

		inline void RenderLightList();

		inline void RenderInstanceMenu();

		inline void ExitUI();
	};

	Trackball UserInterface::trackball;
	std::shared_ptr<Renderer> UserInterface::rt_renderer = nullptr;
	std::shared_ptr<SceneManager> UserInterface::scene_manager = nullptr;
	uint32_t UserInterface::m_width = 0;
	uint32_t UserInterface::m_height = 0;
	uint32_t UserInterface::m_display_aov_idx = 0;
	bool UserInterface::m_mouse_on_GUI = false;

	uint32_t UserInterface::m_click_pixel_x = 0;
	uint32_t UserInterface::m_click_pixel_y = 0;
	glm::vec4 UserInterface::m_click_color(0.0f);
	uint32_t UserInterface::m_click_prim_idx = INVALID_UINT_32;

	bool UserInterface::m_show_move_control_menu = false;
	bool UserInterface::m_show_geometry_list = false;
	bool UserInterface::m_show_material_list = false;
	bool UserInterface::m_show_render_menu = false;
	bool UserInterface::m_show_camera_menu = false;
	bool UserInterface::m_show_light_menu = false;

	void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
	{
		UserInterface::m_width = glm::max(width, 1);
		UserInterface::m_height = glm::max(height, 1);
		UserInterface::trackball.camera.SetAspectRatio(float(width) / float(height));
	}

	void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
	{
		double x_pos, y_pos;
		glfwGetCursorPos(window, &x_pos, &y_pos);

		if (button == GLFW_MOUSE_BUTTON_LEFT)
		{
			if (action == GLFW_PRESS)
			{
				if (!UserInterface::m_mouse_on_GUI)
				{
					UserInterface::m_click_pixel_x = (uint32_t)(x_pos - 0.5);
					UserInterface::m_click_pixel_y = UserInterface::m_height - (uint32_t)(y_pos - 0.5) - 1;

					// read pixel from buffer
					UserInterface::rt_renderer->ReadPixel(UserInterface::m_click_pixel_x,
																				  UserInterface::m_click_pixel_y,
																				  UserInterface::m_display_aov_idx,
																			      &UserInterface::m_click_color,
																			      &UserInterface::m_click_prim_idx);
				}

				UserInterface::trackball.mouse_button = button;
				UserInterface::trackball.start_tracking = true;
				UserInterface::trackball.prev_pos_x = x_pos;
				UserInterface::trackball.prev_pos_y = y_pos;
			}
			else if (action == GLFW_RELEASE)
			{
				UserInterface::trackball.mouse_button = -1;
				UserInterface::trackball.start_tracking = false;
			}
		}
	}

	void CursorPosCallback(GLFWwindow * window, double xpos, double ypos)
	{
		if (UserInterface::m_mouse_on_GUI) 
		{
			return;
		}
		if (UserInterface::trackball.start_tracking)
		{
			double x_pos, y_pos;
			glfwGetCursorPos(window, &x_pos, &y_pos);

			glm::vec3 pre_vec = Trackball::CalculTrackBallVec(
				UserInterface::trackball.prev_pos_x, 
				UserInterface::trackball.prev_pos_y,
				UserInterface::m_width,
				UserInterface::m_height);

			glm::vec3 cur_vec = Trackball::CalculTrackBallVec(
				float(x_pos), 
				float(y_pos), 
				UserInterface::m_width,
				UserInterface::m_height);

			if (glm::abs(glm::dot(pre_vec, cur_vec)) > 0.9999f) { return; }
			glm::vec3 axis = glm::cross(pre_vec, cur_vec);

			float angle = glm::acos(glm::min(glm::dot(pre_vec, cur_vec), 1.0f));

			UserInterface::trackball.Rotate(angle, axis);
			UserInterface::trackball.prev_pos_x = float(xpos);
			UserInterface::trackball.prev_pos_y = float(ypos);
		}
	}

	void ScrollCallback(GLFWwindow * window, double xoffset, double yoffset)
	{
		float fov = UserInterface::trackball.camera.GetFov();
		fov -= glm::radians(yoffset);
		UserInterface::trackball.camera.SetFov(glm::clamp(fov, glm::radians(1.0f), glm::radians(90.0f)));
	}

	void UserInterface::Init(GLFWwindow *window, 
		uint32_t width, 
		uint32_t height,
		std::shared_ptr<Renderer> & ptr_renderer,
		std::shared_ptr<SceneManager> & ptr_scene_manager)
	{
		rt_renderer = ptr_renderer;
		scene_manager = ptr_scene_manager;
		m_width = width ;
		m_height = height;
		InitUI(window);
	}

	void UserInterface::DestroyResources()
	{
		ExitUI();
		rt_renderer = nullptr;
		scene_manager = nullptr;
	}

	void UserInterface::SetCallBack(GLFWwindow *window)
	{
		glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
		glfwSetMouseButtonCallback(window, MouseButtonCallback);
		glfwSetCursorPosCallback(window, CursorPosCallback);
		glfwSetScrollCallback(window, ScrollCallback);
	}

	void UserInterface::ProcessInput(GLFWwindow * window)
	{
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, true);
		}
		else if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		{
			trackball.CameraMoveForward();
		}
		else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		{
			trackball.CameraMoveBackward();
		}
		else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		{
			trackball.CameraMoveLeft();
		}
		else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
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
		if (ImGui::InputFloat("Move Speed", &UserInterface::trackball.move_speed))
		{}
		if (ImGui::InputFloat("Rotate Speed", &UserInterface::trackball.rotate_speed))
		{}
		static float camera_ior = 1.0f;
		if (ImGui::InputFloat("External IOR", &camera_ior))
		{
			camera_ior = glm::max(camera_ior, 0.001f);
			UserInterface::GetCamera().SetIOR(camera_ior);
		}
		static int camera_volume_idx = -1;
		if (ImGui::InputInt("Volume Index", &camera_volume_idx))
		{
			camera_volume_idx = glm::clamp(camera_volume_idx, 0, (int)(scene_manager->GetVolumes().size()) - 1);
			UserInterface::GetCamera().SetVolumeIndex(camera_volume_idx);
		}
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
				scene_manager->AddQube(std::string(geometry_name_buf));
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
				scene_manager->AddSphere(std::string(geometry_name_buf), sphere_radius);
			}
		}
		ImGui::Separator();

		ImGui::Text("Total Geometry Count: %d", (uint32_t)geometries.size());
		ImGui::Text("Geometries:");

		static uint32_t display_geometry_info_idx = INVALID_UINT_32;
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
		if (!geometries.empty() && display_geometry_info_idx != INVALID_UINT_32)
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
					m_click_prim_idx = INVALID_UINT_32;
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
		const uint32_t selected_prim_material_idx = prim_ptr != nullptr ? (*prim_ptr).material_idx : INVALID_UINT_32;

		ImGui::Text("Total Material Count: %d", (uint32_t)materials.size());
		ImGui::Text("Materials:");

		static uint32_t highlight_material_idx = INVALID_UINT_32;
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
		if (!materials.empty() && highlight_material_idx != INVALID_UINT_32)
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
				for (uint32_t i = MATERIAL_TYPE::SURFACE_MTL_DEFAULT; i <= MATERIAL_TYPE::LIGHT_MTL; ++i)
				{
					if (ImGui::Selectable(material_type_names[i])) 
					{ 
						highlight_material.material_type = (MATERIAL_TYPE)i;
						scene_manager->UpdateMaterial(highlight_material_idx);
					}
				}
				ImGui::EndCombo();
			}

			if (highlight_material.material_type == SURFACE_MTL_DEFAULT)
			{
				ImGui::Separator();
				if (ImGui::ColorEdit3("Diffuse Albedo", (float*)(&highlight_material.surface_material.diffuse_albedo), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR))
				{
					scene_manager->UpdateMaterial(highlight_material_idx);
				}
				if (ImGui::SliderFloat("Metalness", &highlight_material.surface_material.metalness, 0.0f, 1.0f))
				{
					scene_manager->UpdateMaterial(highlight_material_idx);
				}

				ImGui::Separator();
				if (ImGui::SliderFloat("Specular Weight", &highlight_material.surface_material.specular_weight, 0.0f, 1.0f))
				{
					scene_manager->UpdateMaterial(highlight_material_idx);
				}
				if (ImGui::ColorEdit3("Specular Albedo", (float*)(&highlight_material.surface_material.specular_albedo), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR))
				{
					scene_manager->UpdateMaterial(highlight_material_idx);
				}
				if (ImGui::InputFloat("IOR", &highlight_material.surface_material.ior_n))
				{
					highlight_material.surface_material.ior_n = glm::max(0.0f, highlight_material.surface_material.ior_n);
					scene_manager->UpdateMaterial(highlight_material_idx);
				}
				if (ImGui::SliderFloat("Roughness X", &highlight_material.surface_material.alpha_x, 0.0f, 1.0f))
				{
					scene_manager->UpdateMaterial(highlight_material_idx);
				}
				if (ImGui::SliderFloat("Roughness Y", &highlight_material.surface_material.alpha_y, 0.0f, 1.0f))
				{
					scene_manager->UpdateMaterial(highlight_material_idx);
				}

				ImGui::Separator();
				if (ImGui::SliderFloat("Transmission Weight", &highlight_material.surface_material.transmission_weight, 0.0f, 1.0f))
				{
					scene_manager->UpdateMaterial(highlight_material_idx);
				}
			}
			else if (highlight_material.material_type == LIGHT_MTL)
			{
				ImGui::Separator();
				if (ImGui::ColorEdit3("Light Color", (float*)(&highlight_material.light_material.light_color), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR))
				{
					scene_manager->UpdateMaterial(highlight_material_idx);
				}
				if (ImGui::InputFloat("Intensity", &highlight_material.light_material.intensity))
				{
					highlight_material.light_material.intensity = glm::max(0.0f, highlight_material.light_material.intensity);
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

		static uint32_t highlight_distant_light_idx = INVALID_UINT_32;
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

	inline void UserInterface::RenderInstanceMenu()
	{
		if (!m_show_move_control_menu) { return; }
		ImGui::Begin("Control Menu");
		ImGui::Text("Total Primitive Instance Count: %d", (uint32_t)scene_manager->GetPrimInstances().size());
		ImGui::Text("Highlighted Instance ID: %d", m_click_prim_idx == INVALID_UINT_32 ? (int)(-1) : (uint32_t)m_click_prim_idx);

		PrimitiveInstance *prim_ptr = scene_manager->GetPrimitiveInstance(m_click_prim_idx);
		if (prim_ptr != nullptr)
		{
			PrimitiveInstance &prim_inst = (*prim_ptr);
			if (ImGui::InputFloat("External IOR", &prim_inst.external_ior))
			{
				prim_inst.external_ior = glm::max(0.001f, prim_inst.external_ior);
				scene_manager->ChangePrimVolumeAttribute(m_click_prim_idx, nullptr, nullptr, nullptr);
			}
			if (ImGui::InputInt("External Volume Index", &prim_inst.outer_volume_idx))
			{
				prim_inst.outer_volume_idx = glm::clamp(prim_inst.outer_volume_idx, 0, (int)(scene_manager->GetVolumes().size()) - 1);
				scene_manager->ChangePrimVolumeAttribute(m_click_prim_idx, nullptr, nullptr, nullptr);
			}
			if (ImGui::InputInt("Internal Volume Index:", &prim_inst.inner_volume_idx))
			{
				prim_inst.inner_volume_idx = glm::clamp(prim_inst.inner_volume_idx, 0, (int)(scene_manager->GetVolumes().size()) - 1);
				scene_manager->ChangePrimVolumeAttribute(m_click_prim_idx, nullptr, nullptr, nullptr);
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
			if (m_click_prim_idx != INVALID_UINT_32)
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
				if (m_click_prim_idx != INVALID_UINT_32)
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
			ImGui::Checkbox("Show Render Setting", &UserInterface::m_show_render_menu);
		}

		if (ImGui::CollapsingHeader("Camera Setting"))
		{
			ImGui::Checkbox("Show Camera Setting", &UserInterface::m_show_camera_menu);
		}

		if (ImGui::CollapsingHeader("Geometries"))
		{
			ImGui::Checkbox("Show Geometry List", &UserInterface::m_show_geometry_list);
		}

		if (ImGui::CollapsingHeader("Materials"))
		{
			ImGui::Checkbox("Show Material List", &UserInterface::m_show_material_list);
		}

		if (ImGui::CollapsingHeader("Instances"))
		{
			ImGui::Checkbox("Show Control Board", &UserInterface::m_show_move_control_menu);
		}

		if (ImGui::CollapsingHeader("Light"))
		{
			ImGui::Checkbox("Show Light List", &UserInterface::m_show_light_menu);
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

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
}