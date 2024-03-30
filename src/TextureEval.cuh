#pragma once

#include <cuda_runtime.h>
#include <vector_types.h>
#include <driver_types.h>

#include <stdio.h>

#include "MathCommon.h"
#include "Texture.h"

namespace YumeRT
{

#define PERLIN_NOISE_RANDOM_VECTOR_COUNT 16
#define PERLIN_NOISE_GRID_SIZE 256
#define PERLIN_NOISE_PERMUTE_TABLE_SIZE 512

	__constant__ uint8_t perlin_noise_permute_table[PERLIN_NOISE_PERMUTE_TABLE_SIZE] =
	{
		172, 142, 240, 182, 179, 214, 29, 111, 156, 3, 131, 161, 164, 194, 243, 124,
		127, 236, 88, 166, 83, 134, 71, 183, 227, 235, 106, 226, 216, 99, 149, 241,
		206, 63, 48, 31, 20, 171, 46, 185, 6, 125, 184, 133, 199, 201, 58, 81,
		177, 174, 92, 78, 32, 190, 89, 229, 14, 159, 196, 69, 242, 222, 13, 54,
		200, 50, 26, 218, 5, 101, 18, 72, 203, 191, 104, 141, 252, 65, 139, 210,
		22, 180, 113, 253, 27, 102, 25, 169, 56, 110, 178, 105, 254, 80, 73, 4,
		90, 155, 91, 120, 181, 9, 123, 198, 193, 118, 146, 44, 247, 162, 231, 143,
		153, 160, 57, 112, 238, 77, 95, 34, 168, 195, 126, 224, 213, 187, 41, 42,
		152, 219, 220, 39, 53, 138, 239, 157, 12, 38, 24, 188, 76, 7, 103, 1,
		33, 130, 74, 189, 79, 232, 96, 86, 135, 35, 234, 205, 150, 165, 30, 109,
		212, 66, 0, 228, 129, 23, 97, 8, 107, 209, 192, 175, 11, 52, 16, 248,
		55, 28, 173, 115, 51, 163, 197, 2, 208, 64, 148, 37, 19, 167, 84, 61,
		176, 211, 17, 93, 108, 121, 116, 87, 43, 249, 147, 122, 21, 117, 136, 233,
		145, 15, 245, 59, 75, 137, 151, 255, 154, 204, 244, 98, 40, 70, 10, 85,
		225, 223, 251, 158, 132, 45, 119, 114, 60, 67, 140, 144, 207, 215, 47, 246,
		230, 186, 250, 100, 36, 221, 217, 68, 128, 49, 62, 237, 82, 94, 170, 202,
		172, 142, 240, 182, 179, 214, 29, 111, 156, 3, 131, 161, 164, 194, 243, 124,
		127, 236, 88, 166, 83, 134, 71, 183, 227, 235, 106, 226, 216, 99, 149, 241,
		206, 63, 48, 31, 20, 171, 46, 185, 6, 125, 184, 133, 199, 201, 58, 81,
		177, 174, 92, 78, 32, 190, 89, 229, 14, 159, 196, 69, 242, 222, 13, 54,
		200, 50, 26, 218, 5, 101, 18, 72, 203, 191, 104, 141, 252, 65, 139, 210,
		22, 180, 113, 253, 27, 102, 25, 169, 56, 110, 178, 105, 254, 80, 73, 4,
		90, 155, 91, 120, 181, 9, 123, 198, 193, 118, 146, 44, 247, 162, 231, 143,
		153, 160, 57, 112, 238, 77, 95, 34, 168, 195, 126, 224, 213, 187, 41, 42,
		152, 219, 220, 39, 53, 138, 239, 157, 12, 38, 24, 188, 76, 7, 103, 1,
		33, 130, 74, 189, 79, 232, 96, 86, 135, 35, 234, 205, 150, 165, 30, 109,
		212, 66, 0, 228, 129, 23, 97, 8, 107, 209, 192, 175, 11, 52, 16, 248,
		55, 28, 173, 115, 51, 163, 197, 2, 208, 64, 148, 37, 19, 167, 84, 61,
		176, 211, 17, 93, 108, 121, 116, 87, 43, 249, 147, 122, 21, 117, 136, 233,
		145, 15, 245, 59, 75, 137, 151, 255, 154, 204, 244, 98, 40, 70, 10, 85,
		225, 223, 251, 158, 132, 45, 119, 114, 60, 67, 140, 144, 207, 215, 47, 246,
		230, 186, 250, 100, 36, 221, 217, 68, 128, 49, 62, 237, 82, 94, 170, 202
	};

