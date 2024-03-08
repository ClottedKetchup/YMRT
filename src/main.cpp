#include <iostream>
#include <vector>
#include <random>

#include "MainSystem.h"
#include "MathCommon.h"

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
			1024,
			800);
		ptr->Run();
	}
	catch (const std::exception& err)
	{
		std::cout << err.what() << std::endl;
	}

	/*int table_size = 256;
	std::vector<int> permute_x(table_size);
	std::vector<int> permute_y(table_size);
	std::vector<int> permute_z(table_size);

	for (int i = 0; i < table_size; ++i) 
	{
		permute_x[i] = i;
		permute_y[i] = i;
		permute_z[i] = i;
	}

	std::mt19937 rng(19980726u);
	std::shuffle(permute_x.begin(), permute_x.end(), rng);
	std::shuffle(permute_y.begin(), permute_y.end(), rng);
	std::shuffle(permute_z.begin(), permute_z.end(), rng);
	
	auto print_vec = [](const std::vector<int>& vec) 
	{
		int i = 0;
		for (const auto &num : vec) 
		{
			std::cout << num << ", ";
			++i;
			if (i == 16) 
			{
				i = 0;
				std::cout << "\n";
			}
		}
		std::cout << std::endl;
	};
	print_vec(permute_x);*/
	return 0;
}