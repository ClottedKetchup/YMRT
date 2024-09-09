#pragma once

#include "GeometryDefines.h"
#include "Texture.h"

namespace YumeRT
{
	ImageTexture LoadImageTexture(const std::string &file_name, 
		std::vector<ImageFile> &image_texture_files, 
		std::vector<ImageTile>&image_texture_tiles);
};