	__device__ __host__ inline float PerlinNoiseEval(const glm::vec3 &p)
	{
		int x0 = (int)glm::floor(p.x);
		int y0 = (int)glm::floor(p.y);
		int z0 = (int)glm::floor(p.z);

		float tx = p.x - (float)x0;
		float ty = p.y - (float)y0;
		float tz = p.z - (float)z0;

		constexpr int table_size_mask = PERLIN_NOISE_GRID_SIZE - 1;

		int x1 = (x0 + 1) & table_size_mask;
		int y1 = (y0 + 1) & table_size_mask;
		int z1 = (z0 + 1) & table_size_mask;
		x0 = x0 & table_size_mask;
		y0 = y0 & table_size_mask;
		z0 = z0 & table_size_mask;

		// permute the grid index
		auto hash = [](int x, int y, int z)->uint8_t
		{
			return perlin_noise_permute_table[perlin_noise_permute_table[perlin_noise_permute_table[x] + y] + z];
		};
		// parameter: permuted grid index and vector from grid vertex to the position
		auto dot_product = [&](const int grid_idx, float x, float y, float z) ->float
		{
			constexpr int mask = PERLIN_NOISE_RANDOM_VECTOR_COUNT - 1;
			switch ((grid_idx & mask))
			{
				// ken-perlin defined random vector on lattice vertex
				case  0: return  x + y;
				case  1: return -x + y;
				case  2: return  x - y;
				case  3: return -x - y;
				case  4: return  x + z;
				case  5: return -x + z;
				case  6: return  x - z;
				case  7: return -x - z;
				case  8: return  y + z;
				case  9: return -y + z;
				case 10: return  y - z;
				case 11: return -y - z;
				case 12: return  y + x;
				case 13: return -x + y;
				case 14: return -y + z;
				case 15: return -y - z;
				default: return 0;
			}
		};
		// lerp function
		auto interpolate = [](float t)->float
		{
			return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
		};

		const float v000 = dot_product(hash(x0, y0, z0), tx, ty, tz);
		const float v100 = dot_product(hash(x1, y0, z0), tx - 1, ty, tz);
		const float v010 = dot_product(hash(x0, y1, z0), tx, ty - 1, tz);
		const float v001 = dot_product(hash(x0, y0, z1), tx, ty, tz - 1);
		const float v110 = dot_product(hash(x1, y1, z0), tx - 1, ty - 1, tz);
		const float v011 = dot_product(hash(x0, y1, z1), tx, ty - 1, tz - 1);
		const float v101 = dot_product(hash(x1, y0, z1), tx - 1, ty, tz - 1);
		const float v111 = dot_product(hash(x1, y1, z1), tx - 1, ty - 1, tz - 1);

		float ix = interpolate(tx);
		float iy = interpolate(ty);
		float iz = interpolate(tz);

		const float v00 = v000 + ix * (v100 - v000);
		const float v01 = v001 + ix * (v101 - v001);
		const float v10 = v010 + ix * (v110 - v010);
		const float v11 = v011 + ix * (v111 - v011);

		const float v0 = v00 + iy * (v10 - v00);
		const float v1 = v01 + iy * (v11 - v01);

		return v0 + iz * (v1 - v0);
	}
	__device__ __host__ inline float PerlinNoiseFBM(const glm::vec3 &p, float lacunarity, float gain, int num_layer, float amplitude = 1.0f, float offset = 0.0f)
	{
		float fbm_result = 0.0f;
		float frequency = 1.0f;
		for (int i = 0; i < num_layer; ++i)
		{
			fbm_result += amplitude * PerlinNoiseEval(frequency * p) + offset;
			amplitude *= gain;
			offset *= gain;
			frequency *= lacunarity;
		}
		return fbm_result;
	}
	__device__ __host__ inline float PerlinNoiseTurbulence(const glm::vec3 &p, float lacunarity, float gain, int num_layer, float amplitude = 1.0f, float offset = 0.0f)
	{
		float turbulence_result = 0.0f;
		float frequency = 1.0f;
		for (int i = 0; i < num_layer; ++i)
		{
			turbulence_result += amplitude * glm::abs(PerlinNoiseEval(frequency * p)) + offset;
			amplitude *= gain;
			offset *= gain;
			frequency *= lacunarity;
		}
		assert(turbulence_result >= 0.0f);
		return turbulence_result;
	}
	
