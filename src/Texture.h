#pragma once

#include <driver_types.h>

#include "MathCommon.h"
#include "SceneDefines.h"
#include "TextureManager.h"

namespace YumeRT
{
#define TEXTURE_INVALID_VALUE 1E36f

	enum TEXTURE_TYPE
	{
		CONSTANT_TEXTURE_FLOAT = 0,
		CONSTANT_TEXTURE_RGB = 1,
		SOLID_TEXTURE_CHECKERBOARD = 2,
		SOLID_TEXTURE_NOISE = 3,
		SOLID_TEXTURE_FBM = 4,
		SOLID_TEXTURE_TURBULENCE = 5,
		SOLID_TEXTURE_MARBLE = 6,
		SOLID_TEXTURE_WOOD = 7,
		SOLID_TEXTURE_POLKA_DOT = 8,
		SOLID_TEXTURE_WAVE = 9
	};

	constexpr int texture_type_count = 10;
	const char* const texture_type_names[texture_type_count] = 
	{
		"constant float", 
		"constant rgb", 
		"checkerboard", 
		"perlin noise",
		"fbm",
		"turbulence",
		"marble",
		"wood",
		"polka dot",
		"wave"
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

	struct NoiseTextureFBM
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
		float lacunarity;
		float gain;
		int layer_count;

		float amplitude;
		float offset;

		__device__ __host__ inline NoiseTextureFBM(uint32_t texture_black_idx, uint32_t texture_white_idx, float frequency, float lacunarity, float gain, int layer_count, float amplitude, float offset)
			: indices{ texture_black_idx, texture_white_idx },
			results{ glm::vec3(TEXTURE_INVALID_VALUE), glm::vec3(TEXTURE_INVALID_VALUE) },
			parent_index(-1), ith_child(-1),
			frequency(frequency), lacunarity(lacunarity), gain(gain), layer_count(layer_count), amplitude(amplitude), offset(offset){}

		__device__ __host__ inline NoiseTextureFBM& operator=(const NoiseTextureFBM& other)
		{
			indices[0] = other.indices[0];
			indices[1] = other.indices[1];

			results[0] = other.results[0];
			results[1] = other.results[1];

			parent_index = other.parent_index;
			ith_child = other.ith_child;

			frequency = other.frequency;
			lacunarity = other.lacunarity;
			gain = other.gain;
			layer_count = other.layer_count; 

			amplitude = other.amplitude;
			offset = other.offset;

			return *this;
		}
	};

	struct NoiseTextureTurbulence
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
		float lacunarity;
		float gain;
		int layer_count;

		float amplitude;
		float offset;

		__device__ __host__ inline NoiseTextureTurbulence(uint32_t texture_black_idx, uint32_t texture_white_idx, float frequency, float lacunarity, float gain, int layer_count, float amplitude, float offset)
			: indices{ texture_black_idx, texture_white_idx },
			results{ glm::vec3(TEXTURE_INVALID_VALUE), glm::vec3(TEXTURE_INVALID_VALUE) },
			parent_index(-1), ith_child(-1),
			frequency(frequency), lacunarity(lacunarity), gain(gain), layer_count(layer_count), amplitude(amplitude), offset(offset){}

