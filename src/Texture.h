#pragma once

#include <driver_types.h>

#include "MathCommon.h"
#include "SceneDefines.h"
#include "TextureManager.h"

namespace YumeRT
{
#define TEXTURE_INVALID_VALUE 1E30f

	enum TEXTURE_TYPE
	{
		CONSTANT_TEXTURE_FLOAT = 0,
		CONSTANT_TEXTURE_RGB = 1,
		SOLID_TEXTURE_CHECKERBOARD = 2,
		SOLID_TEXTURE_NOISE = 3
	};

	struct Texture;

	struct TextureCoordinate
	{
		glm::vec2 st;
		glm::vec3 p_world;
		glm::vec3 p_object;

		__device__ __host__ inline TextureCoordinate() :st(glm::vec2(0.0f)), p_world(glm::vec3(0.0f)), p_object(glm::vec3(0.0f)) {}
		__device__ __host__ inline TextureCoordinate(const glm::vec2 &uv, 
			const glm::vec3 &position_world, 
			const glm::vec3 &position_object) :st(uv), p_world(position_world), p_object(position_object) {}
	};

	struct ConstantTextureFloat 
	{
		float value;

		// parent location in the stack.
		int16_t parent_index;
		// specify current texture's child_index of its parent.
		int16_t ith_child; 

		__device__ __host__ inline ConstantTextureFloat(float val) : value(val), parent_index(-1), ith_child(-1) {}
		__device__ __host__ inline ConstantTextureFloat& operator=(const ConstantTextureFloat& other)
		{
			value = other.value;

			parent_index = other.parent_index;
			ith_child = other.ith_child;

			return *this;
		}
	};

	struct ConstantTextureRGB
	{
		glm::vec3 color;

		// parent location in the stack.
		int16_t parent_index;
		// specify current texture's child_index of its parent.
		int16_t ith_child;

		__device__ __host__ inline ConstantTextureRGB(const glm::vec3& col) : color(col), parent_index(-1), ith_child(-1) {}
		__device__ __host__ inline ConstantTextureRGB& operator=(const ConstantTextureRGB& other)
		{
			color = other.color;

			parent_index = other.parent_index;
			ith_child = other.ith_child;

			return *this;
		}
	};

	struct CheckerBoardTexture
	{
		// texture connection structure
		union
		{
			uint32_t indices[2];
			struct {
				uint32_t texture_black;
				uint32_t texture_white;
			}child_texture_indices;
		};
		
		union
		{
			glm::vec3 results[2]; // store the result from sub-tree
			struct { 
				glm::vec3 col_black;
				glm::vec3	col_white; 
			}child_texture_results;
		};

		// parent location in the stack.
		int16_t parent_index;
		// specify current texture's child_index of its parent.
		int16_t ith_child;

		// texture shading attributes
		float frequency;

		__device__ __host__ inline CheckerBoardTexture(uint32_t texture_black_idx, uint32_t texture_white_idx, float frequency)
			:indices{ texture_black_idx, texture_white_idx }, 
			results{ glm::vec3(TEXTURE_INVALID_VALUE), glm::vec3(TEXTURE_INVALID_VALUE) },
			parent_index(-1), ith_child(-1), 
			frequency(frequency) {}

		__device__ __host__ inline CheckerBoardTexture& operator=(const CheckerBoardTexture& other)
		{
			indices[0] = other.indices[0];
			indices[1] = other.indices[1];

			results[0] = other.results[0];
			results[1] = other.results[1];

			parent_index = other.parent_index;
			ith_child = other.ith_child;

			frequency = other.frequency;
			return *this;
		}
	};

	struct NoiseTexture 
	{
		// texture connection structure
		union
		{
			uint32_t indices[2];
			struct {
				uint32_t texture_black;
				uint32_t texture_white;
			}child_texture_indices;
		};

		union
		{
			glm::vec3 results[2]; // store the result from sub-tree
			struct {
				glm::vec3 col_black;
				glm::vec3	col_white;
			}child_texture_results;
		};

