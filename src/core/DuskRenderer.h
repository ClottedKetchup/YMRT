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


namespace YumeRT {
	class DuskRenderer {
	public:
		static inline std::shared_ptr<DuskRenderer> GetDuskRenderer() {
			static std::shared_ptr<DuskRenderer> ptr(new DuskRenderer());
			return ptr;
		}

		~DuskRenderer();

		void Run();

		inline std::shared_ptr<SceneModule> GetSceneModule() {
			return module_scene;
		}

	private:

		GLFWwindow *window;
		std::shared_ptr<SceneModule> module_scene;
		std::shared_ptr<RenderModule> module_render;
		std::shared_ptr<GuiModule> module_gui;

		DuskRenderer();
	};
}