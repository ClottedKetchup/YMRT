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
		static std::shared_ptr<MainSystem> GetInstance()
		{
			static std::shared_ptr<MainSystem> ptr(new MainSystem());
			return ptr;
		}

		~MainSystem()
		{
			user_interface->DestroyResources();
			rt_renderer->DestroyResources();
			scene_manager->DestroyResources();
			this->DestroyResources();
		}

		void Init(const std::string &vs,
					  const std::string &fs,
					  const std::string &scene_file,
					  uint32_t screen_width,
					  uint32_t screen_height);

		void DestroyResources()
		{
			glfwDestroyWindow(window);
			glfwTerminate();
		}

		void Run();

	private:
		GLFWwindow *window = nullptr;
		GLShader shader;
		GLVertexArray empty_VAO;
		
		std::shared_ptr<SceneManager> scene_manager = nullptr;
		std::shared_ptr<UserInterface> user_interface = nullptr;
		std::shared_ptr<Renderer> rt_renderer = nullptr;

		MainSystem() {}
	};

	void MainSystem::Init(const std::string &vs,
									    const std::string &fs,
										const std::string &scene_file,
										uint32_t screen_width,
										uint32_t screen_height)
	{
		// init cuda GL
		cudaGLSetGLDevice(0);

		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		window = glfwCreateWindow(screen_width, screen_height, "Yume", nullptr, nullptr);
		if (window == nullptr)
		{
			glfwTerminate();
			throw std::runtime_error("Fail to create window");
		}

		glfwMakeContextCurrent(window);

		// load OpenGL
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		{
			glfwTerminate();
			throw std::runtime_error("Fail to initialize GLAD");
		}

		// init gl shader
		shader.InitShader(vs, fs);

		// init an empty vertex array
		empty_VAO.InitVAO();

		scene_manager = SceneManager::GetInstance();
		scene_manager->InitScene();

		rt_renderer = Renderer::GetInstance();
		rt_renderer->Init();

		user_interface = UserInterface::GetInstance();
		user_interface->Init(window, screen_width, screen_height, Renderer::GetInstance(), SceneManager::GetInstance());
		user_interface->SetCallBack(window);
		user_interface->SetCamera(glm::vec3(0.0f, 0.0f, 6.0f),
													 glm::vec3(0.0f, 0.0f, 0.0f),
													 glm::radians(45.0f),
													 float(screen_width) / float(screen_height));
	}

	void MainSystem::Run() 
	{
		while (!glfwWindowShouldClose(window))
		{
			float current_time = glfwGetTime();
			uint32_t current_w = user_interface->GetWidth();
			uint32_t current_h = user_interface->GetHeight();
			if (rt_renderer->GetWidth() != current_w || rt_renderer->GetHeight() != current_h)
			{
				rt_renderer->SetWidth(current_w);
				rt_renderer->SetHeight(current_h);
				rt_renderer->DestroyResources();
				rt_renderer->PrepareResources();
			}

			user_interface->ProcessInput(window);

			glViewport(0, 0, current_w, current_h);
			glClearColor(0.f, 0.f, 0.f, 1.f);
			glClear(GL_COLOR_BUFFER_BIT);

			uint32_t display_aov_idx = user_interface->GetDisplayAovIdx();
			uint32_t highlight_prim_idx = user_interface->GetSelectedPrimIdx();

			bool last_frame_scene_change = scene_manager->IsSceneChange();
			scene_manager->UpdateScene(user_interface->GetCamera(), &highlight_prim_idx);
			user_interface->SetSelectedPrimIdx(highlight_prim_idx);
			rt_renderer->Render(scene_manager->GetScene(), last_frame_scene_change);

			shader.Use();
			empty_VAO.Bind();
			glBindTextureUnit(0, rt_renderer->SetDisplayAov(display_aov_idx, highlight_prim_idx));
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			empty_VAO.UnBind();

			user_interface->RenderUI();

			glfwSwapBuffers(window);
			glfwPollEvents();
		}
	}
}
