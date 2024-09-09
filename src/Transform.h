#include "MathCommon.h"

namespace YumeRT {
	struct TransformState
	{
		// TODO: scene hierarchy.
		glm::vec3 translate_xyz;
		glm::vec3 scale_xyz;
		glm::vec3 rotate_xyz; // should turn into radians

		inline TransformState() { 
			SetInvalid(); 
		}

		// this function have to specify a pivot.
		inline glm::mat4 GetTransformMatrix(const glm::vec3 &pivot)
		{
			auto matrix = Matrix_T(pivot) *
				Matrix_R(glm::vec3(0.0f, 0.0f, 1.0f), glm::radians(rotate_xyz.z)) *
				Matrix_R(glm::vec3(0.0f, 1.0f, 0.0f), glm::radians(rotate_xyz.y)) *
				Matrix_R(glm::vec3(1.0f, 0.0f, 0.0f), glm::radians(rotate_xyz.x)) *
				Matrix_S(scale_xyz) *
				Matrix_T(-pivot);

			matrix[3][0] = translate_xyz.x;
			matrix[3][1] = translate_xyz.y;
			matrix[3][2] = translate_xyz.z;
			matrix[3][3] = 1.0f;
			return matrix;
		}

		// always scale and rotate around origin.
		inline glm::mat4 GetTransformMatrix()
		{
			auto matrix = Matrix_R(glm::vec3(0.0f, 0.0f, 1.0f), glm::radians(rotate_xyz.z)) *
				Matrix_R(glm::vec3(0.0f, 1.0f, 0.0f), glm::radians(rotate_xyz.y)) *
				Matrix_R(glm::vec3(1.0f, 0.0f, 0.0f), glm::radians(rotate_xyz.x)) *
				Matrix_S(scale_xyz);

			matrix[3][0] = translate_xyz.x;
			matrix[3][1] = translate_xyz.y;
			matrix[3][2] = translate_xyz.z;
			matrix[3][3] = 1.0f;
			return matrix;
		}

		inline void SetInvalid() { 
			auto set_float_bits = [](float &f) {
				uint32_t &ui = *((uint32_t*)(&f));
				ui = 0xFFFFFFFF;
			};
			set_float_bits(scale_xyz.x);
			set_float_bits(scale_xyz.y);
			set_float_bits(scale_xyz.z);
		}

		inline bool Invalid() { 
			auto check_float_bits = [](float &f) {
				uint32_t &ui = *((uint32_t*)(&f));
				return ui == 0xFFFFFFFF;
			};
			return check_float_bits(scale_xyz.x) && check_float_bits(scale_xyz.y) && check_float_bits(scale_xyz.z);
		}
	};
	

};