#pragma once

#include "src/core/RenderModule.h"
#include "src/core/SceneModule.h"
#include "src/Camera.h"

#include <memory>
#include <string>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"



namespace YumeRT {
	struct DuskTrackball {
		int mouse_button = -1;

		float prev_pos_x = -1.0f;
		float prev_pos_y = -1.0f;
		float move_speed = 0.5f;
		float rotate_speed = 0.5f;

		bool start_tracking = false;
		Camera *cam;

		inline DuskTrackball(Camera& camera) : cam(&camera) {}
		static inline glm::vec3 CalculTrackBallVec(float xpos, float ypos, float width, float height)
		{
			glm::vec2 ndc = {
			2.f * (xpos / width) - 1.f,
			2.f * (ypos / height) - 1.f
			};
			ndc.y = -ndc.y;
			float r2 = ndc.x * ndc.x + ndc.y * ndc.y;
			float z = 0.0f;
			if (r2 < 1.0f) {
				z = glm::sqrt(1.0f - r2);
			}
			else {
				ndc /= glm::sqrt(r2);
			}
			return glm::normalize(glm::vec3(ndc.x, ndc.y, z));
		}
		inline void CameraMoveForward()
		{
			cam->MoveForward(move_speed);
		}
		inline void CameraMoveBackward()
		{
			cam->MoveBackward(move_speed);
		}
		inline void CameraMoveLeft()
		{
			cam->MoveLeft(move_speed);
		}
		inline void CameraMoveRight()
		{
			cam->MoveRight(move_speed);
		}
		inline void Rotate(float angle /* radians */, const glm::vec3& axis /* normalized */)
		{
			glm::mat4 rm = Matrix_R(axis, angle * rotate_speed);
			glm::vec4 local_dir_z = rm * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
			glm::vec3 world_dir_z = glm::vec3(cam->CameraToWorld(local_dir_z));

			if (glm::abs(glm::dot(world_dir_z, WORLD_UP)) < 0.999f) {
				cam->SetDir(world_dir_z);
			}
		}
	};

	class GuiModule {
	public:
		GuiModule(GLFWwindow* window,
			int width, int height,
			std::shared_ptr<SceneModule> &module_scene,
			std::shared_ptr<RenderModule> &module_render);
		~GuiModule();
		GuiModule(const GuiModule&) = delete;
		GuiModule(const GuiModule&&) = delete;
		GuiModule& operator=(const GuiModule&) = delete;

		void Draw();
		
		inline int GetWindowWidth() const {
			return m_width;
		}
		inline void SetWindowWidth(int width) {
			m_width = width;
		}
		inline int GetWindowHeight() const {
			return m_height;
		}
		inline void SetWindowHeight(int height) {
			m_height = height;
		}

	private:
		bool draw_selected_effect;
		bool show_primitive_list;
		int m_width, m_height;
		float primitive_scale_upper;
		float primitive_scale_lower;
		float primitive_move_speed;
		uint32_t clicked_pixel_primitive_index;

		DuskTrackball trackball;

		GLFWwindow *m_window;
		std::shared_ptr<SceneModule> m_module_scene;
		std::shared_ptr<RenderModule> m_module_render;
		
		bool UpdateScene();
		bool UpdateCamera(int index);

		void InitGUI();
		void RenderImages();
		void RenderMainMenu();
		void RenderObjectList();
		void ExitGUI();

		enum TASK_QUEUE_CLEAR_FLAG
		{
			NONE_QUEUE = 0b0000,
			EDITOR_VIEW_QUEUE = 0b0001,
			PATH_TRACING_QUEUE = 0b0010,
			BOTH_QUEUE = 0b0011,
		};

		template<typename Function>
		void UpdateDeviceData(Function update_function)
		{
			auto& m_renderer = (*m_module_render);
			auto& m_scene = (*m_module_scene);
			auto& scene_resource = m_scene.scene_resource;

			std::scoped_lock lk(scene_resource.scene_mutex, m_renderer.editor_view_task_queue_mutex, m_renderer.editor_view_result_queue_mutex, m_renderer.path_tracing_task_queue_mutex, m_renderer.path_tracing_result_queue_mutex);

			++scene_resource.scene_change_time;

			m_renderer.editor_view_task_queue.clear(); 
			m_renderer.editor_view_result_queue_albedo.clear();
			m_renderer.editor_view_result_queue_primitive_index.clear();
			m_renderer.editor_view_result_queue_extra.clear();

			m_renderer.path_tracing_task_queue.clear();
			m_renderer.path_tracing_result_queue_beauty.clear();
			m_renderer.path_tracing_result_queue_extra.clear();

			update_function();
		}
	};
};