		// parent location in the stack.
		int16_t parent_index;
		// specify current texture's child_index of its parent.
		int16_t ith_child;

		// texture shading attributes
		float frequency;
		bool normalized;

		__device__ __host__ inline NoiseTexture(uint32_t texture_black_idx, uint32_t texture_white_idx, float frequency, bool normalized)
			: indices{ texture_black_idx, texture_white_idx },
			results{ glm::vec3(TEXTURE_INVALID_VALUE), glm::vec3(TEXTURE_INVALID_VALUE) },
			parent_index(-1), ith_child(-1), 
			normalized(normalized), frequency(frequency) {}

		__device__ __host__ inline NoiseTexture& operator=(const NoiseTexture& other)
		{
			indices[0] = other.indices[0];
			indices[1] = other.indices[1];

			results[0] = other.results[0];
			results[1] = other.results[1];

			parent_index = other.parent_index;
			ith_child = other.ith_child;

			normalized = other.normalized;
			frequency = other.frequency;
			return *this;
		}
	};


	struct Texture
	{
		uint32_t texture_type;
		union
		{
			ConstantTextureFloat constant_texture_float;
			ConstantTextureRGB constant_texture_rgb;
			CheckerBoardTexture checker_board_texture;
			NoiseTexture noise_texture;
		};

		__device__ __host__ inline Texture() {}
		__device__ __host__ inline Texture& operator=(const Texture& other) 
		{
			texture_type = other.texture_type;
			switch (texture_type)
			{
			case CONSTANT_TEXTURE_FLOAT:
				constant_texture_float = other.constant_texture_float;
				break;
			case CONSTANT_TEXTURE_RGB:
				constant_texture_rgb = other.constant_texture_rgb;
				break;
			case SOLID_TEXTURE_CHECKERBOARD:
				checker_board_texture = other.checker_board_texture;
				break;
			case SOLID_TEXTURE_NOISE:
				noise_texture = other.noise_texture;
				break;
			default:
				break;
			}
			return *this;
		}
		
		__device__ __host__ inline int16_t GetChildCount() const
		{
			if (texture_type <= CONSTANT_TEXTURE_RGB)
			{
				return 0;
			}
			else if (texture_type == SOLID_TEXTURE_CHECKERBOARD)
			{
				return 2;
			}
			else if (texture_type == SOLID_TEXTURE_NOISE)
			{
				return 2;
			}
			else 
			{
				return 0;
			}
		}
		__device__ __host__ inline glm::vec3 GetChildResult(int i) const
		{
			if (texture_type <= CONSTANT_TEXTURE_RGB)
			{
				return glm::vec3(0.0f);
			}
			else if (texture_type == SOLID_TEXTURE_CHECKERBOARD)
			{
				return checker_board_texture.results[i];
			}
			else if (texture_type == SOLID_TEXTURE_NOISE)
			{
				return noise_texture.results[i];
			}
			else 
			{
				return glm::vec3(0.0f);
			}
		}
		__device__ __host__ inline void SetResult(const glm::vec3 &col, int i) 
		{
			if (texture_type <= CONSTANT_TEXTURE_RGB)
			{
				// nothing to do
			}
			else if (texture_type == SOLID_TEXTURE_CHECKERBOARD)
			{
				checker_board_texture.results[i] = col;
			}
			else if (texture_type == SOLID_TEXTURE_NOISE)
			{
				noise_texture.results[i] = col;
			}
			else 
			{
			}
		}
		__device__ __host__ inline uint32_t GetChildTextureIndex(int i) const
		{
			if (texture_type <= CONSTANT_TEXTURE_RGB)
			{
				return 0;
			}
			else if (texture_type == SOLID_TEXTURE_CHECKERBOARD)
			{
				return checker_board_texture.indices[i];
			}
			else if (texture_type == SOLID_TEXTURE_NOISE)
			{
				return noise_texture.indices[i];
			}
			else 
			{
				return 0;
			}
		}

