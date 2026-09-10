#include "src/core/YMRayTracer.h"

namespace YMRT {

	static constexpr int default_width = 1600;
	static constexpr int default_height = 900;

	YMRayTracer::YMRayTracer() :window(nullptr), module_scene(nullptr), module_render(nullptr), module_gui(nullptr)
	{
		CUDA_CHECK(cudaSetDevice(0));
		CUDA_CHECK(cudaFree(0));
		CUDA_CHECK(cudaDeviceSetLimit(cudaLimitStackSize, 2048u));

		// TODO: follow cuda's official sample to select device, and record properties of devices
		cudaGLSetGLDevice(0);

		// OpenGL basic init.
		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		window = glfwCreateWindow(default_width, default_height, "YMRT", nullptr, nullptr);
		if (window == nullptr) {
			glfwTerminate();
			throw std::runtime_error("Fatal error: Fail to create glfw window.\n");
		}

		glfwMakeContextCurrent(window);

		// Load OpenGL functions.
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			glfwTerminate();
			throw std::runtime_error("Fatal error: Fail to initialize GLAD\n");
		}

		module_scene = std::shared_ptr<SceneModule>(new SceneModule());
		module_render = std::shared_ptr<RenderModule>(new RenderModule());
		module_gui = std::shared_ptr<GuiModule>(new GuiModule(window, default_width, default_height, module_scene, module_render));
	}

	YMRayTracer::~YMRayTracer()
	{
		// Release all resources before destory context.
		// Note: this destroy sequence must be obeyed!
		module_gui = nullptr;
		module_render = nullptr;
		module_scene = nullptr;
		
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void YMRayTracer::Run()
	{
		while (!glfwWindowShouldClose(window)) 
		{
			glViewport(0, 0, module_gui->GetWindowWidth(), module_gui->GetWindowHeight());
			glClearColor(0.f, 0.f, 0.f, 1.f);
			glClear(GL_COLOR_BUFFER_BIT);

			module_gui->Draw();

			glfwSwapBuffers(window);
			glfwPollEvents();
		}
	}
};