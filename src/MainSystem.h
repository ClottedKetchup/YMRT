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
		static inline std::shared_ptr<MainSystem> GetInstance(const std::string &exec_path, uint32_t screen_width = 1024, uint32_t screen_height = 1024)
		{
			static std::shared_ptr<MainSystem> ptr(new MainSystem(exec_path, screen_width, screen_height));
			return ptr;
		}

		inline ~MainSystem()
		{
			user_interface = nullptr, rt_renderer = nullptr, scene_manager = nullptr;

			printf("Cuda and GL context exit...\n");
			glfwDestroyWindow(window);
			glfwTerminate();
		}

		inline std::shared_ptr<SceneManager> GetSceneManager() { return scene_manager; }

		inline void Run();

	private:
		const std::string executable_path;

		GLFWwindow *window = nullptr;
		GLShader shader;
		GLVertexArray empty_VAO;
		
		std::shared_ptr<SceneManager> scene_manager = nullptr;
		std::shared_ptr<UserInterface> user_interface = nullptr;
		std::shared_ptr<Renderer> rt_renderer = nullptr;

		inline MainSystem(const std::string &exec_path,
			uint32_t screen_width,
			uint32_t screen_height);
	};

	inline MainSystem::MainSystem(const std::string &exec_path,
		uint32_t screen_width,
		uint32_t screen_height): executable_path(exec_path)
	{
		// init cuda GL
		cudaGLSetGLDevice(0);

		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		window = glfwCreateWindow(screen_width, screen_height, "Yume", nullptr, nullptr);
		if (window == nullptr) {
			glfwTerminate();
			throw std::runtime_error("Fail to create window");
		}

		glfwMakeContextCurrent(window);

		// load OpenGL
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			glfwTerminate();
			throw std::runtime_error("Fail to initialize GLAD");
		}

		const std::string exec_prefix = executable_path.substr(0, executable_path.rfind('\\') + 1);
		const std::string vert_path = exec_prefix + std::string("shaders\\vert.glsl");
		const std::string frag_path = exec_prefix + std::string("shaders\\frag.glsl");

		// init gl shader
		shader.InitShader(vert_path, frag_path);
		// init an empty vertex array
		empty_VAO.InitVAO();

		scene_manager = std::shared_ptr<SceneManager>(new SceneManager());
		rt_renderer = std::shared_ptr<Renderer>(new Renderer());
		user_interface = std::shared_ptr<UserInterface>(new UserInterface(window, screen_width, screen_height, rt_renderer, scene_manager));
	}

	inline void MainSystem::Run()
	{
		printf("Begin main loop...\n");
		// first upload the scene resource
		scene_manager->UploadSceneSetting();
		// past scene setting to ui interface
		user_interface->SetCamera(scene_manager->GetCamera());

		while (!glfwWindowShouldClose(window))
		{
			const float current_time = glfwGetTime();
			const uint32_t current_width = user_interface->GetWidth();
			const uint32_t current_height = user_interface->GetHeight();
			if (rt_renderer->GetWidth() != current_width || rt_renderer->GetHeight() != current_height)
			{
				rt_renderer->SetRenderSettingChange(true);
				rt_renderer->SetWidth(current_width);
				rt_renderer->SetHeight(current_height);
				rt_renderer->DestroyResources();
				rt_renderer->PrepareResources();
			}

			user_interface->ProcessInput(window);

			glViewport(0, 0, current_width, current_height);
			glClearColor(0.f, 0.f, 0.f, 1.f);
			glClear(GL_COLOR_BUFFER_BIT);

			uint32_t display_aov_idx = user_interface->GetDisplayAovIdx();
			uint32_t highlight_prim_idx = user_interface->GetSelectedPrimIdx();

			bool last_frame_scene_change = scene_manager->IsSceneChange();
			scene_manager->UpdateScene(user_interface->GetCamera(), &highlight_prim_idx);
			user_interface->SetSelectedPrimIdx(highlight_prim_idx);
			rt_renderer->Render(scene_manager->GetScene(), scene_manager->GetImageTextureManager(), last_frame_scene_change);

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

/*

ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		// ImGui::DockSpaceOverViewport();

		ImGuiIO &io = ImGui::GetIO();



		{
			static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

			// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window_ not dockable into,
			// because it would be confusing to have two docking targets within each others.
			ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

			// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
			// and handle the pass-thru hole, so we ask Begin() to not render a background.
			if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
				window_flags |= ImGuiWindowFlags_NoBackground;

			// Important: note that we proceed even if Begin() returns false (aka window_ is collapsed).
			// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
			// all active windows docked into it will lose their parent and become undocked.
			// We cannot preserve the docking relationship between an active window_ and an inactive docking, otherwise
			// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("DockSpace", nullptr, window_flags);

			ImGui::PopStyleVar();
			ImGui::PopStyleVar(2);

			// Submit the DockSpace
			if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
			{
				ImGuiID dockspace_id = ImGui::GetID("VulkanAppDockspace");
				ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
			}

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

			{
				int tex_width, tex_height;
				auto tex_handle = rt_renderer->GetAccumulate(&tex_width, &tex_height);

				ImGui::Begin("Viewport");
				ImGui::Text("Viewport width: %d, height %d\n", static_cast<int>(ImGui::GetContentRegionAvail().x), static_cast<int>(ImGui::GetContentRegionAvail().y));
				ImGui::Image((ImTextureID)tex_handle, ImVec2(tex_width, tex_height), ImVec2(0, 1), ImVec2(1, 0));

				if (ImGui::IsWindowFocused())
				{
					if (ImGui::IsKeyDown(ImGuiKey_W))
					{
						trackball.CameraMoveForward();
					}
					else if (ImGui::IsKeyDown(ImGuiKey_S))
					{
						trackball.CameraMoveBackward();
					}
					else if (ImGui::IsKeyDown(ImGuiKey_A))
					{
						trackball.CameraMoveLeft();
					}
					else if (ImGui::IsKeyDown(ImGuiKey_D))
					{
						trackball.CameraMoveRight();
					}
					else {

					}

					if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
					{
						printf("mouse drag\n");
					}
				}

				ImGui::End();
			}

			ImGui::ShowDemoWindow();

			ImGui::End();
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// multi-viewport
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			// restore opengl context.
			GLFWwindow *current_opengl_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(current_opengl_context);
		}
	}
*/

