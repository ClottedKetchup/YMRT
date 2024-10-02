#include "MathCommon.h"

namespace YumeRT {
	struct TransformState
	{
		// TODO: scene hierarchy.
		glm::vec3 translate_xyz;
		glm::vec3 scale_xyz;
		glm::vec3 rotate_xyz; // should turn into radians
		
		int parent_transform_index = EMPTY_UINT32;
		int valid;
		int padding;

		inline TransformState(const glm::vec3 &translate, const glm::vec3& scale, const glm::vec3& rotate, int parent_index = EMPTY_UINT32): 
			translate_xyz(translate), scale_xyz(scale), rotate_xyz(rotate), parent_transform_index(parent_index), valid(false) {
				
		}

		inline void SetValid() {
			valid = true;
		}

		inline void SetInvalid() {
			valid = false;
		}

		inline bool Valid() {
			return valid;
		}

		// this function have to specify a pivot.
		inline glm::mat4 GetTransformMatrix(const glm::vec3 &pivot)
		{
			auto matrix = Matrix_T(translate_xyz) *
				Matrix_T(pivot) *
				Matrix_R(glm::vec3(0.0f, 0.0f, 1.0f), glm::radians(rotate_xyz.z)) *
				Matrix_R(glm::vec3(0.0f, 1.0f, 0.0f), glm::radians(rotate_xyz.y)) *
				Matrix_R(glm::vec3(1.0f, 0.0f, 0.0f), glm::radians(rotate_xyz.x)) *
				Matrix_S(scale_xyz) *
				Matrix_T(-pivot);

			return matrix;
		}

		// always scale and rotate around origin.
		inline glm::mat4 GetTransformMatrix(std::vector<TransformState> &transform_states, std::vector<glm::mat4>& transforms, std::vector<glm::mat4>& i_transforms) const
		{
			assert(transform_states.size() == transforms.size());
			
			auto matrix = Matrix_T(translate_xyz) *
				Matrix_R(glm::vec3(0.0f, 0.0f, 1.0f), glm::radians(rotate_xyz.z)) *
				Matrix_R(glm::vec3(0.0f, 1.0f, 0.0f), glm::radians(rotate_xyz.y)) *
				Matrix_R(glm::vec3(1.0f, 0.0f, 0.0f), glm::radians(rotate_xyz.x)) *
				Matrix_S(scale_xyz);

			auto parent_index = parent_transform_index;
			while (parent_index != EMPTY_UINT32) {
				const auto& parent_transform_state = transform_states[parent_index];
				assert(parent_transform_state.valid);
				const auto& parent_matrix = transforms[parent_index];
				matrix = parent_matrix * matrix;

				parent_index = parent_transform_state.parent_transform_index;
			}
			return matrix;
		}
	};
	

};