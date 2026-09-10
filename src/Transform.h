#include "MathCommon.h"

namespace YMRT {
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

		inline bool IsValid() const {
			return valid;
		}

		// this function have to specify a pivot.
	};
	
	// always scale and rotate around origin.
	inline void InitTransformMatrix(std::vector<TransformState>& transform_states, std::vector<glm::mat4>& transforms, std::vector<glm::mat4>& i_transforms, const uint32_t current_transform_index)
	{
		assert(transform_states.size() == transforms.size());
		assert(current_transform_index < transform_states.size());

		auto& current_transform_state = transform_states.at(current_transform_index);

		auto matrix = Matrix_T(current_transform_state.translate_xyz) *
			Matrix_R(glm::vec3(0.0f, 0.0f, 1.0f), glm::radians(current_transform_state.rotate_xyz.z)) *
			Matrix_R(glm::vec3(0.0f, 1.0f, 0.0f), glm::radians(current_transform_state.rotate_xyz.y)) *
			Matrix_R(glm::vec3(1.0f, 0.0f, 0.0f), glm::radians(current_transform_state.rotate_xyz.x)) *
			Matrix_S(current_transform_state.scale_xyz);

		if ((uint32_t)current_transform_state.parent_transform_index < transform_states.size())
		{
			const auto& parent_transform_state = transform_states[current_transform_state.parent_transform_index];
			if (!parent_transform_state.IsValid()) {
				InitTransformMatrix(transform_states, transforms, i_transforms, current_transform_state.parent_transform_index);
			}
			const auto& parent_matrix = transforms[current_transform_state.parent_transform_index];
			matrix = parent_matrix * matrix;
		}

		transforms.at(current_transform_index) = matrix;
		i_transforms.at(current_transform_index) = glm::inverse(matrix);
		transform_states.at(current_transform_index).SetValid();
	}
};