		__device__ __host__ inline int16_t GetParentIndex() const 
		{
			if (texture_type == CONSTANT_TEXTURE_FLOAT) 
			{
				return constant_texture_float.parent_index;
			}
			else if (texture_type == CONSTANT_TEXTURE_RGB)
			{
				return constant_texture_rgb.parent_index;
			}
			else if (texture_type == SOLID_TEXTURE_CHECKERBOARD) 
			{
				return checker_board_texture.parent_index;
			}
			else if (texture_type == SOLID_TEXTURE_NOISE) 
			{
				return noise_texture.parent_index;
			}
			else 
			{
				return 0;
			}
		}
		__device__ __host__ inline void SetParentIndex(int16_t parent_index) 
		{
			if (texture_type == CONSTANT_TEXTURE_FLOAT)
			{
				constant_texture_float.parent_index = parent_index;
			}
			else if (texture_type == CONSTANT_TEXTURE_RGB)
			{
				constant_texture_rgb.parent_index = parent_index;
			}
			else if (texture_type == SOLID_TEXTURE_CHECKERBOARD)
			{
				checker_board_texture.parent_index = parent_index;
			}
			else if (texture_type == SOLID_TEXTURE_NOISE)
			{
				noise_texture.parent_index = parent_index;
			}
			else
			{
				
			}
		}
		__device__ __host__ inline int16_t GetIthChild() const 
		{
			if (texture_type == CONSTANT_TEXTURE_FLOAT)
			{
				return constant_texture_float.ith_child;
			}
			else if (texture_type == CONSTANT_TEXTURE_RGB)
			{
				return constant_texture_rgb.ith_child;
			}
			else if (texture_type == SOLID_TEXTURE_CHECKERBOARD)
			{
				return checker_board_texture.ith_child;
			}
			else if (texture_type == SOLID_TEXTURE_NOISE)
			{
				return noise_texture.ith_child;
			}
			else
			{
				return 0;
			}
		}
		__device__ __host__ inline void SetIthChild(int16_t ith_child) 
		{
			if (texture_type == CONSTANT_TEXTURE_FLOAT)
			{
				constant_texture_float.ith_child = ith_child;
			}
			else if (texture_type == CONSTANT_TEXTURE_RGB)
			{
				constant_texture_rgb.ith_child = ith_child;
			}
			else if (texture_type == SOLID_TEXTURE_CHECKERBOARD)
			{
				checker_board_texture.ith_child = ith_child;
			}
			else if (texture_type == SOLID_TEXTURE_NOISE)
			{
				noise_texture.ith_child = ith_child;
			}
			else
			{
				
			}
		}

		__device__ __host__ inline Texture& InitConstantTextureFloat(float val)
		{
			texture_type = CONSTANT_TEXTURE_FLOAT;
			constant_texture_float = ConstantTextureFloat(val);
			return *this;
		}
		__device__ __host__ inline Texture& InitConstantTextureRGB(const glm::vec3& col)
		{
			texture_type = CONSTANT_TEXTURE_RGB;
			constant_texture_rgb = ConstantTextureRGB(col);
			return *this;
		}
		__device__ __host__ inline Texture& InitCheckerBoardTexture(uint32_t texture_black_idx, uint32_t texture_white_idx, float freqency = 1.0f)
		{
			texture_type = SOLID_TEXTURE_CHECKERBOARD;
			checker_board_texture = CheckerBoardTexture(texture_black_idx, texture_white_idx, freqency);
			return *this;
		}
		__device__ __host__ inline Texture& InitNoiseTexture(uint32_t texture_black_idx, uint32_t texture_white_idx, float frequency = 1.0f, bool normalized = true)
		{
			texture_type = SOLID_TEXTURE_NOISE;
			noise_texture = NoiseTexture(texture_black_idx, texture_white_idx, frequency, normalized);
			return *this;
		}
	};

	
};

