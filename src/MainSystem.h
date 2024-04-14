#pragma once

#include <memory>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cuda_runtime.h>
#include <cuda_gl_interop.h>

#include "GLShader.h"
#include "Renderer.h"
#include "UserInterface.h"
#include "SceneManager.h"
#include "Helper.h"

namespace YumeRT 
{
	class MainSystem
	{
	public:
		static inline std::shared_ptr<MainSystem> GetInstance()
		{
			static std::shared_ptr<MainSystem> ptr(new MainSystem());
			return ptr;
		}

		inline ~MainSystem()
		{
			user_interface->DestroyResources();
			rt_renderer->DestroyResources();
			scene_manager->DestroyResources();
			this->DestroyResources();
		}

		inline std::shared_ptr<SceneManager> GetSceneManager() { return scene_manager; }

		void Init(const std::string &vs,
					  const std::string &fs,
					  const std::string &scene_file,
					  uint32_t screen_width,
					  uint32_t screen_height);

		void Run();

	private:
		GLFWwindow *window = nullptr;
		GLShader shader;
		GLVertexArray empty_VAO;
		
		std::shared_ptr<SceneManager> scene_manager = nullptr;
		std::shared_ptr<UserInterface> user_interface = nullptr;
		std::shared_ptr<Renderer> rt_renderer = nullptr;

		inline MainSystem() {}

		inline void DestroyResources()
		{
			glfwDestroyWindow(window);
			glfwTerminate();
		}
	};
}
