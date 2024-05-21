#include <ppl.h>

#include "FileIO.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace YumeRT 
{
	ImageTexture LoadImageTexture(const std::string &file_name, ImageTextureManager &image_texture_manager)
	{
		// load tex file
		ImageTexture image_texture;
		image_texture.tile_offset = -1;
		image_texture.file_offset = -1;

		image_texture.warp_mode = WARP_MODE_REPEAT;

		image_texture.u_scale = 1.0f;
		image_texture.v_scale = 1.0f;
		image_texture.u_offset = 0.0f;
		image_texture.v_offset = 0.0f;

		std::vector<float> image_buffer;
		
		// read file
		{
			unsigned char *image_handle = nullptr;
			int image_width = 0, image_height = 0, image_channel = 0;
			image_handle = stbi_load(file_name.c_str(), &image_width, &image_height, &image_channel, 0);
			if (image_width == 0 || image_height == 0 || image_channel == 0 || image_handle == nullptr) 
			{
				printf("Image file %s load fail.\n", file_name.c_str());
				if (image_handle != nullptr) { stbi_image_free(image_handle); }
				return image_texture;
			}

			image_buffer.resize((size_t)image_width * (size_t)image_height * (size_t)image_channel, 0.0f);
			constexpr float inv_255 = 1.0f / 255.0f;
			for (size_t i = 0; i < image_buffer.size(); ++i) {
				image_buffer[i] = SRGBToLinear(float((int)(image_handle[i])) * inv_255);
			}
			printf("Image file %s load success.\n", file_name.c_str());
			stbi_image_free(image_handle);

			image_texture.width = image_width;
			image_texture.height = image_height;
			image_texture.channel_count = image_channel;
		}

		image_texture.file_offset = (int)image_texture_manager.image_texture_files.size();
		image_texture_manager.image_texture_files.push_back(ImageFile(file_name));

		image_texture.mipmap_count = 1;
		image_texture.mipmap_tile_offsets[0] = 0;

		image_texture.tile_offset = (int)image_texture_manager.image_texture_tiles.size();
		const int tile_x_count = (image_texture.width + TEX_TILE_RES_X - 1) / TEX_TILE_RES_X;
		const int tile_y_count = (image_texture.height + TEX_TILE_RES_Y - 1) / TEX_TILE_RES_Y;
		for (int i = 0; i < tile_y_count * tile_x_count; ++i) { image_texture_manager.image_texture_tiles.push_back(ImageTile()); }
		
		concurrency::parallel_for(0, tile_y_count * tile_x_count, [&](int tile_idx) 
		{
			ImageTile &image_tile = image_texture_manager.image_texture_tiles[image_texture.tile_offset + tile_idx];
			image_tile.data_size = sizeof(float) * image_texture.channel_count * TEX_TILE_RES_X * TEX_TILE_RES_Y;
			image_tile.host_data = (void*)malloc(image_tile.data_size);
			memset(image_tile.host_data, 0, image_tile.data_size);

			const int tile_j = tile_idx / tile_x_count;
			const int tile_i = tile_idx % tile_x_count;
			const int tile_x_base = tile_i * TEX_TILE_RES_X;
			const int tile_y_base = tile_j * TEX_TILE_RES_Y;
			for (int j = 0; j < TEX_TILE_RES_Y; ++j)
			{
				for (int i = 0; i < TEX_TILE_RES_X; ++i)
				{
					const int pixel_x = tile_x_base + i;
					const int pixel_y = tile_y_base + j;
					if (!(pixel_x < image_texture.width && pixel_y < image_texture.height)) { continue; }
					const size_t pixel_offset = (pixel_y * image_texture.width + pixel_x) * image_texture.channel_count;
					const size_t tile_pixel = (j * TEX_TILE_RES_X + i) * image_texture.channel_count;
					for (int channel_idx = 0; channel_idx < image_texture.channel_count; ++channel_idx){
						((float*)(image_tile.host_data))[tile_pixel + channel_idx] = image_buffer[pixel_offset + channel_idx];
					}
				}
			}
		});
		return image_texture;
	}
};