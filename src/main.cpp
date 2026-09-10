#include <iostream>

#include "src/core/YMRayTracer.h"

int main(int argc, char *argv[])
{
	try {
		auto ym_ray_tracer = YMRT::YMRayTracer::GetYMRayTracer();
		ym_ray_tracer->Run();
	}
	catch (const std::exception& err) {
		std::cout << err.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}