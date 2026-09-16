#include <ppl.h>

#include <filesystem>

#include "FileIO.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"



namespace YMRT 
{
	static bool LoadPng8(const std::string& _own_file_name, const int image_texture_color_space, int& image_width, int& image_height, int& image_channel, std::vector<float>& image_buffer)
	{
		// TODO: put the filp switch to texture attribute.
		unsigned char* image_handle = nullptr;

		stbi_set_flip_vertically_on_load(true);
		image_handle = stbi_load(_own_file_name.c_str(), &image_width, &image_height, &image_channel, 0);
		assert(sizeof(stbi_uc) == sizeof(unsigned char));

		if (image_width == 0 || image_height == 0 || image_channel == 0 || image_handle == nullptr) {
			printf("Image file %s load fail.\n", _own_file_name.c_str());
			if (image_handle != nullptr) {
				stbi_image_free(image_handle);
			}
			return false;
		}

		image_buffer.resize((size_t)image_width * (size_t)image_height * (size_t)image_channel, 0.0f);

		constexpr float inv_255 = 1.0f / 255.0f;
		for (size_t i = 0; i < image_buffer.size(); ++i) {
			float value = float((int)(image_handle[i])) * inv_255;
			image_buffer[i] = (image_texture_color_space == IMAGE_SRGB) ? SRGBToLinear(value) : value;
		}
		printf("Image file %s load success.\n", _own_file_name.c_str());
		stbi_image_free(image_handle);

		return true;
	}

	static bool LoadPng16(const std::string& _own_file_name, const int image_texture_color_space, int& image_width, int& image_height, int& image_channel, std::vector<float>& image_buffer)
	{
		// TODO: put the filp switch to texture attribute.
		unsigned short* image_handle = nullptr;

		stbi_set_flip_vertically_on_load(true);
		image_handle = stbi_load_16(_own_file_name.c_str(), &image_width, &image_height, &image_channel, 0);
		assert(sizeof(stbi_us) == sizeof(unsigned short));

		if (image_width == 0 || image_height == 0 || image_channel == 0 || image_handle == nullptr) {
			printf("Image file %s load fail.\n", _own_file_name.c_str());
			if (image_handle != nullptr) {
				stbi_image_free(image_handle);
			}
			return false;
		}

		image_buffer.resize((size_t)image_width * (size_t)image_height * (size_t)image_channel, 0.0f);

		constexpr float inv_65535 = 1.0f / 65535.0f;
		for (size_t i = 0; i < image_buffer.size(); ++i) {
			float value = float((int)(image_handle[i])) * inv_65535;
			image_buffer[i] = (image_texture_color_space == IMAGE_SRGB) ? SRGBToLinear(value) : value;
		}
		printf("Image file %s load success.\n", _own_file_name.c_str());
		stbi_image_free(image_handle);

		return true;
	}

	ImageTexture LoadImageTexture(const std::string &file_name, 
		std::vector<ImageFile> &image_texture_files, 
		std::vector<ImageTile> &image_texture_tiles,
		const int image_texture_color_space)
	{
		std::string _own_file_name = file_name;
		for (auto& c : _own_file_name) {
			if (c == '\\') {
				c = '/';
			}
		}

		// load tex file
		ImageTexture image_texture;
		image_texture.tile_offset = -1;
		image_texture.file_offset = -1;

		image_texture.warp_mode = WARP_MODE_REPEAT;
		image_texture.color_space = image_texture_color_space;

		image_texture.scale_u = 1.0f;
		image_texture.scale_v = 1.0f;
		image_texture.offset_u = 0.0f;
		image_texture.offset_v = 0.0f;

		std::vector<float> image_buffer;
		
		// read file
		{
			int image_width = 0, image_height = 0, image_channel = 0;
			if (stbi_is_16_bit(_own_file_name.c_str())) {
				if (!LoadPng16(_own_file_name, image_texture.color_space, image_width, image_height, image_channel, image_buffer)) {
					return image_texture;
				}
			}
			else {
				if (!LoadPng8(_own_file_name, image_texture.color_space, image_width, image_height, image_channel, image_buffer)) {
					return image_texture;
				}
			}

			image_texture.width = image_width;
			image_texture.height = image_height;
			image_texture.channel_count = image_channel;
		}

		image_texture.file_offset = (int)image_texture_files.size();
		image_texture_files.push_back(ImageFile(_own_file_name));

		image_texture.mipmap_count = 1;
		image_texture.mipmap_tile_offsets[0] = 0;

		image_texture.tile_offset = (int)image_texture_tiles.size();
		const int tile_x_count = (image_texture.width + TEX_TILE_RES_X - 1) / TEX_TILE_RES_X;
		const int tile_y_count = (image_texture.height + TEX_TILE_RES_Y - 1) / TEX_TILE_RES_Y;
		for (int i = 0; i < tile_y_count * tile_x_count; ++i) { 
			image_texture_tiles.push_back(ImageTile()); 
		}
		
		concurrency::parallel_for(0, tile_y_count * tile_x_count, [&](int tile_idx) 
		{
			ImageTile &image_tile = image_texture_tiles[image_texture.tile_offset + tile_idx];
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
					if (!(pixel_x < image_texture.width && pixel_y < image_texture.height)) { 
						continue; 
					}
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

	bool SavePngImage(const std::string &file_path, const std::vector<glm::vec4> &host_image, const int image_width, const int image_height)
	{
		if (file_path.empty() || image_width <= 0 || image_height <= 0) {
			return false;
		}
		if (host_image.size() != (size_t)image_width * image_height) {
			return false;
		}

		std::filesystem::path png_file_path(file_path);
		png_file_path.replace_extension(".png");
		if (png_file_path.has_parent_path()) {
			std::filesystem::create_directories(png_file_path.parent_path());
		}

		stbi_flip_vertically_on_write(true);

		std::vector<unsigned char> png_pixel_data(host_image.size() * 4);
		for (size_t pixel_index = 0; pixel_index < host_image.size(); ++pixel_index) {
			const glm::vec4 &pixel_color = host_image.at(pixel_index);
			png_pixel_data.at(pixel_index * 4 + 0) = (unsigned char)(glm::clamp(pixel_color.x, 0.0f, 1.0f) * 255.0f + 0.5f);
			png_pixel_data.at(pixel_index * 4 + 1) = (unsigned char)(glm::clamp(pixel_color.y, 0.0f, 1.0f) * 255.0f + 0.5f);
			png_pixel_data.at(pixel_index * 4 + 2) = (unsigned char)(glm::clamp(pixel_color.z, 0.0f, 1.0f) * 255.0f + 0.5f);
			png_pixel_data.at(pixel_index * 4 + 3) = (unsigned char)(glm::clamp(pixel_color.w, 0.0f, 1.0f) * 255.0f + 0.5f);
		}

		return stbi_write_png(png_file_path.string().c_str(), image_width, image_height, 4, png_pixel_data.data(), image_width * 4) != 0;
	}
};
