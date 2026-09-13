#pragma once

#include "GeometryDefines.h"
#include "Texture.h"

namespace YMRT
{
	ImageTexture LoadImageTexture(const std::string &file_name,
		std::vector<ImageFile> &image_texture_files,
		std::vector<ImageTile>&image_texture_tiles,
		const int image_texture_color_space);

	bool SavePngImage(const std::string &file_path, const std::vector<glm::vec4> &host_image, const int image_width, const int image_height);
};