	__device__ __host__ inline glm::vec3 EvalTexConstantFloat(const Texture& texture, const TextureManager& texture_manager, const TextureCoordinate& texture_coordinate)
	{
		return glm::vec3(texture.constant_texture_float.value);
	}
	__device__ __host__ inline glm::vec3 EvalTexConstantRGB(const Texture& texture, const TextureManager& texture_manager, const TextureCoordinate& texture_coordinate)
	{
		return texture.constant_texture_rgb.color;
	}
	__device__ __host__ inline glm::vec3 EvalTexCheckerBoard(const Texture& texture, const TextureManager& texture_manager, const TextureCoordinate& texture_coordinate)
	{
		const glm::vec3 tex_white_result = texture.checker_board_texture.child_texture_results.col_white;
		const glm::vec3 tex_black_result = texture.checker_board_texture.child_texture_results.col_black;

		float freq = texture.checker_board_texture.frequency;
		float x = glm::sin(texture_coordinate.p_object.x * freq);
		float y = glm::sin(texture_coordinate.p_object.y * freq);
		float z = glm::sin(texture_coordinate.p_object.z * freq);
		return x * y * z < 0.0f ? tex_white_result : tex_black_result;
	}
	__device__ __host__ inline glm::vec3 EvalTexPerlinNoise(const Texture& texture, const TextureManager& texture_manager, const TextureCoordinate& texture_coordinate)
	{
		const glm::vec3 tex_white_result = texture.noise_texture.child_texture_results.col_white;
		const glm::vec3 tex_black_result = texture.noise_texture.child_texture_results.col_black;

		float freq = texture.noise_texture.frequency;
		float noise_val = PerlinNoiseEval(texture_coordinate.p_object * freq);
		if (texture.noise_texture.normalized) { noise_val = noise_val * 0.5f + 0.5f; }
		return tex_black_result + noise_val * (tex_white_result - tex_black_result);
	}
	__device__ __host__ inline glm::vec3 EvalTexPerlinNoiseFBM(const Texture& texture, const TextureManager& texture_manager, const TextureCoordinate& texture_coordinate)
	{
		const glm::vec3 tex_white_result = texture.noise_texture_fbm.child_texture_results.col_white;
		const glm::vec3 tex_black_result = texture.noise_texture_fbm.child_texture_results.col_black;

		float fbm_result = PerlinNoiseFBM(texture.noise_texture_fbm.frequency * texture_coordinate.p_object,
																texture.noise_texture_fbm.lacunarity, 
																texture.noise_texture_fbm.gain, 
																texture.noise_texture_fbm.layer_count, 
																texture.noise_texture_fbm.amplitude, 
																texture.noise_texture_fbm.offset);

		return tex_black_result + fbm_result * (tex_white_result - tex_black_result);
	}
	__device__ __host__ inline glm::vec3 EvalTexPerlinNoiseTurbulence(const Texture& texture, const TextureManager& texture_manager, const TextureCoordinate& texture_coordinate)
	{
		const glm::vec3 tex_white_result = texture.noise_texture_turbulence.child_texture_results.col_white;
		const glm::vec3 tex_black_result = texture.noise_texture_turbulence.child_texture_results.col_black;

		float turbulence_result = PerlinNoiseTurbulence(texture.noise_texture_turbulence.frequency * texture_coordinate.p_object,
																					  texture.noise_texture_turbulence.lacunarity,
																					  texture.noise_texture_turbulence.gain,
																					  texture.noise_texture_turbulence.layer_count, 
																					  texture.noise_texture_turbulence.amplitude, 
																					  texture.noise_texture_turbulence.offset);

		return tex_black_result + turbulence_result * (tex_white_result - tex_black_result);
	}
	__device__ __host__ inline glm::vec3 EvalTexPerlinNoiseMarble(const Texture& texture, const TextureManager& texture_manager, const TextureCoordinate& texture_coordinate)
	{
		const glm::vec3 tex_white_result = texture.noise_texture_marble.child_texture_results.col_white;
		const glm::vec3 tex_black_result = texture.noise_texture_marble.child_texture_results.col_black;

		float fbm_result = PerlinNoiseFBM(texture.noise_texture_marble.frequency * texture_coordinate.p_object,
																texture.noise_texture_marble.lacunarity,
																texture.noise_texture_marble.gain,
																texture.noise_texture_marble.layer_count);
		float turb_coordinate = 0.0f;
		turb_coordinate += texture.noise_texture_marble.x_turb ? texture_coordinate.p_object.x : 0.0f;
		turb_coordinate += texture.noise_texture_marble.y_turb ? texture_coordinate.p_object.y : 0.0f;
		turb_coordinate += texture.noise_texture_marble.z_turb ? texture_coordinate.p_object.z : 0.0f;

		float marble = glm::sin(turb_coordinate + texture.noise_texture_marble.variation * fbm_result) * 0.5f + 0.5f;
		return tex_black_result + marble * (tex_white_result - tex_black_result);
	}
	__device__ __host__ inline glm::vec3 EvalTexPerlinNoiseWood(const Texture& texture, const TextureManager& texture_manager, const TextureCoordinate& texture_coordinate)
	{
		const glm::vec3 tex_white_result = texture.noise_texture_wood.child_texture_results.col_white;
		const glm::vec3 tex_black_result = texture.noise_texture_wood.child_texture_results.col_black;
		
		float noise_val = PerlinNoiseEval(texture_coordinate.p_object * texture.noise_texture_wood.frequency);
		float wood = noise_val * texture.noise_texture_wood.variation;
		wood = wood - floor(wood);
		return tex_black_result + wood * (tex_white_result - tex_black_result);
	}
	__device__ __host__ inline glm::vec3 EvalTexPerlinNoisePolkaDot(const Texture& texture, const TextureManager& texture_manager, const TextureCoordinate& texture_coordinate)
	{
		const glm::vec2 st = texture_coordinate.st * texture.noise_texture_polka_dot.frequency;
		const glm::vec3 tex_white_result = texture.noise_texture_polka_dot.results[1];
		const glm::vec3 tex_black_result = texture.noise_texture_polka_dot.results[0];

		float f_s = floor(st.x);
		float f_t = floor(st.y);
		if (PerlinNoiseEval(glm::vec3(f_s + 0.5f, f_t + 0.5f, 0.0f)) > 0.0f)
		{
			auto simple_hash = [](float f, float multi) 
			{
				return glm::fract(f * multi) * 114.f;
			};

			float radius = texture.noise_texture_polka_dot.radius;
			float max_offset = 0.5f - radius;
			float ds = st.x - f_s;
			float dt = st.y - f_t;
			float s_center = 0.5f + max_offset * PerlinNoiseEval(glm::vec3(simple_hash(st.x, 1145.14f), simple_hash(st.y, 9807.26f), 0.0f));
			float t_center = 0.5f + max_offset * PerlinNoiseEval(glm::vec3(simple_hash(st.x, 4444.44f), simple_hash(st.y, 2333.33f), 0.0f));
			return Sqr(ds - s_center) + Sqr(dt - t_center) < radius * radius ? tex_white_result : tex_black_result;
		}
		else 
		{
			return tex_black_result;
		}
	}
	__device__ __host__ inline glm::vec3 EvalTexPerlinNoiseWave(const Texture& texture, const TextureManager& texture_manager, const TextureCoordinate& texture_coordinate)
	{
		const glm::vec3 tex_white_result = texture.noise_texture_wave.results[1];
		const glm::vec3 tex_black_result = texture.noise_texture_wave.results[0];

		float fbm_0 = PerlinNoiseFBM(texture_coordinate.p_object * texture.noise_texture_wave.freq_0, 
														texture.noise_texture_wave.lacunarity_0, 
														texture.noise_texture_wave.gain_0, 
														texture.noise_texture_wave.layer_count_0);
		float fbm_1 = PerlinNoiseFBM(texture_coordinate.p_object * texture.noise_texture_wave.freq_1,
														texture.noise_texture_wave.lacunarity_1,
														texture.noise_texture_wave.gain_1,
														texture.noise_texture_wave.layer_count_1);
		float wave = glm::abs(fbm_0) * fbm_1;
		return tex_black_result + wave * (tex_white_result - tex_black_result);
	}
	__device__ __host__ inline glm::vec3 EvalTexResult(const Texture& texture, const TextureManager& texture_manager, const TextureCoordinate& texture_coordinate)
	{
		if (texture.texture_type == CONSTANT_TEXTURE_FLOAT)
		{
			return EvalTexConstantFloat(texture, texture_manager, texture_coordinate);
		}
		else if (texture.texture_type == CONSTANT_TEXTURE_RGB)
		{
			return EvalTexConstantRGB(texture, texture_manager, texture_coordinate);
		}
		else if (texture.texture_type == SOLID_TEXTURE_CHECKERBOARD)
		{
			return EvalTexCheckerBoard(texture, texture_manager, texture_coordinate);
		}
		else if (texture.texture_type == SOLID_TEXTURE_NOISE)
		{
			return EvalTexPerlinNoise(texture, texture_manager, texture_coordinate);
		}
		else if (texture.texture_type == SOLID_TEXTURE_FBM)
		{
			return EvalTexPerlinNoiseFBM(texture, texture_manager, texture_coordinate);
		}
		else if (texture.texture_type == SOLID_TEXTURE_TURBULENCE)
		{
			return EvalTexPerlinNoiseTurbulence(texture, texture_manager, texture_coordinate);
		}
		else if (texture.texture_type == SOLID_TEXTURE_MARBLE)
		{
			return EvalTexPerlinNoiseMarble(texture, texture_manager, texture_coordinate);
		}
		else if (texture.texture_type == SOLID_TEXTURE_WOOD)
		{
			return EvalTexPerlinNoiseWood(texture, texture_manager, texture_coordinate);
		}
		else if (texture.texture_type == SOLID_TEXTURE_POLKA_DOT)
		{
			return EvalTexPerlinNoisePolkaDot(texture, texture_manager, texture_coordinate);
		}
		else if (texture.texture_type == SOLID_TEXTURE_WAVE)
		{
			return EvalTexPerlinNoiseWave(texture, texture_manager, texture_coordinate);
		}
		else
		{
			return glm::vec3(0.0f);
		}
	}

