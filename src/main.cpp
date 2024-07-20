#include <iostream>

#include "src/core/DuskRenderer.h"
	
int main(int argc, char *argv[])
{
	try {
		auto dusk_renderer = YumeRT::DuskRenderer::GetDuskRenderer();

		// Do some default initialization...
		auto &camera = dusk_renderer->GetSceneModule()->GetCamera(EDITOR_CAMERA_INDEX);
		camera.SetFov(glm::radians(45.0f));
		camera.SetPosition(glm::vec3(0.0f, 0.0f, 10.0f));
		camera.SetDir(glm::vec3(0.0f, 0.0f, 10.0f) - glm::vec3(0.0f, 0.0f, 4.0f));

		dusk_renderer->Run();
	}
	catch (const std::exception& err) {
		std::cout << err.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}