		__device__ __host__ inline NoiseTextureTurbulence& operator=(const NoiseTextureTurbulence& other)
		{
			indices[0] = other.indices[0];
			indices[1] = other.indices[1];

			results[0] = other.results[0];
			results[1] = other.results[1];

			parent_index = other.parent_index;
			ith_child = other.ith_child;

			frequency = other.frequency;
			lacunarity = other.lacunarity;
			gain = other.gain;
			layer_count = other.layer_count;

			amplitude = other.amplitude;
			offset = other.offset;

			return *this;
		}
	};

	struct NoiseTextureMarble
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
		float lacunarity;
		float gain;
		int layer_count;
		float variation;

		bool x_turb;
		bool y_turb;
		bool z_turb;
		bool dummy;

		__device__ __host__ inline NoiseTextureMarble(uint32_t texture_black_idx, uint32_t texture_white_idx, 
			float frequency, float lacunarity, float gain, int layer_count, float variation, bool x_turb, bool y_turb, bool z_turb)
			: indices{ texture_black_idx, texture_white_idx },
			results{ glm::vec3(TEXTURE_INVALID_VALUE), glm::vec3(TEXTURE_INVALID_VALUE) },
			parent_index(-1), ith_child(-1),
			frequency(frequency), lacunarity(lacunarity), gain(gain), layer_count(layer_count), variation(variation), x_turb(x_turb), y_turb(y_turb), z_turb(z_turb){}

		__device__ __host__ inline NoiseTextureMarble& operator=(const NoiseTextureMarble& other)
		{
			indices[0] = other.indices[0];
			indices[1] = other.indices[1];

			results[0] = other.results[0];
			results[1] = other.results[1];

			parent_index = other.parent_index;
			ith_child = other.ith_child;

			frequency = other.frequency;
			lacunarity = other.lacunarity;
			gain = other.gain;
			layer_count = other.layer_count;

			variation = other.variation;

			x_turb = other.x_turb;
			y_turb = other.y_turb;
			z_turb = other.z_turb;
			dummy = other.dummy;
			return *this;
		}
	};

	struct NoiseTextureWood
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
		float variation;


		__device__ __host__ inline NoiseTextureWood(uint32_t texture_black_idx, uint32_t texture_white_idx, float frequency, float variation)
			: indices{ texture_black_idx, texture_white_idx },
			results{ glm::vec3(TEXTURE_INVALID_VALUE), glm::vec3(TEXTURE_INVALID_VALUE) },
			parent_index(-1), ith_child(-1),
			frequency(frequency),  variation(variation){}

		__device__ __host__ inline NoiseTextureWood& operator=(const NoiseTextureWood& other)
		{
			indices[0] = other.indices[0];
			indices[1] = other.indices[1];

			results[0] = other.results[0];
			results[1] = other.results[1];

			parent_index = other.parent_index;
			ith_child = other.ith_child;

			frequency = other.frequency;
			variation = other.variation;

			return *this;
		}
	};

	struct NoiseTexturePolkaDot
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
		float radius;


		__device__ __host__ inline NoiseTexturePolkaDot(uint32_t texture_black_idx, uint32_t texture_white_idx, float frequency, float radius)
			: indices{ texture_black_idx, texture_white_idx },
			results{ glm::vec3(TEXTURE_INVALID_VALUE), glm::vec3(TEXTURE_INVALID_VALUE) },
			parent_index(-1), ith_child(-1),  
			frequency(frequency),  radius(radius){}

		__device__ __host__ inline NoiseTexturePolkaDot& operator=(const NoiseTexturePolkaDot& other)
		{
			indices[0] = other.indices[0];
			indices[1] = other.indices[1];

			results[0] = other.results[0];
			results[1] = other.results[1];

			parent_index = other.parent_index;
			ith_child = other.ith_child;

			frequency = other.frequency;
			radius = other.radius;

			return *this;
		}
	};

	struct NoiseTextureWave
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
		float freq_0;
		float lacunarity_0;
		float gain_0;
		int layer_count_0;
		float freq_1;
		float lacunarity_1;
		float gain_1;
		int layer_count_1;

		__device__ __host__ inline NoiseTextureWave(uint32_t texture_black_idx, uint32_t texture_white_idx,
			float freq_0, float lacunarity_0, float gain_0, int layer_count_0,
			float freq_1, float lacunarity_1, float gain_1, int layer_count_1)
			: indices{ texture_black_idx, texture_white_idx },
			results{ glm::vec3(TEXTURE_INVALID_VALUE), glm::vec3(TEXTURE_INVALID_VALUE) },
			parent_index(-1), ith_child(-1), 
			freq_0(freq_0),  lacunarity_0(lacunarity_0),  gain_0(gain_0),  layer_count_0(layer_count_0),
			freq_1(freq_1), lacunarity_1(lacunarity_1), gain_1(gain_1), layer_count_1(layer_count_1){}

		__device__ __host__ inline NoiseTextureWave& operator=(const NoiseTextureWave& other)
		{
			indices[0] = other.indices[0];
			indices[1] = other.indices[1];

			results[0] = other.results[0];
			results[1] = other.results[1];

			parent_index = other.parent_index;
			ith_child = other.ith_child;

			 freq_0 = other.freq_0;
			 lacunarity_0 = other.lacunarity_0;
			 gain_0 = other.gain_0;
			 layer_count_0 = other.layer_count_0;
			 freq_1 = other.freq_1;
			 lacunarity_1 = other.lacunarity_1;
			 gain_1 = other.gain_1;
			 layer_count_1 = other.layer_count_1;

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
			NoiseTextureFBM noise_texture_fbm;
			NoiseTextureTurbulence noise_texture_turbulence;
			NoiseTextureMarble noise_texture_marble;
			NoiseTextureWood noise_texture_wood;
			NoiseTexturePolkaDot noise_texture_polka_dot;
			NoiseTextureWave noise_texture_wave;
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
			case SOLID_TEXTURE_FBM:
				noise_texture_fbm = other.noise_texture_fbm;
				break;
			case SOLID_TEXTURE_TURBULENCE:
				noise_texture_turbulence = other.noise_texture_turbulence;
				break;
			case SOLID_TEXTURE_MARBLE:
				noise_texture_marble = other.noise_texture_marble;
				break;
			case SOLID_TEXTURE_WOOD:
				noise_texture_wood = other.noise_texture_wood;
				break;
			case SOLID_TEXTURE_POLKA_DOT:
				noise_texture_polka_dot = other.noise_texture_polka_dot;
				break;
			case SOLID_TEXTURE_WAVE:
				noise_texture_wave = other.noise_texture_wave;
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
			else if (texture_type == SOLID_TEXTURE_FBM)
			{
				return 2;
			}
			else if (texture_type == SOLID_TEXTURE_TURBULENCE)
			{
				return 2;
			}
			else if (texture_type == SOLID_TEXTURE_MARBLE)
			{
				return 2;
			}
			else if (texture_type == SOLID_TEXTURE_WOOD)
			{
				return 2;
			}
			else if (texture_type == SOLID_TEXTURE_POLKA_DOT)
			{
				return 2;
			}
			else if (texture_type == SOLID_TEXTURE_WAVE)
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
			else if (texture_type == SOLID_TEXTURE_FBM)
			{
				return noise_texture_fbm.results[i];
			}
			else if (texture_type == SOLID_TEXTURE_TURBULENCE)
			{
				return noise_texture_turbulence.results[i];
			}
			else if (texture_type == SOLID_TEXTURE_MARBLE)
			{
				return noise_texture_marble.results[i];
			}
			else if (texture_type == SOLID_TEXTURE_WOOD)
			{
				return noise_texture_wood.results[i];
			}
			else if (texture_type == SOLID_TEXTURE_POLKA_DOT)
			{
				return noise_texture_polka_dot.results[i];
			}
			else if (texture_type == SOLID_TEXTURE_WAVE)
			{
				return noise_texture_wave.results[i];
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
				noise_texture.results[i].x = col.x;
				noise_texture.results[i].y = col.y;
				noise_texture.results[i].z = col.z;
			}
			else if (texture_type == SOLID_TEXTURE_FBM)
			{
				noise_texture_fbm.results[i] = col;
			}
			else if (texture_type == SOLID_TEXTURE_TURBULENCE)
			{
				noise_texture_turbulence.results[i] = col;
			}
			else if (texture_type == SOLID_TEXTURE_MARBLE)
			{
				noise_texture_marble.results[i] = col;
			}
			else if (texture_type == SOLID_TEXTURE_WOOD)
			{
				noise_texture_wood.results[i] = col;
			}
			else if (texture_type == SOLID_TEXTURE_POLKA_DOT)
			{
				noise_texture_polka_dot.results[i].x = col.x;
				noise_texture_polka_dot.results[i].y = col.y;
				noise_texture_polka_dot.results[i].z = col.z;
			}
			else if (texture_type == SOLID_TEXTURE_WAVE)
			{
				noise_texture_wave.results[i] = col;
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
			else if (texture_type == SOLID_TEXTURE_FBM)
			{
				return noise_texture_fbm.indices[i];
			}
			else if (texture_type == SOLID_TEXTURE_TURBULENCE)
			{
				return noise_texture_turbulence.indices[i];
			}
			else if (texture_type == SOLID_TEXTURE_MARBLE)
			{
				return noise_texture_marble.indices[i];
			}
			else if (texture_type == SOLID_TEXTURE_WOOD)
			{
				return noise_texture_wood.indices[i];
			}
			else if (texture_type == SOLID_TEXTURE_POLKA_DOT)
			{
				return noise_texture_polka_dot.indices[i];
			}
			else if (texture_type == SOLID_TEXTURE_WAVE)
			{
				return noise_texture_wave.indices[i];
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
			else if (texture_type == SOLID_TEXTURE_FBM)
			{
				return noise_texture_fbm.parent_index;
			}
			else if (texture_type == SOLID_TEXTURE_TURBULENCE)
			{
				return noise_texture_turbulence.parent_index;
			}
			else if (texture_type == SOLID_TEXTURE_MARBLE)
			{
				return noise_texture_marble.parent_index;
			}
			else if (texture_type == SOLID_TEXTURE_WOOD)
			{
				return noise_texture_wood.parent_index;
			}
			else if (texture_type == SOLID_TEXTURE_POLKA_DOT)
			{
				return noise_texture_polka_dot.parent_index;
			}
			else if (texture_type == SOLID_TEXTURE_WAVE)
			{
				return noise_texture_wave.parent_index;
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
			else if (texture_type == SOLID_TEXTURE_FBM)
			{
				noise_texture_fbm.parent_index = parent_index;
			}
			else if (texture_type == SOLID_TEXTURE_TURBULENCE)
			{
				noise_texture_turbulence.parent_index = parent_index;
			}
			else if (texture_type == SOLID_TEXTURE_MARBLE)
			{
				noise_texture_marble.parent_index = parent_index;
			}
			else if (texture_type == SOLID_TEXTURE_WOOD)
			{
				noise_texture_wood.parent_index = parent_index;
			}
			else if (texture_type == SOLID_TEXTURE_POLKA_DOT)
			{
				noise_texture_polka_dot.parent_index = parent_index;
			}
			else if (texture_type == SOLID_TEXTURE_WAVE)
			{
				noise_texture_wave.parent_index = parent_index;
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
			else if (texture_type == SOLID_TEXTURE_FBM)
			{
				return noise_texture_fbm.ith_child;
			}
			else if (texture_type == SOLID_TEXTURE_TURBULENCE)
			{
				return noise_texture_turbulence.ith_child;
			}
			else if (texture_type == SOLID_TEXTURE_MARBLE)
			{
				return noise_texture_marble.ith_child;
			}
			else if (texture_type == SOLID_TEXTURE_WOOD)
			{
				return noise_texture_wood.ith_child;
			}
			else if (texture_type == SOLID_TEXTURE_POLKA_DOT)
			{
				return noise_texture_polka_dot.ith_child;
			}
			else if (texture_type == SOLID_TEXTURE_WAVE)
			{
				return noise_texture_wave.ith_child;
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
			else if (texture_type == SOLID_TEXTURE_FBM)
			{
				noise_texture_fbm.ith_child = ith_child;
			}
			else if (texture_type == SOLID_TEXTURE_TURBULENCE)
			{
				noise_texture_turbulence.ith_child = ith_child;
			}
			else if (texture_type == SOLID_TEXTURE_MARBLE)
			{
				noise_texture_marble.ith_child = ith_child;
			}
			else if (texture_type == SOLID_TEXTURE_WOOD)
			{
				noise_texture_wood.ith_child = ith_child;
			}
			else if (texture_type == SOLID_TEXTURE_POLKA_DOT)
			{
				noise_texture_polka_dot.ith_child = ith_child;
			}
			else if (texture_type == SOLID_TEXTURE_WAVE)
			{
				noise_texture_wave.ith_child = ith_child;
			}
			else
			{
				
			}
		}

		__device__ __host__ inline float GetUpperBound(const glm::vec3 &ray_origin, const glm::vec3 &ray_direction, float t_max) const
		{
			if (texture_type == CONSTANT_TEXTURE_FLOAT)
			{
				return constant_texture_float.value;
			}
			else if (texture_type == CONSTANT_TEXTURE_RGB)
			{
				return glm::max(constant_texture_rgb.color.x, glm::max(constant_texture_rgb.color.y, constant_texture_rgb.color.z));
			}
			else if (texture_type == SOLID_TEXTURE_CHECKERBOARD)
			{
				return 1.0f;
			}
			else if (texture_type == SOLID_TEXTURE_NOISE)
			{
				return 1.0f;
			}
			else if (texture_type == SOLID_TEXTURE_FBM)
			{
				return (noise_texture_fbm.amplitude + noise_texture_fbm.offset) * SafeRcp(1.0f - noise_texture_fbm.gain);
			}
			else if (texture_type == SOLID_TEXTURE_TURBULENCE)
			{
				return (noise_texture_turbulence.amplitude + noise_texture_turbulence.offset) * SafeRcp(1.0f - noise_texture_turbulence.gain);
			}
			else if (texture_type == SOLID_TEXTURE_MARBLE)
			{
				return 1.0f;
			}
			else if (texture_type == SOLID_TEXTURE_WOOD)
			{
				return 1.0f;
			}
			else if (texture_type == SOLID_TEXTURE_POLKA_DOT)
			{
				return 1.0f;
			}
			else if (texture_type == SOLID_TEXTURE_WAVE)
			{
				return  SafeRcp(1.0f - noise_texture_wave.gain_0) * SafeRcp(1.0f - noise_texture_wave.gain_1);
			}
			else
			{
				return 0.0f;
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
		__device__ __host__ inline Texture& InitNoiseTextureFBM(uint32_t texture_black_idx, uint32_t texture_white_idx, float frequency = 16.0f, float lacunarity = 2.0f, float gain = 0.5f, int layer_count = 4, float amplitude = 1.0f, float offset = 0.0f)
		{
			texture_type = SOLID_TEXTURE_FBM;
			noise_texture_fbm = NoiseTextureFBM(texture_black_idx, texture_white_idx, frequency, lacunarity, gain, layer_count, amplitude, offset);
			return *this;
		}
		__device__ __host__ inline Texture& InitNoiseTextureTurbulence(uint32_t texture_black_idx, uint32_t texture_white_idx, float frequency = 16.0f, float lacunarity = 2.0f, float gain = 0.5f, int layer_count = 4, float amplitude = 1.0f, float offset = 0.0f)
		{
			texture_type = SOLID_TEXTURE_TURBULENCE;
			noise_texture_turbulence = NoiseTextureTurbulence(texture_black_idx, texture_white_idx, frequency, lacunarity, gain, layer_count, amplitude, offset);
			return *this;
		}
		__device__ __host__ inline Texture& InitNoiseTextureMarble(uint32_t texture_black_idx, uint32_t texture_white_idx, float frequency = 16.0f, float lacunarity = 2.0f, float gain = 0.5f, int layer_count = 4, float variation = 10.0f, bool x_turb = true, bool y_turb = false, bool z_turb = false)
		{
			texture_type = SOLID_TEXTURE_MARBLE;
			noise_texture_marble = NoiseTextureMarble(texture_black_idx, texture_white_idx, frequency, lacunarity, gain, layer_count, variation, x_turb, y_turb, z_turb);
			return *this;
		}
		__device__ __host__ inline Texture& InitNoiseTextureWood(uint32_t texture_black_idx, uint32_t texture_white_idx, float frequency = 2.0f, float variation = 10.0f)
		{
			texture_type = SOLID_TEXTURE_WOOD;
			noise_texture_wood = NoiseTextureWood(texture_black_idx, texture_white_idx, frequency, variation);
			return *this;
		}
		__device__ __host__ inline Texture& InitNoiseTexturePolkaDot(uint32_t texture_black_idx, uint32_t texture_white_idx, float frequency = 8.0f, float radius = 0.35f)
		{
			texture_type = SOLID_TEXTURE_POLKA_DOT;
			noise_texture_polka_dot = NoiseTexturePolkaDot(texture_black_idx, texture_white_idx, frequency, radius);
			return *this;
		}
		__device__ __host__ inline Texture& InitNoiseTextureWave(uint32_t texture_black_idx, uint32_t texture_white_idx, 
			float freq_0 = 0.1f, float lacunarity_0 = 2.0f, float gain_0 = 0.5f, int layer_count_0 = 2,
			float freq_1 = 2.0f, float lacunarity_1 = 2.0f, float gain_1 = 0.5f, int layer_count_1 = 4)
		{
			texture_type = SOLID_TEXTURE_WAVE;
			noise_texture_wave = NoiseTextureWave(texture_black_idx, texture_white_idx, 
				freq_0, lacunarity_0, gain_0, layer_count_0,
				freq_1, lacunarity_1, gain_1, layer_count_1);
			return *this;
		}
	};

	
};