	__device__ __host__ glm::vec3 TextureEval(const Texture& texture,
																	const Texture *textures,
																	const TextureManager& texture_manager,
																	const TextureCoordinate& texture_coordinate)
	{
		constexpr int texture_stack_size = 10;
		Texture texture_stack[texture_stack_size];
		int16_t top = -1;

		auto child_texture_eval_complete = [](const Texture& parent_tex, int16_t *next_child)->bool 
		{
			const int16_t child_texture_count = parent_tex.GetChildCount();
			if (child_texture_count == 0) { return true; }
			for (int16_t empty_slot_index = 0; empty_slot_index < child_texture_count; ++empty_slot_index) 
			{
				const glm::vec3 result_i = parent_tex.GetChildResult(empty_slot_index);
				if (!(result_i.x < TEXTURE_INVALID_VALUE && result_i.y < TEXTURE_INVALID_VALUE && result_i.z < TEXTURE_INVALID_VALUE)) 
				{
					*next_child = empty_slot_index;
					return false;
				}
			}
			return true;
		};

		glm::vec3 final_result(0.0f);

		// push the root into stack
		texture_stack[++top] = texture;
		while (top != -1)
		{
			Texture& top_texture = texture_stack[top];
			int16_t next_child_index = -1;

			if (child_texture_eval_complete(top_texture, &next_child_index)) 
			{
				const glm::vec3 top_result = EvalTexResult(top_texture, texture_manager, texture_coordinate);
				int16_t top_tex_parent_idx = top_texture.GetParentIndex();
				if (top_tex_parent_idx != -1)
				{
					Texture &parent_tex = texture_stack[top_tex_parent_idx];
					parent_tex.SetResult(top_result, top_texture.GetIthChild());
					--top;
					continue;
				}
				else 
				{
					// only the root texture's parent index is -1
					final_result = top_result;
					break;
				}
			}
			
			const Texture &next_texture = textures[top_texture.GetChildTextureIndex(next_child_index)];
			int16_t next_tex_parent_idx = top;
			
			// push next child into stack and set its properties
			assert(top + 1 < texture_stack_size);
			texture_stack[++top] = next_texture;
			texture_stack[top].SetParentIndex(next_tex_parent_idx);
			texture_stack[top].SetIthChild(next_child_index);
		}

		return final_result;
	}
};