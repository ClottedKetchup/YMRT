#pragma once

#include <driver_types.h>
#include <string>
#include <vector>

#include "MathCommon.h"

namespace YumeRT
{
	struct ImageFile 
	{
		std::string file_name;
		__host__ inline ImageFile() {}
		__host__ inline ImageFile(const std::string &name) : file_name(name) {}
	};

	struct ImageTile
	{
		void *device_data;
		void *host_data;

		__device__ __host__ inline ImageTile() : device_data(nullptr), host_data(nullptr) {}
	};

	// should be single instance, the same as Scene
	struct ImageTileCache
	{
		ImageTile *image_tiles;
		__device__ __host__ inline ImageTileCache() : image_tiles(nullptr) {}
	};

	struct ImageTextureManager 
	{
		ImageTileCache image_tile_cache;
		std::vector<ImageFile> image_texture_files;
		std::vector<ImageTile> image_texture_tiles;
		__host__ inline ImageTextureManager() : image_tile_cache(), image_texture_files(0), image_texture_tiles(0) {}
	};
};