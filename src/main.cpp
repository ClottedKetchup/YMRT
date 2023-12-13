#include <iostream>

#include "MainSystem.h"

int main(int argc, char *argv[])
{
	try
	{
		std::string path(argv[0]);
		std::string prefix = path.substr(0, path.rfind('\\') + 1);
		std::string vert_path = prefix + std::string("shaders\\vert.glsl");
		std::string frag_path = prefix + std::string("shaders\\frag.glsl");
		std::string model_path = "";

		std::shared_ptr<YumeRT::MainSystem> ptr = 
			YumeRT::MainSystem::GetInstance();
		ptr->Init(vert_path,
			frag_path,
			model_path,
			1280,
			980);
		ptr->Run();
	}
	catch (const std::exception& err)
	{
		std::cout << err.what() << std::endl;
	}
}