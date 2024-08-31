#pragma once

#include <driver_types.h>
#include <string>
#include <vector>

#include "Helper.h"
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
		size_t data_size;

		__device__ __host__ inline ImageTile() : device_data(nullptr), host_data(nullptr), data_size(0) {}
		__device__ __host__ inline void* GetTileData() const
		{
#ifdef __CUDA_ARCH__
			return device_data;
#else
			return host_data;
#endif
		}
	};

	// should be single instance, the same as Scene
	struct ImageTileCache
	{
		ImageTile *image_tiles_device;
		ImageTile *image_tiles_host;
		__device__ __host__ inline ImageTileCache() : image_tiles_device(nullptr), image_tiles_host(nullptr) {}
		__device__ __host__ inline ImageTile* GetImageTiles() const
		{
#ifdef __CUDA_ARCH__
			return image_tiles_device;
#else
			return image_tiles_host;
#endif 
		}
	};

	struct ImageTextureManager 
	{
		ImageTileCache image_tile_cache;
		std::vector<ImageFile> image_texture_files;
		std::vector<ImageTile> image_texture_tiles;

		__host__ inline ImageTextureManager() : image_tile_cache(), image_texture_files(0), image_texture_tiles(0) {}
		__host__ inline void UploadToDevice() 
		{
			for (ImageTile &tile : image_texture_tiles) 
			{
				assert(tile.host_data != nullptr);
				UPLOAD_TO_GPU(tile.device_data, tile.host_data, tile.data_size);
			}
			// be careful when you upload new image, you have to reset both the host pointer and device pointer!
			image_tile_cache.image_tiles_host = image_texture_tiles.data();
			UPLOAD_TO_GPU(image_tile_cache.image_tiles_device, image_texture_tiles.data(), sizeof(ImageTile) * image_texture_tiles.size());
		}
		__host__ inline void RefreshHostData()
		{
			image_tile_cache.image_tiles_host = image_texture_tiles.data();
		}
		__host__ inline void Release() 
		{
			image_tile_cache.image_tiles_host = nullptr;
			FREE_GPU_RESOURCE(image_tile_cache.image_tiles_device);
			for (ImageTile& tile : image_texture_tiles) 
			{
				FREE_GPU_RESOURCE(tile.device_data);
				free(tile.host_data);
			}
		}
	};
};