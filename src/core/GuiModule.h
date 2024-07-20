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
		int m_width, m_height;

		DuskTrackball trackball;

		GLFWwindow *m_window;
		std::shared_ptr<SceneModule> m_module_scene;
		std::shared_ptr<RenderModule> m_module_render;
		
		void UpdateScene();
		void UpdateCamera(int index);

		void InitGUI();
		void RenderImages();
		void ExitGUI();
	};
};