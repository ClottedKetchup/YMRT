#pragma once

#include "src/core/RenderModule.h"
#include "src/core/GuiModule.h"
#include "src/core/SceneModule.h"
#include "src/Helper.h"

#include <memory>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cuda_runtime.h>
#include <cuda_gl_interop.h>


namespace YMRT {
	class YMRayTracer {
	public:
		static inline std::shared_ptr<YMRayTracer> GetYMRayTracer() {
			static std::shared_ptr<YMRayTracer> ptr(new YMRayTracer());
			return ptr;
		}

		~YMRayTracer();

		void Run();

		inline std::shared_ptr<SceneModule> GetSceneModule() {
			return module_scene;
		}

	private:

		GLFWwindow *window;
		std::shared_ptr<SceneModule> module_scene;
		std::shared_ptr<RenderModule> module_render;
		std::shared_ptr<GuiModule> module_gui;

		YMRayTracer();
	};
}