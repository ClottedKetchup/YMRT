#include <iostream>

#include "src/core/DuskRenderer.h"

int main(int argc, char *argv[])
{
	try {
		auto dusk_renderer = YumeRT::DuskRenderer::GetDuskRenderer();
		dusk_renderer->Run();
	}
	catch (const std::exception& err) {
		std::cout << err.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}