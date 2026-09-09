#include "DskParser.h"

#include <chrono>

namespace YumeRT
{
	static const std::string dsk_file_postfix = ".dsktmp";

	enum Statement_Type {
		Illegal = 0,
		Type_Declaration = 1,
		Property_Setting = 2
	};

	void issue_an_error(const StatementToken& statement, const std::string& error) 
	{
		std::cerr << error << " at line " << statement.line << std::endl;
	};

	std::filesystem::path get_texture_folder_from_scene_file(const std::filesystem::path& scene_file_dir)
	{
		std::filesystem::path file_name = scene_file_dir.filename();

		std::filesystem::path file_stem = file_name.stem();
		std::filesystem::path file_extension = file_name.extension();

		std::string folder_name = "texture_" + file_stem.string() + "_" + file_extension.string().substr(1);

		return scene_file_dir.parent_path() / std::filesystem::path(folder_name);
	}

	bool is_valid_bracket(const char c) 
	{
		return c == '<' || c == '>' || c == '(' || c == ')';
	}

	bool is_valid_bracket(const std::string& s) 
	{
		return !s.empty() && (s == "Begin" || s == "End");
	}

	bool is_begin_bracket(const std::string& s) 
	{
		return !s.empty() && s == "Begin";
	}

	bool is_end_bracket(const std::string& s)
	{
		return !s.empty() && s == "End";
	}

	bool is_valid_character(const char c) 
	{
		return std::isalpha(c) || std::isdigit(c) || c == '=' || c == '<' || c == '>' || c == ',' || c == '.' || c == '\"' || c == '(' || c == ')' || c == '#' || c == '_' || c == ';' || c == '[' || c == ']' || c == '+' || c == '-' || c == '\\' || c == '/' || c == ':';
	}

	bool is_valid_data_type(const std::string &data_type)
	{
		static const std::unordered_set<std::string> data_type_set = 
		{
			"Camera",

			"Transform",

			"Mesh",
			"Cube",
			"Sphere",

			"PrimitiveInstance",

			"DefaultMaterial",
			"LightMaterial",

			"ImageTexture",
			"ConstantTextureFloat",
			"ConstantTextureRgb",
			"CheckerBoardTexture",
			"NoiseTexture",
			"NoiseTextureFBM",
			"NoiseTextureTurbulence",
			"NoiseTextureMarble",
			"NoiseTextureWood",
			"NoiseTexturePolkaDot",
			"NoiseTextureWave",

			"DistantLight",
			
			"Volume"
		};

		return !data_type.empty() && data_type_set.find(data_type) != data_type_set.end();
	}

	bool is_camera_attribute(const std::string& s) 
	{
		static const std::unordered_set<std::string> camera_attribute_set =
		{
			"from", "look", "fov"
		};
		return !s.empty() && camera_attribute_set.find(s) != camera_attribute_set.end();
	}

	bool is_transform_attribute(const std::string& s) 
	{
		static const std::unordered_set<std::string> transform_attribute_set =
		{
			"name", "translate", "scale", "rotate", "parent_transform_index"
		};
		return !s.empty() && transform_attribute_set.find(s) != transform_attribute_set.end();
	}

	bool is_mesh_attribute(const std::string& s) 
	{
		static const std::unordered_set<std::string> mesh_attribute_set =
		{
			"name", 
			"mesh_triangles",
			"mesh_position_indices",
			"mesh_positions",
			"mesh_normal_indices",
			"mesh_normals",
			"mesh_texcoord_indices",
			"mesh_texcoords"
		};
		return !s.empty() && mesh_attribute_set.find(s) != mesh_attribute_set.end();
	}

	bool is_cube_attribute(const std::string& s)
	{
		return !s.empty() && s == "name";
	}

	bool is_sphere_attribute(const std::string& s)
	{
		return !s.empty() && (s == "name" || s == "radius");
	}

	bool is_primitive_instance_attribute(const std::string& s)
	{
		static const std::unordered_set<std::string> primitive_instance_attribute_set =
		{
			"name",
			"geometry_index",
			"transform_index",
			"material_index",
			"inner_volume_index",
			"treat_as_boundary"
		};
		return !s.empty() && primitive_instance_attribute_set.find(s) != primitive_instance_attribute_set.end();
	}

	bool is_default_material_attribute(const std::string& s)
	{
		static const std::unordered_set<std::string> default_material_attribute_set =
		{
			"name",
			"diffuse_albedo",
			"specular_albedo",
			"roughness_x", 
			"roughness_y",
			"ior_n",
			"metalness",
			"specular_weight",
			"transmission_weight",
			
			"ior_priority",

			"coat_albedo",
			"coat_weight",
			"coat_thickness",
			"coat_ior",
			"coat_roughness_x",
			"coat_roughness_y",

			"diffuse_albedo_tex",
			"alpha_x_tex",
			"specular_albedo_tex",
			"alpha_y_tex",
			"specular_weight_tex",
			"metalness_tex",
			"transmission_weight_tex",
			"normal_mapping_tex",
			"bump_mapping_tex",

			"coat_albedo_tex",
			"coat_weight_tex",
			"coat_thickness_tex",
			"coat_roughness_x_tex",
			"coat_roughness_y_tex",

			"coat_normal_tex",
			"coat_normal_tex_type"
		};
		return !s.empty() && default_material_attribute_set.find(s) != default_material_attribute_set.end();
	}

	bool is_light_material_attribute(const std::string& s) 
	{
		static const std::unordered_set<std::string> light_material_attribute_set =
		{
			"name", "light_color", "intensity"
		};
		return !s.empty() && light_material_attribute_set.find(s) != light_material_attribute_set.end();
	}

	bool is_image_texture_attribute(const std::string& s)
	{
		return !s.empty() && (s == "name" || s == "texture_file_name");
	}

	bool is_constant_texture_float_attribute(const std::string& s)
	{
		return !s.empty() && (s == "name" || s == "value");
	}

	bool is_constant_texture_rgb_attribute(const std::string& s)
	{
		return !s.empty() && (s == "name" ||  s == "color");
	}

	bool is_checker_texture_attribute(const std::string& s) 
	{
		static const std::unordered_set<std::string> checker_texture_attribute_set = 
		{
			"name",
			"texture_black_idx", 
			"texture_white_idx",
			"frequency"
		};
		return !s.empty() && checker_texture_attribute_set.find(s) != checker_texture_attribute_set.end();
	}

	bool is_noise_texture_attribute(const std::string& s)
	{
		static const std::unordered_set<std::string> noise_texture_attribute_set =
		{
			"name",
			"texture_black_idx",
			"texture_white_idx",
			"frequency",
			"normalized"
		};
		return !s.empty() && noise_texture_attribute_set.find(s) != noise_texture_attribute_set.end();
	}

	bool is_noise_texture_fbm_attribute(const std::string& s)
	{
		static const std::unordered_set<std::string> noise_texture_fbm_attribute_set =
		{
			"name",
			"texture_black_idx",
			"texture_white_idx",
			"frequency",
			"lacunarity",
			"gain",
			"layer_count",
			"amplitude",
			"offset"
		};
		return !s.empty() && noise_texture_fbm_attribute_set.find(s) != noise_texture_fbm_attribute_set.end();
	}

	bool is_noise_texture_turbulence_attribute(const std::string& s)
	{
		static const std::unordered_set<std::string> noise_texture_turbulence_attribute_set =
		{
			"name",
			"texture_black_idx",
			"texture_white_idx",
			"frequency",
			"lacunarity",
			"gain",
			"layer_count",
			"amplitude",
			"offset"
		};
		return !s.empty() && noise_texture_turbulence_attribute_set.find(s) != noise_texture_turbulence_attribute_set.end();
	}

	bool is_noise_texture_marble_attribute(const std::string& s)
	{
		static const std::unordered_set<std::string> noise_texture_marble_attribute_set =
		{
			"name",
			"texture_black_idx",
			"texture_white_idx",
			"frequency",
			"lacunarity",
			"gain",
			"layer_count",
			"variation",
			"x_turb",
			"y_turb",
			"z_turb"
		};
		return !s.empty() && noise_texture_marble_attribute_set.find(s) != noise_texture_marble_attribute_set.end();
	}

	bool is_noise_texture_wood_attribute(const std::string& s)
	{
		static const std::unordered_set<std::string> noise_texture_wood_attribute_set =
		{
			"name",
			"texture_black_idx",
			"texture_white_idx",
			"frequency",
			"variation"
		};
		return !s.empty() && noise_texture_wood_attribute_set.find(s) != noise_texture_wood_attribute_set.end();
	}

	bool is_noise_texture_polka_dot_attribute(const std::string& s)
	{
		static const std::unordered_set<std::string> noise_texture_polka_dot_attribute_set =
		{
			"name",
			"texture_black_idx", 
			"texture_white_idx",
			"frequency",
			"radius"
		};
		return !s.empty() && noise_texture_polka_dot_attribute_set.find(s) != noise_texture_polka_dot_attribute_set.end();
	}

	bool is_noise_texture_wave_attribute(const std::string& s)
	{
		static const std::unordered_set<std::string> noise_texture_wave_attribute_set =
		{
			"name",
			"texture_black_idx",
			"texture_white_idx",
			"freq_0",
			"lacunarity_0",
			"gain_0",
			"layer_count_0",
			"freq_1",
			"lacunarity_1",
			"gain_1",
			"layer_count_1"
		};
		return !s.empty() && noise_texture_wave_attribute_set.find(s) != noise_texture_wave_attribute_set.end();
	}

	bool is_distant_light_attribute(const std::string& s)
	{
		static const std::unordered_set<std::string> distant_light_attribute_set = 
		{
			"name", 
			"light_color", 
			"transform_index",
			"intensity", 
			"theta_max"
		};

		return !s.empty() && distant_light_attribute_set.find(s) != distant_light_attribute_set.end();
	}

	bool is_volume_attribute(const std::string& s)
	{
		static const std::unordered_set<std::string> volume_attribute_set =
		{
			"name", 
			"transform_index",
			"sigma_t",
			"albedo",
			"g",
			"density_texture_index"
		};

		return !s.empty() && volume_attribute_set.find(s) != volume_attribute_set.end();
	}

	bool check_attribute_exist(const std::string& current_basic_type, const std::string& attribute_name) 
	{
		using CHECK_FUNC = bool (*)(const std::string& s);
		static const std::unordered_map<std::string, CHECK_FUNC> type_check_function_map = 
		{
			{ "Camera", is_camera_attribute },

			{ "Transform", is_transform_attribute },

			{ "Mesh", is_mesh_attribute },
			{ "Cube", is_cube_attribute },
			{ "Sphere", is_sphere_attribute },

			{ "PrimitiveInstance", is_primitive_instance_attribute },

			{ "DefaultMaterial", is_default_material_attribute },
			{ "LightMaterial", is_light_material_attribute },

			{ "ImageTexture", is_image_texture_attribute },
			{ "ConstantTextureFloat", is_constant_texture_float_attribute },
			{ "ConstantTextureRgb", is_constant_texture_rgb_attribute },
			{ "CheckerBoardTexture", is_checker_texture_attribute },
			{ "NoiseTexture", is_noise_texture_attribute },
			{ "NoiseTextureFBM", is_noise_texture_fbm_attribute },
			{ "NoiseTextureTurbulence", is_noise_texture_turbulence_attribute },
			{ "NoiseTextureMarble", is_noise_texture_marble_attribute },
			{ "NoiseTextureWood", is_noise_texture_wood_attribute },
			{ "NoiseTexturePolkaDot", is_noise_texture_polka_dot_attribute },
			{ "NoiseTextureWave", is_noise_texture_wave_attribute },

			{ "DistantLight", is_distant_light_attribute },

			{ "Volume", is_volume_attribute }
		};

		if (current_basic_type.empty() || attribute_name.empty()) {
			return false;
		}

		auto iter = type_check_function_map.find(current_basic_type);
		if (iter == type_check_function_map.end()) {
			return false;
		}

		return (*iter).second(attribute_name);
	}

	bool is_numeric_int(const std::string& value) 
	{
		if (value.empty()) {
			return false;
		}

		if (value.size() > 1 && value.front() == '0') {
			return false;
		}

		for (int i = 0; i < (int)value.size(); ++i) {
			if (!std::isdigit(value[i])) {
				return false;
			}
		}

		return true;
	}

	bool is_numeric_float(const std::string &value) 
	{
		if (value.empty()) {
			return false;
		}

		const size_t dot_pos = value.find('.');
		if (dot_pos == std::string::npos) {
			int digit_count = 0;
			for (int i = 0; i < (int)value.size(); ++i) {
				const bool is_digit = std::isdigit(value[i]);
				if (is_digit) {
					++digit_count;
				}

				if (i == 0 && !is_digit && value[i] != '+' && value[i] != '-') {
					return false;
				}
				if (i > 0 && !is_digit) {
					return false;
				}
			}

			if (digit_count == 0) {
				return false;
			}

			return true;
		}
		else {
			auto left_part_valid = [](const std::string& left_value, int &left_digit_count) {
				if (left_value.empty()) {
					return true;
				}

				left_digit_count = 0;
				for (int i = 0; i < (int)left_value.size(); ++i) {
					const bool is_digit = std::isdigit(left_value[i]);
					if (is_digit) {
						++left_digit_count;
					}

					if (i == 0 && !is_digit && left_value[i] != '+' && left_value[i] != '-') {
						return false;
					}
					if (i > 0 && !is_digit) {
						return false;
					}
				}
				return true;
			};
			auto right_part_valid = [](const std::string& right_value, int& right_digit_count) {
				if (right_value.empty()) {
					return true;
				}

				right_digit_count = 0;
				for (int i = 0; i < (int)right_value.size(); ++i) {
					const bool is_digit = std::isdigit(right_value[i]);
					if (is_digit) {
						++right_digit_count;
					}

					if (i == (int)right_value.size() - 1 && !is_digit && right_value[i] != 'f') {
						return false;
					}
					if (i < (int)right_value.size() - 1 && !is_digit) {
						return false;
					}
				}
				return true;
			};

			const std::string left_value(value.begin(), value.begin() + dot_pos);
			const std::string right_value(value.begin() + dot_pos + 1, value.end());
			if (left_value.empty() && right_value.empty()) {
				return false;
			}

			int left_digit_count = 0, right_digit_count = 0;
			if (!left_part_valid(left_value, left_digit_count) || !right_part_valid(right_value, right_digit_count)) {
				return false;
			}

			if (left_digit_count + right_digit_count == 0) {
				return false;
			}

			return true;
		}
	}

	bool is_numeric_vector(const std::string& value) 
	{
		if (value.size() < 2) {
			return false;
		}

		if (!(value.front() == '<' && value.back() == '>')) {
			return false;
		}

		int float_count = 0;
		int start = 1, end = 1;
		for (; start < (int)value.size() &&  end < (int)value.size(); ++end) {
			if (value[end] != ',' && value[end] != '>') {
				continue;
			}
			const std::string numeric_float(value.begin() + start, value.begin() + end);
			if (!is_numeric_float(numeric_float)) {
				return false;
			}
			++float_count;
			start = end + 1;
		}

		if (float_count != 3) {
			return false;
		}

		return true;
	}

	bool is_numeric_boolean(const std::string& value)
	{
		return !value.empty() && (value == "true" || value == "false");
	}

	bool is_var_name(const std::string& value) 
	{
		if (value.size() < 2) {
			return false;
		}

		if (!(value.front() == '\"' && value.back() == '\"')) {
			return false;
		}

		for (int i = 1; i < (int)value.size() - 1; ++i) {
			if (!(std::isalpha(value[i]) || std::isdigit(value[i]) || value[i] == '_' || value[i] == '.')) {
				return false;
			}
		}

		return true;
	}

	bool is_file_path(const std::string& value)
	{
		if (value.size() < 2) {
			return false;
		}

		if (!(value.front() == '\"' && value.back() == '\"')) {
			return false;
		}

		for (int i = 1; i < (int)value.size() - 1; ++i) {
			if (!(std::isalpha(value[i]) || std::isdigit(value[i]) || value[i] == '_' || value[i] == '.' || value[i] == '\\' || value[i] == '/' || value[i] == ':')) {
				return false;
			}
		}

		return true;
	}

	bool is_data_array(const std::string& value)
	{
		if (value.size() < 2) {
			return false;
		}

		if (!(value.front() == '[' && value.back() == ']')) {
			return false;
		}

		int start = 1, end = 1;
		for (; start < (int)value.size() && end < (int)value.size(); ++end) {
			if (value[end] != ',' && value[end] != ']') {
				continue;
			}
			const std::string numeric_value(value.begin() + start, value.begin() + end);
			if (!(is_numeric_float(numeric_value) || is_numeric_int(numeric_value))) {
				return false;
			}
			start = end + 1;
		}

		return true;
	}

	bool is_type_bracket(const std::string& statement, const size_t line) 
	{
		assert(!statement.empty());
		size_t left_bracket = std::string::npos;
		for (int i = 0; i < (int)statement.size(); ++i) {
			if (statement.at(i) == '(') {
				left_bracket = i;
				break;
			}
		}

		size_t right_bracket = std::string::npos;
		for (int j = (int)statement.size() - 1; j >= 0; --j) {
			if (statement.at(j) == ')') {
				right_bracket = j;
				break;
			}
		}

		if (left_bracket == std::string::npos || right_bracket == std::string::npos || left_bracket >= right_bracket) {
			return false;
		}
		if (right_bracket != statement.size() - 1) {
			return false;
		}

		const std::string data_type(statement.begin() + left_bracket + 1, statement.begin() + right_bracket);
		if (!is_valid_data_type(data_type)) {
			return false;
		}

		const std::string bracket_begin_end(statement.begin(), statement.begin() + left_bracket);
		if (!is_valid_bracket(bracket_begin_end)) {
			return false;
		}

		return true;
	}

	StatementToken split_type_bracket(const std::string& statement, const size_t line) 
	{
		assert(!statement.empty());
		const size_t left_bracket = statement.find('(');
		const size_t right_bracket = statement.find(')');
		assert(left_bracket != std::string::npos && right_bracket != std::string::npos);

		assert(left_bracket < right_bracket);
		assert(right_bracket == statement.size() - 1);

		const std::string data_type(statement.begin() + left_bracket + 1, statement.begin() + right_bracket);
		const std::string bracket_begin_end(statement.begin(), statement.begin() + left_bracket);

		return StatementToken(STATEMENT_TYPE::TYPE_BRACKET, line, bracket_begin_end, data_type);
	}

	bool is_attribute_assign(const std::string& statement, const size_t line) 
	{
		assert(!statement.empty());
		size_t equal_symbol = std::string::npos;
		for (int i = 0; i < (int)statement.size(); ++i) {
			if (statement.at(i) == '=') {
				equal_symbol = i;
				break;
			}
		}

		if (equal_symbol == std::string::npos) {
			return false;
		}

		const std::string attribute(statement.begin(), statement.begin() + equal_symbol);
		for (int i = 0; i < (int)attribute.size(); ++i) {
			if (!(std::isalpha(attribute.at(i)) || attribute.at(i) == '_')) {
				return false;
			}
		}

		const std::string value(statement.begin() + equal_symbol + 1, statement.end());
		if (!(is_numeric_float(value) || 
			is_numeric_vector(value) ||
			is_var_name(value) || 
			is_file_path(value) ||
			is_numeric_boolean(value) || 
			is_numeric_int(value) || 
			is_data_array(value))) 
		{
			return false;
		}

		return true;
	}

	StatementToken split_attribute_assign(const std::string& statement, const size_t line)
	{
		assert(!statement.empty());
		const size_t equal_symbol = statement.find('=');
		assert(equal_symbol != std::string::npos);

		const std::string attribute(statement.begin(), statement.begin() + equal_symbol);
		assert(!attribute.empty());
		const std::string value(statement.begin() + equal_symbol + 1, statement.end());
		assert(!value.empty());

		return StatementToken(STATEMENT_TYPE::ATTRIBUTE_ASSIGN, line, attribute, value);
	}

	bool is_valid_statement(const std::string& statement, const size_t line, STATEMENT_TYPE &statement_type)
	{
		if (is_type_bracket(statement, line)) {
			statement_type = STATEMENT_TYPE::TYPE_BRACKET;
			return true;
		}

		if (is_attribute_assign(statement, line)) {
			statement_type = STATEMENT_TYPE::ATTRIBUTE_ASSIGN;
			return true;
		}

		return false;
	}

	StatementToken extract_statement_token(const std::string& statement, const size_t line, const STATEMENT_TYPE statement_type)
	{
		if (statement_type == STATEMENT_TYPE::TYPE_BRACKET) {
			return split_type_bracket(statement, line);
		}
		else if (statement_type == STATEMENT_TYPE::ATTRIBUTE_ASSIGN) {
			return split_attribute_assign(statement, line);
		}
		else {
			assert(0);
			return StatementToken(STATEMENT_TYPE::NONE, line, "", "");
		}
	}

	bool check_statements_valid(const std::vector<StatementToken> &statement_tokens)
	{
		std::vector<StatementToken> state_stack;

		for (size_t line_index = 0; line_index < statement_tokens.size(); ++line_index)
		{
			const StatementToken& statement = statement_tokens.at(line_index);

			if (statement.type == STATEMENT_TYPE::TYPE_BRACKET) {
				if (is_begin_bracket(statement.left_line)) {
					if (!state_stack.empty()) {
						issue_an_error(statement, "Error: recursive statement is not supported!");
						return false;
					}
					state_stack.push_back(statement);
				}
				else if (is_end_bracket(statement.left_line)) {
					if (state_stack.empty() || state_stack.back().right_line != statement.right_line) {
						issue_an_error(statement, "Error:bracket not match!");
						return false;
					}
					state_stack.pop_back();
				}
				else {
					issue_an_error(statement, "Error:illegal token!");
					return false;
				}
			}
			else if (statement.type == STATEMENT_TYPE::ATTRIBUTE_ASSIGN) {
				if (state_stack.empty()) {
					issue_an_error(statement, "Error:type statement is required before specifying some value!");
					return false;
				}
				if (!check_attribute_exist(state_stack.back().right_line, statement.left_line)) {
					issue_an_error(statement, "Error:unknown attribute of current basic type!");
					return false;
				}
			}
			else{
				return false;
			}
		}
		
		return true;
	}

	void split_file_content(std::vector<std::string>& texts, const std::string& file_content) 
	{
		if (file_content.empty()) {
			return;
		}
		
		size_t start = 0, end = 0;
		for (; end < file_content.size() && start < file_content.size(); ++end) {
			if (file_content.at(end) != ';') {
				continue;
			}
			const std::string statement(file_content.begin() + start, file_content.begin() + end);
			start = end + 1;
			if (!statement.empty()) {
				texts.push_back(statement);
			}
		}

		if (start < file_content.size()) {
			const std::string statement(file_content.begin() + start, file_content.end());
			if (!statement.empty()) {
				texts.push_back(statement);
			}
		}
	}

	bool attribute_get_vector(const std::string &value, glm::vec3 &v) 
	{
		assert(value.front() == '<' && value.back() == '>');
		int float_count = 0;
		int start = 1, end = 1;
		for (; start < (int)value.size() && end < (int)value.size(); ++end) {
			if (value[end] != ',' && value[end] != '>') {
				continue;
			}
			const std::string numeric_float(value.begin() + start, value.begin() + end);
			assert(float_count < 3);
			v[float_count++] = std::stof(numeric_float);
			start = end + 1;
		}

		return true;
	}

	bool attribute_get_boolean(const std::string& value, bool& boolean)
	{
		boolean = (value == "true");
		return true;
	}

	bool attribute_get_float(const std::string& value, float& f)
	{
		f = std::stof(value);
		return true;
	}

	bool attribute_get_int(const std::string& value, int& i)
	{
		i = std::stoi(value);
		return true;
	}

	bool attribute_get_string(const std::string& value, std::string& str)
	{
		assert(value.front() == '\"' && value.back() == '\"');
		str = std::string(value.begin() + 1, value.end() - 1);
		return true;
	}

	bool attribute_get_name(const std::unordered_map<std::string, uint32_t> &name_to_index_map, const StatementToken& statement_token, const std::string& value, std::string& str)
	{
		std::string name;
		attribute_get_string(value, name);

		if (name_to_index_map.find(name) != name_to_index_map.end()) {
			issue_an_error(statement_token, "Error: this basic type's name is already declared!");
			return false;
		}

		str = name;
		return true;
	}

	bool attribute_get_index_from_name(const std::unordered_map<std::string, uint32_t>& name_to_index_map, const StatementToken& statement_token, const std::string& value, uint32_t &i)
	{
		std::string name;
		attribute_get_string(value, name);

		auto iter = name_to_index_map.find(name);
		if (iter == name_to_index_map.end()) {
			issue_an_error(statement_token, "Error: Undeclared variable!");
			return false;
		}
		else {
			i = (*iter).second;
			return true;
		}
	}

	bool attribute_get_int_array(const std::string& value, std::vector<uint32_t> &arr)
	{
		assert(value.front() == '[' && value.back() == ']');

		int start = 1, end = 1;
		for (; start < (int)value.size() && end < (int)value.size(); ++end) {
			if (value[end] != ',' && value[end] != ']') {
				continue;
			}
			const std::string numeric_int(value.begin() + start, value.begin() + end);
			arr.push_back(std::stoi(numeric_int));
			start = end + 1;
		}

		return true;
	}

	bool attribute_get_float_array(const std::string& value, std::vector<float>& arr)
	{
		assert(value.front() == '[' && value.back() == ']');

		int start = 1, end = 1;
		for (; start < (int)value.size() && end < (int)value.size(); ++end) {
			if (value[end] != ',' && value[end] != ']') {
				continue;
			}
			const std::string numeric_float(value.begin() + start, value.begin() + end);
			arr.push_back(std::stof(numeric_float));
			start = end + 1;
		}

		return true;
	}

	bool attribute_get_triangle_array(const std::string& value, std::vector<Triangle>& arr)
	{
		std::vector<uint32_t> temp_arr;
		if (!attribute_get_int_array(value, temp_arr)) {
			return false;
		}
		
		for (size_t index = 0; index < temp_arr.size(); index += 3) {
			Triangle triangle;
			triangle.id0 = temp_arr.at(index);
			triangle.id1 = temp_arr.at(index + 1);
			triangle.id2 = temp_arr.at(index + 2);
			arr.push_back(triangle);
		}

		return false;
	}
	

	void read_camera_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		glm::vec3 from = glm::vec3(0.0f, 0.0f, 10.0f);
		glm::vec3 look = glm::vec3(0.0f, 0.0f, 4.0f);
		float fov = 45.0f;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index) 
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "from") {
				attribute_get_vector(right_value, from);
			}
			else if (attribute == "look") {
				attribute_get_vector(right_value, look);
			}
			else if (attribute == "fov") {
				attribute_get_float(right_value, fov);
				fov = glm::max(fov, 0.01f);
			}
			else {
				assert(0);
			}
		}

		auto& camera = scene_asset_manager.GetSceneCamera();
		camera.SetFov(glm::radians(fov));
		camera.SetPosition(from);
		camera.SetDir(from - look);
	}

	void read_transform_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		glm::vec3 translate(0.0, 0.0, 0.0);
		glm::vec3 scale(1.0, 1.0, 1.0);
		glm::vec3 rotate(0.0, 0.0, 0.0);
		uint32_t parent_transform_index = EMPTY_UINT32;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "translate") {
				attribute_get_vector(right_value, translate);
			}
			else if (attribute == "scale") {
				attribute_get_vector(right_value, scale);
				scale = glm::max(glm::vec3(0.0001f), scale);
			}
			else if (attribute == "rotate") {
				attribute_get_vector(right_value, rotate);
				rotate = glm::clamp(rotate, glm::vec3(0.0f), glm::vec3(360.0f));
			}
			else if (attribute == "parent_transform_index") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, parent_transform_index)) {
					return;
				}
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: transform type require an name to specify!");
			return;
		}

		const uint32_t transform_index = scene_asset_manager.CreateTransform(name, translate, scale, rotate, parent_transform_index);
		name_to_index_map[name] = transform_index;
	}

	void read_mesh_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		std::vector<Triangle> mesh_triangles;
		std::vector<uint32_t>	mesh_position_indices;
		std::vector<float> mesh_positions;
		std::vector < uint32_t> mesh_normal_indices;
		std::vector<float> mesh_normals;
		std::vector < uint32_t> mesh_texcoord_indices;
		std::vector<float> mesh_texcoords;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "mesh_triangles") {
				attribute_get_triangle_array(right_value, mesh_triangles);
			}
			else if (attribute == "mesh_position_indices") {
				attribute_get_int_array(right_value, mesh_position_indices);
			}
			else if (attribute == "mesh_positions") {
				attribute_get_float_array(right_value, mesh_positions);
			}
			else if (attribute == "mesh_normal_indices") {
				attribute_get_int_array(right_value, mesh_normal_indices);
			}
			else if (attribute == "mesh_normals") {
				attribute_get_float_array(right_value, mesh_normals);
			}
			else if (attribute == "mesh_texcoord_indices") {
				attribute_get_int_array(right_value, mesh_texcoord_indices);
			}
			else if (attribute == "mesh_texcoords") {
				attribute_get_float_array(right_value, mesh_texcoords);
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: mesh type require an name to specify!");
			return;
		}
		if (mesh_position_indices.size() != mesh_triangles.size() * 3) {
			issue_an_error(statement_tokens.at(start), "Error: mesh's position indices number is wrong!");
			return;
		}
		if (mesh_normal_indices.size() != mesh_triangles.size() * 3) {
			issue_an_error(statement_tokens.at(start), "Error: mesh's normal indices number is wrong!");
			return;
		}
		if (!mesh_texcoord_indices.empty() && mesh_texcoord_indices.size() != mesh_triangles.size() * 3) {
			issue_an_error(statement_tokens.at(start), "Error: mesh's texcoord indices number is wrong!");
			return;
		}
		if (mesh_normals.empty() || mesh_positions.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: mesh require position and normal array!");
			return;
		}

		const uint32_t mesh_index = scene_asset_manager.CreateMesh(name, mesh_triangles, mesh_position_indices, mesh_positions, mesh_normal_indices, mesh_normals, mesh_texcoord_indices, mesh_texcoords);
		name_to_index_map[name] = mesh_index;
	}

	void read_cube_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: cube type require an name to specify!");
			return;
		}

		const uint32_t cube_index = scene_asset_manager.CreateCube(name);
		name_to_index_map[name] = cube_index;
	}

	void read_sphere_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		float radius = 1.0f;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "radius") {
				attribute_get_float(right_value, radius);
				radius = glm::max(0.0001f, radius);
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: sphere type require an name to specify!");
			return;
		}

		const uint32_t sphere_index = scene_asset_manager.CreateSphere(name, radius);
		name_to_index_map[name] = sphere_index;
	}

	void read_primitive_instance_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		uint32_t geometry_index = EMPTY_UINT32;
		uint32_t transform_index = EMPTY_UINT32;
		uint32_t material_index = EMPTY_UINT32;
		uint32_t inner_volume_index = EMPTY_UINT32;
		bool treat_as_boundary = false;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "geometry_index") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, geometry_index)) {
					return;
				}
			}
			else if (attribute == "transform_index") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, transform_index)) {
					return;
				}
			}
			else if (attribute == "material_index") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, material_index)) {
					return;
				}
			}
			else if (attribute == "inner_volume_index") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, inner_volume_index)) {
					return;
				}
			}
			else if (attribute == "treat_as_boundary") {
				attribute_get_boolean(right_value, treat_as_boundary);
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: primitive instance type require an name to specify!");
			return;
		}
		if (geometry_index == EMPTY_UINT32) {
			issue_an_error(statement_tokens.at(start), "Error: primitive instance type require a geometry to specify!");
			return;
		}
		if (transform_index == EMPTY_UINT32) {
			issue_an_error(statement_tokens.at(start), "Error: primitive instance type require a transform to specify!");
			return;
		}

		const uint32_t prim_index = scene_asset_manager.CreatePrimitiveInstance(name, geometry_index, transform_index, material_index, inner_volume_index, treat_as_boundary);
		name_to_index_map[name] = prim_index;
	}

	void read_default_material_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		glm::vec3 diffuse_albedo = glm::vec3(0.5f);
		glm::vec3 specular_albedo = glm::vec3(1.0f);
		float roughness_x = 0.2f;  
		float roughness_y = 0.2f;
		float ior_n = 1.3f;
		float metalness = 0.0f;
		float specular_weight = 1.0f;
		float transmission_weight = 0.0f;
		uint32_t ior_priority = 0;

		glm::vec3 coat_albedo = glm::vec3(1.0f);
		float coat_weight = 0.0f;
		float coat_thickness = 1.0f;
		float coat_ior = 1.6f;
		float coat_roughness_x = 0.2f;
		float coat_roughness_y = 0.2f;

		uint32_t diffuse_albedo_tex = EMPTY_UINT32;
		uint32_t alpha_x_tex = EMPTY_UINT32;
		uint32_t specular_albedo_tex = EMPTY_UINT32;
		uint32_t alpha_y_tex = EMPTY_UINT32;
		uint32_t specular_weight_tex = EMPTY_UINT32;
		uint32_t metalness_tex = EMPTY_UINT32;
		uint32_t transmission_weight_tex = EMPTY_UINT32;
		uint32_t normal_mapping_tex = EMPTY_UINT32;
		uint32_t bump_mapping_tex = EMPTY_UINT32;

		uint32_t coat_albedo_tex = EMPTY_UINT32;
		uint32_t coat_weight_tex = EMPTY_UINT32;
		uint32_t coat_thickness_tex = EMPTY_UINT32;
		uint32_t coat_roughness_x_tex = EMPTY_UINT32;
		uint32_t coat_roughness_y_tex = EMPTY_UINT32;

		uint32_t coat_normal_tex = EMPTY_UINT32;
		uint32_t coat_normal_tex_type = COAT_NORMAL_TEXTURE_TYPE::BUMP_TEX;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;
		
			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "diffuse_albedo") {
				attribute_get_vector(right_value, diffuse_albedo);
				diffuse_albedo = glm::max(diffuse_albedo, glm::vec3(0.0f));
			}
			else if (attribute == "specular_albedo") {
				attribute_get_vector(right_value, specular_albedo);
				specular_albedo = glm::max(specular_albedo, glm::vec3(0.0f));
			}
			else if (attribute == "roughness_x") {
				attribute_get_float(right_value, roughness_x);
				roughness_x = glm::clamp(roughness_x, 0.0f, 1.0f);
			}
			else if (attribute == "roughness_y") {
				attribute_get_float(right_value, roughness_y);
				roughness_y = glm::clamp(roughness_y, 0.0f, 1.0f);
			}
			else if (attribute == "ior_n") {
				attribute_get_float(right_value, ior_n);
				ior_n = glm::max(0.0001f, ior_n);
			}
			else if (attribute == "metalness") {
				attribute_get_float(right_value, metalness);
				metalness = glm::clamp(metalness, 0.0f, 1.0f);
			}
			else if (attribute == "specular_weight") {
				attribute_get_float(right_value, specular_weight);
				specular_weight = glm::clamp(specular_weight, 0.0f, 1.0f);
			}
			else if (attribute == "transmission_weight") {
				attribute_get_float(right_value, transmission_weight);
				transmission_weight = glm::clamp(transmission_weight, 0.0f, 1.0f);
			}
			else if (attribute == "ior_priority") {
				int priority = 0;
				attribute_get_int(right_value, priority);
				ior_priority = glm::max(0, priority);
			}
			else if (attribute == "coat_albedo") {
				attribute_get_vector(right_value, coat_albedo);
				coat_albedo = glm::max(coat_albedo, glm::vec3(0.0f));
			}
			else if (attribute == "coat_weight") {
				attribute_get_float(right_value, coat_weight);
				coat_weight = glm::clamp(coat_weight, 0.0f, 1.0f);
			}
			else if (attribute == "coat_thickness") {
				attribute_get_float(right_value, coat_thickness);
				coat_thickness = glm::max(coat_thickness, 0.0f);
			}
			else if (attribute == "coat_ior") {
				attribute_get_float(right_value, coat_ior);
				coat_ior = glm::max(coat_ior, 0.0001f);
			}
			else if (attribute == "coat_roughness_x") {
				attribute_get_float(right_value, coat_roughness_x);
				coat_roughness_x = glm::clamp(coat_roughness_x, 0.0f, 1.0f);
			}
			else if (attribute == "coat_roughness_y") {
				attribute_get_float(right_value, coat_roughness_y);
				coat_roughness_y = glm::clamp(coat_roughness_y, 0.0f, 1.0f);
			}
			else if (attribute == "diffuse_albedo_tex") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, diffuse_albedo_tex)) {
					return;
				}
			}
			else if (attribute == "alpha_x_tex") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, alpha_x_tex)) {
					return;
				}
			}
			else if (attribute == "specular_albedo_tex") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, specular_albedo_tex)) {
					return;
				}
			}
			else if (attribute == "alpha_y_tex") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, alpha_y_tex)) {
					return;
				}
			}
			else if (attribute == "specular_weight_tex") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, specular_weight_tex)) {
					return;
				}
			}
			else if (attribute == "metalness_tex") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, metalness_tex)) {
					return;
				}
			}
			else if (attribute == "transmission_weight_tex") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, transmission_weight_tex)) {
					return;
				}
			}
			else if (attribute == "normal_mapping_tex") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, normal_mapping_tex)) {
					return;
				}
			}
			else if (attribute == "bump_mapping_tex") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, bump_mapping_tex)) {
					return;
				}
			}
			else if (attribute == "coat_albedo_tex") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, coat_albedo_tex)) {
					return;
				}
			}
			else if (attribute == "coat_weight_tex") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, coat_weight_tex)) {
					return;
				}
			}
			else if (attribute == "coat_thickness_tex") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, coat_thickness_tex)) {
					return;
				}
			}
			else if (attribute == "coat_roughness_x_tex") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, coat_roughness_x_tex)) {
					return;
				}
			}
			else if (attribute == "coat_roughness_y_tex") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, coat_roughness_y_tex)) {
					return;
				}
			}
			else if (attribute == "coat_normal_tex") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, coat_normal_tex)) {
					return;
				}
			}
			else if (attribute == "coat_normal_tex_type") {
				std::string tex_type;
				attribute_get_string(right_value, tex_type);
				if (tex_type == "normal_map") {
					coat_normal_tex_type = NORMAL_TEX;
				}
				else if (tex_type == "bump_map") {
					coat_normal_tex_type = BUMP_TEX;
				}
				else {
					issue_an_error(statement_token, "Error: invalid normal tex type!");
					return;
				}
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: default material type require an name to specify!");
			return;
		}

		const uint32_t material_index = scene_asset_manager.CreateDefaultMaterial(name, diffuse_albedo, specular_albedo, roughness_x, roughness_y, ior_n, metalness, specular_weight, transmission_weight, ior_priority,
			coat_albedo, coat_weight, coat_thickness, coat_ior, coat_roughness_x, coat_roughness_y,
			diffuse_albedo_tex, alpha_x_tex, specular_albedo_tex, alpha_y_tex, specular_weight_tex, metalness_tex, transmission_weight_tex, normal_mapping_tex, bump_mapping_tex,
			coat_albedo_tex, coat_weight_tex, coat_thickness_tex, coat_roughness_x_tex, coat_roughness_y_tex, coat_normal_tex, coat_normal_tex_type);
		name_to_index_map[name] = material_index;
	}

	void read_light_material_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		glm::vec3 light_color(1.0f);
		float intensity = 1.0f;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;
			
			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "light_color") {
				attribute_get_vector(right_value, light_color);
				light_color = glm::max(light_color, glm::vec3(0.0f));
			}
			else if (attribute == "intensity") {
				attribute_get_float(right_value, intensity);
				intensity = glm::max(intensity, 0.0f);
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: light material type require an name to specify!");
			return;
		}

		const uint32_t material_index = scene_asset_manager.CreateLightMaterial(name, light_color, intensity);
		name_to_index_map[name] = material_index;
	}

	void read_image_texture_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name; 
		std::string texture_file_name;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "texture_file_name") {
				attribute_get_string(right_value, texture_file_name);
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: image texture type require an name to specify!");
			return;
		}
		if (texture_file_name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: image texture type require an file path to specify!");
			return;
		}
		if (scene_file_dir.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: scene file invalid!");
			return;
		}

		std::filesystem::path texture_folder_path = get_texture_folder_from_scene_file(scene_file_dir);
		std::filesystem::path texture_file_path = texture_folder_path / std::filesystem::path(texture_file_name);

		const uint32_t image_texture_index = scene_asset_manager.CreateImageTexture(name, texture_file_path.string());
		name_to_index_map[name] = image_texture_index;
	}

	void read_constant_texture_float_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		float value = 1.0f;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;
		
			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "value") {
				attribute_get_float(right_value, value);
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: constant float texture type require an name to specify!");
			return;
		}

		const uint32_t constant_texture_index = scene_asset_manager.CreateConstantTexture(name, value);
		name_to_index_map[name] = constant_texture_index;
	}

	void read_constant_texture_rgb_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		glm::vec3 color(0.5f);

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "color") {
				attribute_get_vector(right_value, color);
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: constant rgb texture type require an name to specify!");
			return;
		}

		const uint32_t constant_texture_index = scene_asset_manager.CreateConstantTexture(name, color);
		name_to_index_map[name] = constant_texture_index;
	}

	void read_checker_texture_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		uint32_t texture_black_idx = EMPTY_UINT32;
		uint32_t texture_white_idx = EMPTY_UINT32;
		float	frequency = 4.0f;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "texture_black_idx") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, texture_black_idx)) {
					return;
				}
			}
			else if (attribute == "texture_white_idx") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, texture_white_idx)) {
					return;
				}
			}
			else if (attribute == "frequency") {
				attribute_get_float(right_value, frequency);
				frequency = glm::max(frequency, 0.001f);
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: checker board texture type require an name to specify!");
			return;
		}
		if (texture_black_idx == EMPTY_UINT32 || texture_white_idx == EMPTY_UINT32) {
			issue_an_error(statement_tokens.at(start), "Error: checker board texture type require sub textures to specify!");
			return;
		}

		const uint32_t checker_texture_index = scene_asset_manager.CreateCheckerBoardTexture(name, texture_black_idx, texture_white_idx, frequency);
		name_to_index_map[name] = checker_texture_index;
	}

	void read_noise_texture_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		uint32_t texture_black_idx = EMPTY_UINT32;
		uint32_t texture_white_idx = EMPTY_UINT32;
		float	frequency = 1.0f;
		bool	normalized = true;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "texture_black_idx") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, texture_black_idx)) {
					return;
				}
			}
			else if (attribute == "texture_white_idx") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, texture_white_idx)) {
					return;
				}
			}
			else if (attribute == "frequency") {
				attribute_get_float(right_value, frequency);
				frequency = glm::max(frequency, 0.001f);
			}
			else if (attribute == "normalized") {
				attribute_get_boolean(right_value, normalized);
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: noise texture type require an name to specify!");
			return;
		}
		if (texture_black_idx == EMPTY_UINT32 || texture_white_idx == EMPTY_UINT32) {
			issue_an_error(statement_tokens.at(start), "Error: noise texture type require sub textures to specify!");
			return;
		}

		const uint32_t noise_texture_index = scene_asset_manager.CreateNoiseTexture(name, texture_black_idx, texture_white_idx, frequency, normalized);
		name_to_index_map[name] = noise_texture_index;
	}

	void read_noise_texture_fbm_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		uint32_t texture_black_idx = EMPTY_UINT32;
		uint32_t texture_white_idx = EMPTY_UINT32;
		float frequency = 16.0f;
		float lacunarity = 2.0f;
		float gain = 0.5f;
		int layer_count = 4;
		float amplitude = 1.0;
		float offset = 0.0f;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;
		
			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "texture_black_idx") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, texture_black_idx)) {
					return;
				}
			}
			else if (attribute == "texture_white_idx") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, texture_white_idx)) {
					return;
				}
			}
			else if (attribute == "frequency") {
				attribute_get_float(right_value, frequency);
				frequency = glm::max(0.001f, frequency);
			}
			else if (attribute == "gain") {
				attribute_get_float(right_value, gain);
			}
			else if (attribute == "layer_count") {
				attribute_get_int(right_value, layer_count);
			}
			else if (attribute == "amplitude") {
				attribute_get_float(right_value, amplitude);
			}
			else if (attribute == "offset") {
				attribute_get_float(right_value, offset);
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: noise texture fbm type require an name to specify!");
			return;
		}
		if (texture_black_idx == EMPTY_UINT32 || texture_white_idx == EMPTY_UINT32) {
			issue_an_error(statement_tokens.at(start), "Error: noise texture fbm type require sub textures to specify!");
			return;
		}

		const uint32_t noise_texture_fbm_index = scene_asset_manager.CreateNoiseTextureFBM(name, texture_black_idx, texture_white_idx, frequency, lacunarity, gain, layer_count, amplitude, offset);
		name_to_index_map[name] = noise_texture_fbm_index;
	}

	void read_noise_texture_turbulence_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		uint32_t texture_black_idx = EMPTY_UINT32;
		uint32_t texture_white_idx = EMPTY_UINT32;
		float frequency = 16.0f;
		float lacunarity = 2.0f;
		float gain = 0.5f;
		int layer_count = 4;
		float amplitude = 1.0;
		float offset = 0.0f;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "texture_black_idx") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, texture_black_idx)) {
					return;
				}
			}
			else if (attribute == "texture_white_idx") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, texture_white_idx)) {
					return;
				}
			}
			else if (attribute == "frequency") {
				attribute_get_float(right_value, frequency);
				frequency = glm::max(0.001f, frequency);
			}
			else if (attribute == "gain") {
				attribute_get_float(right_value, gain);
			}
			else if (attribute == "layer_count") {
				attribute_get_int(right_value, layer_count);
			}
			else if (attribute == "amplitude") {
				attribute_get_float(right_value, amplitude);
			}
			else if (attribute == "offset") {
				attribute_get_float(right_value, offset);
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: noise texture turbulence type require an name to specify!");
			return;
		}
		if (texture_black_idx == EMPTY_UINT32 || texture_white_idx == EMPTY_UINT32) {
			issue_an_error(statement_tokens.at(start), "Error: noise texture turbulence type require sub textures to specify!");
			return;
		}

		const uint32_t noise_texture_turbulence_index = scene_asset_manager.CreateNoiseTextureTurbulence(name, texture_black_idx, texture_white_idx, frequency, lacunarity, gain, layer_count, amplitude, offset);
		name_to_index_map[name] = noise_texture_turbulence_index;
	}

	void read_noise_texture_marble_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		uint32_t texture_black_idx = EMPTY_UINT32;
		uint32_t texture_white_idx = EMPTY_UINT32;
		float frequency = 16.0f;
		float	lacunarity = 2.0f;
		float gain = 0.5f;
		int layer_count = 4;
		float variation = 10.0f;
		bool x_turb = true;
		bool y_turb = true;
		bool z_turb = true;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "texture_black_idx") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, texture_black_idx)) {
					return;
				}
			}
			else if (attribute == "texture_white_idx") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, texture_white_idx)) {
					return;
				}
			}
			else if (attribute == "frequency") {
				attribute_get_float(right_value, frequency);
			}
			else if (attribute == "gain") {
				attribute_get_float(right_value, gain);
			}
			else if (attribute == "layer_count") {
				attribute_get_int(right_value, layer_count);
			}
			else if (attribute == "variation") {
				attribute_get_float(right_value, variation);
			}
			else if (attribute == "x_turb") {
				attribute_get_boolean(right_value, x_turb);
			}
			else if (attribute == "y_turb") {
				attribute_get_boolean(right_value, y_turb);
			}
			else if (attribute == "z_turb") {
				attribute_get_boolean(right_value, z_turb);
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: noise texture marble type require an name to specify!");
			return;
		}
		if (texture_black_idx == EMPTY_UINT32 || texture_white_idx == EMPTY_UINT32) {
			issue_an_error(statement_tokens.at(start), "Error: noise texture marble type require sub textures to specify!");
			return;
		}

		const uint32_t noise_texture_marble_index = scene_asset_manager.CreateNoiseTextureMarble(name, texture_black_idx, texture_white_idx, frequency, lacunarity, gain, layer_count, variation, x_turb, y_turb, z_turb);
		name_to_index_map[name] = noise_texture_marble_index;
	}

	void read_noise_texture_wood_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		uint32_t texture_black_idx = EMPTY_UINT32;
		uint32_t texture_white_idx = EMPTY_UINT32;
		float	frequency = 2.0f;
		float variation = 10.0f;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "texture_black_idx") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, texture_black_idx)) {
					return;
				}
			}
			else if (attribute == "texture_white_idx") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, texture_white_idx)) {
					return;
				}
			}
			else if (attribute == "frequency") {
				attribute_get_float(right_value, frequency);
				frequency = glm::max(0.001f, frequency);
			}
			else if (attribute == "variation") {
				attribute_get_float(right_value, variation);
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: noise texture wood type require an name to specify!");
			return;
		}
		if (texture_black_idx == EMPTY_UINT32 || texture_white_idx == EMPTY_UINT32) {
			issue_an_error(statement_tokens.at(start), "Error: noise texture wood type require sub textures to specify!");
			return;
		}

		const uint32_t noise_texture_wood_index = scene_asset_manager.CreateNoiseTextureWood(name, texture_black_idx, texture_white_idx, frequency, variation);
		name_to_index_map[name] = noise_texture_wood_index;
	}

	void read_noise_texture_polka_dot_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		uint32_t texture_black_idx = EMPTY_UINT32;
		uint32_t texture_white_idx = EMPTY_UINT32;
		float	frequency = 8.0f;
		float	radius = 0.35f;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "texture_black_idx") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, texture_black_idx)) {
					return;
				}
			}
			else if (attribute == "texture_white_idx") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, texture_white_idx)) {
					return;
				}
			}
			else if (attribute == "frequency") {
				attribute_get_float(right_value, frequency);
			}
			else if (attribute == "radius") {
				attribute_get_float(right_value, radius);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: noise texture polka dot type require an name to specify!");
			return;
		}
		if (texture_black_idx == EMPTY_UINT32 || texture_white_idx == EMPTY_UINT32) {
			issue_an_error(statement_tokens.at(start), "Error: noise texture polka dot type require sub textures to specify!");
			return;
		}

		const uint32_t noise_texture_polka_dot_index = scene_asset_manager.CreateNoiseTexturePolkaDot(name, texture_black_idx, texture_white_idx, frequency, radius);
		name_to_index_map[name] = noise_texture_polka_dot_index;
	}

	void read_noise_texture_wave_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		uint32_t texture_black_idx = EMPTY_UINT32;
		uint32_t texture_white_idx = EMPTY_UINT32;
		float	freq_0 = 0.1f;
		float lacunarity_0 = 2.0f;
		float gain_0 = 0.5f;
		int layer_count_0 = 2;
		float freq_1 = 2.0f;
		float lacunarity_1 = 2.0f;
		float	gain_1 = 0.5f;
		int layer_count_1 = 4;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "texture_black_idx") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, texture_black_idx)) {
					return;
				}
			}
			else if (attribute == "texture_white_idx") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, texture_white_idx)) {
					return;
				}
			}
			else if (attribute == "freq_0") {
				attribute_get_float(right_value, freq_0);
			}
			else if (attribute == "lacunarity_0") {
				attribute_get_float(right_value, lacunarity_0);
			}
			else if (attribute == "gain_0") {
				attribute_get_float(right_value, gain_0);
			}
			else if (attribute == "layer_count_0") {
				attribute_get_int(right_value, layer_count_0);
			}
			else if (attribute == "freq_1") {
				attribute_get_float(right_value, freq_1);
			}
			else if (attribute == "lacunarity_1") {
				attribute_get_float(right_value, lacunarity_1);
			}
			else if (attribute == "gain_1") {
				attribute_get_float(right_value, gain_1);
			}
			else if (attribute == "layer_count_1") {
				attribute_get_int(right_value, layer_count_1);
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: noise texture wave type require an name to specify!");
			return;
		}
		if (texture_black_idx == EMPTY_UINT32 || texture_white_idx == EMPTY_UINT32) {
			issue_an_error(statement_tokens.at(start), "Error: noise texture wave type require sub textures to specify!");
			return;
		}

		const uint32_t noise_texture_wave_index = scene_asset_manager.CreateNoiseTextureWave(name, texture_black_idx, texture_white_idx, 
				freq_0, lacunarity_0, gain_0, layer_count_0, freq_1, lacunarity_1, gain_1, layer_count_1);
		name_to_index_map[name] = noise_texture_wave_index;
	}

	void read_distant_light_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		glm::vec3	light_color(1.0f);
		uint32_t transform_index = EMPTY_UINT32;
		float intensity = 1.0f;
		float theta_max = 5.0f;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "light_color") {
				attribute_get_vector(right_value, light_color);
				light_color = glm::max(light_color, glm::vec3(0.0f));
			}
			else if (attribute == "transform_index") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, transform_index)) {
					return;
				}
			}
			else if (attribute == "intensity") {
				attribute_get_float(right_value, intensity);
				intensity = glm::max(0.0f, intensity);
			}
			else if (attribute == "theta_max") {
				attribute_get_float(right_value, theta_max);
				theta_max = glm::max(0.0f, theta_max);
			}
			else {
				assert(0);	
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: distant light type require an name to specify!");
			return;
		}
		if (transform_index == EMPTY_UINT32) {
			issue_an_error(statement_tokens.at(start), "Error: distant light type require transform to specify!");
			return;
		}

		const uint32_t distant_light_index = scene_asset_manager.CreateDistantLight(name, light_color, transform_index, intensity, theta_max);
		name_to_index_map[name] = distant_light_index;
	}

	void read_volume_attributes(const std::vector<StatementToken>& statement_tokens, const size_t start, const size_t end, SceneModule& scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		std::string name;
		uint32_t transform_index = EMPTY_UINT32;
		glm::vec3	sigma_t(0.05f);
		glm::vec3 albedo(1.0f);
		float	g = 0.0f;
		uint32_t density_texture_index = EMPTY_UINT32;

		for (size_t line_index = start + 1; line_index < statement_tokens.size() && line_index < end; ++line_index)
		{
			const StatementToken& statement_token = statement_tokens.at(line_index);
			const std::string& attribute = statement_token.left_line;
			const std::string& right_value = statement_token.right_line;

			if (attribute == "name") {
				if (!attribute_get_name(name_to_index_map, statement_token, right_value, name)) {
					return;
				}
			}
			else if (attribute == "transform_index") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, transform_index)) {
					return;
				}
			}
			else if (attribute == "sigma_t") {
				attribute_get_vector(right_value, sigma_t);
				sigma_t = glm::max(glm::vec3(0.0f), sigma_t);
			}
			else if (attribute == "albedo") {
				attribute_get_vector(right_value, albedo);
				albedo = glm::max(glm::vec3(0.0f), albedo);
			}
			else if (attribute == "g") {
				attribute_get_float(right_value, g);
				g = glm::clamp(g, 0.0f, 1.0f);
			}
			else if (attribute == "density_texture_index") {
				if (!attribute_get_index_from_name(name_to_index_map, statement_token, right_value, density_texture_index)) {
					return;
				}
			}
			else {
				assert(0);
			}
		}

		if (name.empty()) {
			issue_an_error(statement_tokens.at(start), "Error: volume type require an name to specify!");
			return;
		}
		if (transform_index == EMPTY_UINT32) {
			issue_an_error(statement_tokens.at(start), "Error: volume type require transform to specify!");
			return;
		}

		const uint32_t volume_index = scene_asset_manager.CreateVolume(name, transform_index, sigma_t, albedo, g, density_texture_index);
		name_to_index_map[name] = volume_index;
	}

	void read_asset_attributes(const std::vector<StatementToken> &statement_tokens, const std::string &basic_type, const size_t start, const size_t end, SceneModule &scene_asset_manager, std::unordered_map<std::string, uint32_t>& name_to_index_map, const std::filesystem::path& scene_file_dir)
	{
		using TYPE_LOADING_FUNC = void (*)(const std::vector<StatementToken>&, const size_t, const size_t, SceneModule&, std::unordered_map<std::string, uint32_t>&, const std::filesystem::path&);
		static const std::unordered_map<std::string, TYPE_LOADING_FUNC> type_loading_function_map =
		{
			{ "Camera", read_camera_attributes },

			{ "Transform", read_transform_attributes },

			{ "Mesh", read_mesh_attributes },
			{ "Cube", read_cube_attributes },
			{ "Sphere", read_sphere_attributes },

			{ "PrimitiveInstance", read_primitive_instance_attributes },

			{ "DefaultMaterial", read_default_material_attributes },
			{ "LightMaterial", read_light_material_attributes },

			{ "ImageTexture", read_image_texture_attributes },
			{ "ConstantTextureFloat", read_constant_texture_float_attributes },
			{ "ConstantTextureRgb", read_constant_texture_rgb_attributes },
			{ "CheckerBoardTexture", read_checker_texture_attributes },
			{ "NoiseTexture", read_noise_texture_attributes },
			{ "NoiseTextureFBM", read_noise_texture_fbm_attributes },
			{ "NoiseTextureTurbulence", read_noise_texture_turbulence_attributes },
			{ "NoiseTextureMarble", read_noise_texture_marble_attributes },
			{ "NoiseTextureWood", read_noise_texture_wood_attributes },
			{ "NoiseTexturePolkaDot", read_noise_texture_polka_dot_attributes },
			{ "NoiseTextureWave", read_noise_texture_wave_attributes },

			{ "DistantLight", read_distant_light_attributes },

			{ "Volume", read_volume_attributes }
		};

		if (basic_type.empty()) {
			return;
		}

		auto iter = type_loading_function_map.find(basic_type);
		if (iter == type_loading_function_map.end()) {
			return;
		}

		return (*iter).second(statement_tokens, start, end, scene_asset_manager, name_to_index_map, scene_file_dir);
	}

	void load_scene_asset(const std::vector<StatementToken> &statement_tokens, std::shared_ptr<YumeRT::SceneModule> scene_module, const std::filesystem::path& scene_file_dir)
	{
		auto& scene_asset_manager = *scene_module;

		std::unordered_map<std::string, uint32_t> name_to_index_map;

		size_t total_asset_count = 0;
		for (const auto& statement_token : statement_tokens) {
			if (statement_token.type == STATEMENT_TYPE::TYPE_BRACKET && is_begin_bracket(statement_token.left_line)) {
				++total_asset_count;
			}
		}

		size_t asset_index = 0;
		size_t start = 0;
		while (start < statement_tokens.size()) {
			const StatementToken& current_line = statement_tokens.at(start);
			if (!(current_line.type == STATEMENT_TYPE::TYPE_BRACKET && is_begin_bracket(current_line.left_line))) {
				++start;
				continue;
			}

			size_t end = start + 1;
			for (;  end < statement_tokens.size(); ++end) {
				const StatementToken& end_line = statement_tokens.at(end);
				if (end_line.type == STATEMENT_TYPE::TYPE_BRACKET && is_end_bracket(end_line.left_line)) {
					assert(current_line.right_line == end_line.right_line);
					break;
				}
			}

			std::string asset_name;
			for (size_t name_line_index = start + 1; name_line_index < end; ++name_line_index) {
				if (statement_tokens.at(name_line_index).left_line == "name") {
					asset_name = statement_tokens.at(name_line_index).right_line;
					break;
				}
			}
			if (asset_name.size() >= 2 && asset_name.front() == '\"' && asset_name.back() == '\"') {
				asset_name = asset_name.substr(1, asset_name.size() - 2);
			}
			printf("Loading scene asset (%zu/%zu): %s \"%s\"\n", asset_index + 1, total_asset_count, current_line.right_line.c_str(), asset_name.c_str());
			fflush(stdout);

			read_asset_attributes(statement_tokens, current_line.right_line, start, end, scene_asset_manager, name_to_index_map, scene_file_dir);
			++asset_index;
			start = end + 1;
		}
	}

	bool SceneParser::parsing_scene(const std::string& file_path, std::shared_ptr<YumeRT::SceneModule> scene_module)
	{
		std::string _own_file_path = file_path;
		for (auto& c : _own_file_path) {
			if (c == '\\') {
				c = '/';
			}
		}

		const size_t dot_pos = _own_file_path.find('.');
		if (dot_pos == std::string::npos) {
			std::cerr << "Error: invalid file type!" << std::endl;
			return false;
		}

		const std::string postfix(_own_file_path.begin() + dot_pos, _own_file_path.end());
		if (postfix != dsk_file_postfix) {
			std::cerr << "Error: invalid file type!" << std::endl;
			return false;
		}

		// note: read file.
		std::ifstream ifs;

		ifs.open(_own_file_path);
		if (!ifs.is_open()) {
			std::cerr << "Error: failed to open file!" << std::endl;
			return false;
		}

		printf("Scene file load begin: %s\n", _own_file_path.c_str());
		fflush(stdout);
		const auto load_begin_time = std::chrono::steady_clock::now();

		std::filesystem::path scene_file_dir(_own_file_path);

		std::string file_content;
		std::string str_buf;
		while (std::getline(ifs, str_buf)) {
			if (str_buf.empty()) {
				continue;
			}
			str_buf.erase(std::remove_if(str_buf.begin(), str_buf.end(), [](char x) {return !is_valid_character(x); }), str_buf.end());
			if (str_buf.empty() || str_buf.at(0) == '#') {
				continue;
			}
			file_content.append(str_buf);
		}

		ifs.close();

		std::vector<std::string> texts;
		split_file_content(texts, file_content);

		// note: remove redundant symbol for each line.
		for (auto& str : texts) {
			assert(str.find(';') == std::string::npos);
		}

		std::vector<StatementToken> statement_tokens;
		statement_tokens.reserve(texts.size());

		for (size_t line_index = 0; line_index < texts.size(); ++line_index) 
		{
			const std::string& statement_token = texts.at(line_index);
			if (statement_token.empty()) {
				continue;
			}
			if (statement_token[0] == '#') {
				continue;
			}

			STATEMENT_TYPE statement_type;
			if (!is_valid_statement(statement_token, line_index, statement_type)) {
				std::cerr << "Error: illegal statement!" << " at line " << line_index << "." << std::endl;
				return false;
			}

			statement_tokens.push_back(extract_statement_token(statement_token, line_index, statement_type));
		}

		if (!check_statements_valid(statement_tokens)) {
			return false;
		}

		{
			const double parse_cost_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - load_begin_time).count();
			printf("Scene file parse complete: %zu statements, cost %.2f s.\n", statement_tokens.size(), parse_cost_seconds);
			fflush(stdout);
		}

		load_scene_asset(statement_tokens, scene_module, scene_file_dir);

		{
			auto& scene_asset_manager = *scene_module;
			const double load_cost_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - load_begin_time).count();
			printf("Scene file load complete: %zu geometries, %zu transforms, %zu materials, %zu textures, %zu primitive instances, %zu distant lights, %zu volumes, cost %.2f s.\n",
				scene_asset_manager.geometries.size(), scene_asset_manager.transform_states.size(), scene_asset_manager.materials.size(), scene_asset_manager.textures.size(), scene_asset_manager.primitive_instances.size(), scene_asset_manager.distant_lights.size(), scene_asset_manager.volumes.size(), load_cost_seconds);
			fflush(stdout);
		}

		return true;
	}

	
	void save_attribute_vector(std::ofstream& os, const std::string& attribute_name, const glm::vec3 value) {
		os << "\t" << attribute_name << " = ";
		os << "<" << value.x << "," << value.y << "," << value.z << ">" << ";" << std::endl;
	}

	void save_attribute_float(std::ofstream& os, const std::string& attribute_name, const float value) {
		os << "\t" << attribute_name << " = " << value <<  ";" << std::endl;
	}

	void save_attribute_int(std::ofstream& os, const std::string& attribute_name, const int value) {
		os << "\t" << attribute_name << " = " << value << ";" << std::endl;
	}

	void save_attribute_triangle_array(std::ofstream& os, const std::string& attribute_name, const Triangle* triangles, const size_t size)
	{
		os << "\t" << attribute_name << " = " << "\n";
		os << "[";
		for (size_t index = 0; index < size; ++index) {
			const Triangle& tri = triangles[index];
			os << tri.id0 << ",";
			os << tri.id1 << ",";
			os << tri.id2;
			if (index != size - 1) {
				os << ",";
				os << "\n";
			}
		}
		os << "]";
		os << ";" << std::endl;
	}

	void save_attribute_float_array(std::ofstream& os, const std::string& attribute_name, const float* data, const size_t size, const size_t per_line_element_count)
	{
		os << "\t" << attribute_name << " = " << "\n";
		os << "[";
		for (size_t index = 0; index < size; index += per_line_element_count) {
			for (size_t line_i = 0; line_i < per_line_element_count - 1; ++line_i) {
				os << data[index + line_i] << ",";
			}
			os << data[index + per_line_element_count - 1];
			if (index + per_line_element_count - 1 != size - 1) {
				os << ",";
				os << "\n";
			}
		}
		os << "]";
		os << ";" << std::endl;
	}

	void save_attribute_int_array(std::ofstream& os, const std::string& attribute_name, const uint32_t* data, const size_t size, const size_t per_line_element_count)
	{
		os << "\t" << attribute_name << " = " << "\n";
		os << "[";
		for (size_t index = 0; index < size; index += 3) {
			for (size_t line_i = 0; line_i < per_line_element_count - 1; ++line_i) {
				os << data[index + line_i] << ",";
			}
			os << data[index + per_line_element_count - 1];
			if (index + per_line_element_count - 1 != size - 1) {
				os << ",";
				os << "\n";
			}
		}
		os << "]";
		os << ";" << std::endl;
	}

	void save_attribute_boolean(std::ofstream& os, const std::string &attribute_name, const bool value) {
		os << "\t" << attribute_name << " = " << (value ? "true" : "false") << ";" << std::endl;
	}

	void save_attribute_name(std::ofstream& os, const std::string& attribute_name, const std::string value) {
		os << "\t" << attribute_name << " = " << "\"" << value << "\"" << ";" << std::endl;
	}

	bool SceneParser::save_camera(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const std::filesystem::path& texture_folder_path)
	{
		const auto& camera = scene_asset_manager.GetSceneCamera();
		float fov = glm::degrees(camera.GetFov());
		glm::vec3 from = camera.GetPosition();
		glm::vec3 look = from - camera.GetW();

		os << "Begin(Camera);" << std::endl;
		save_attribute_float(os, "fov", fov);
		save_attribute_vector(os, "from", from);
		save_attribute_vector(os, "look", look);
		os << "End(Camera);" << "\n" << std::endl;

		return true;
	}

	bool SceneParser::save_mesh(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.geometries.size())) {
			return false;
		}
		
		const auto& mesh = scene_asset_manager.geometries.at(index).triangle_mesh;
		const auto& name = scene_asset_manager.geometry_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		const Triangle* mesh_triangles = mesh.GetTrianglesHost();
		const size_t triangle_count = mesh.triangle_count;

		const uint32_t* mesh_position_indices = mesh.GetPositionIndicesHost();
		const glm::vec3* mesh_positions = mesh.GetPositionsHost();
		const size_t position_array_size = mesh.position_array_size;

		const uint32_t* mesh_normal_indices = mesh.GetNormalIndicesHost();
		const glm::vec3* mesh_normals = mesh.GetNormalsHost();
		const size_t normal_array_size = mesh.normal_array_size;

		const uint32_t* mesh_texcoord_indices = mesh.GetTexcoordIndicesHost();
		const glm::vec2* mesh_texcoords = mesh.GetTexcoordsHost();
		const size_t texcoord_array_size = mesh.texcoord_array_size;

		os << "Begin(Mesh);" << std::endl;
		save_attribute_name(os, "name", name);
		save_attribute_triangle_array(os, "mesh_triangles", mesh_triangles, triangle_count);
		save_attribute_int_array(os, "mesh_position_indices", mesh_position_indices, triangle_count * 3, 3);
		save_attribute_float_array(os, "mesh_positions", (float*)mesh_positions, position_array_size, 3);
		save_attribute_int_array(os, "mesh_normal_indices", mesh_normal_indices, triangle_count * 3, 3);
		save_attribute_float_array(os, "mesh_normals", (float*)mesh_normals, normal_array_size, 3);
		if (mesh_texcoord_indices != nullptr) {
			save_attribute_int_array(os, "mesh_texcoord_indices", mesh_texcoord_indices, triangle_count * 3, 3);
		}
		if (mesh_texcoords != nullptr) {
			save_attribute_float_array(os, "mesh_texcoords", (float*)mesh_texcoords, texcoord_array_size, 2);
		}
		os << "End(Mesh);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}
	bool SceneParser::save_sphere(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.geometries.size())) {
			return false;
		}

		const auto& sphere = scene_asset_manager.geometries.at(index).sphere;
		const auto& name = scene_asset_manager.geometry_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		os << "Begin(Sphere);" << std::endl;
		save_attribute_name(os, "name", name);
		save_attribute_float(os, "radius", sphere.radius);
		os << "End(Sphere);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}
	bool SceneParser::save_geometry(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		const auto& geometry = scene_asset_manager.geometries.at(index);
		if (geometry.geometry_type == GEOMETRY_TYPE::TRIANGLE_MESH) {
			return save_mesh(os, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		else if (geometry.geometry_type == GEOMETRY_TYPE::SPHERE) {
			return save_sphere(os, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		else {
			return false;
		}
	}

	bool SceneParser::save_transform(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.transform_states.size())) {
			return false;
		}

		const auto& transform_state = scene_asset_manager.transform_states.at(index);
		const auto& name = scene_asset_manager.transform_names.at(index);
		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		if ((uint32_t)transform_state.parent_transform_index < (uint32_t)scene_asset_manager.transform_states.size()) {
			save_transform(os, processed_name_set, scene_asset_manager, transform_state.parent_transform_index, texture_folder_path);
		}

		os << "Begin(Transform);" << std::endl;
		save_attribute_name(os, "name", name);
		save_attribute_vector(os, "translate", transform_state.translate_xyz);
		save_attribute_vector(os, "scale", transform_state.scale_xyz);
		save_attribute_vector(os, "rotate", transform_state.rotate_xyz);
		if ((uint32_t)transform_state.parent_transform_index < (uint32_t)scene_asset_manager.transform_states.size()) {
			save_attribute_name(os, "parent_transform_index", scene_asset_manager.transform_names.at(transform_state.parent_transform_index));
		}
		os << "End(Transform);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}

	bool SceneParser::save_default_material(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.materials.size())) {
			return false;
		}

		const auto& default_material = scene_asset_manager.materials.at(index).default_mtl;
		const auto& name = scene_asset_manager.material_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		if ((uint32_t)default_material.diffuse_albedo_tex < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, default_material.diffuse_albedo_tex, texture_folder_path);
		}
		if ((uint32_t)default_material.alpha_x_tex < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, default_material.alpha_x_tex, texture_folder_path);
		}
		if ((uint32_t)default_material.specular_albedo_tex < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, default_material.specular_albedo_tex, texture_folder_path);
		}
		if ((uint32_t)default_material.alpha_y_tex < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, default_material.alpha_y_tex, texture_folder_path);
		}
		if ((uint32_t)default_material.specular_weight_tex < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, default_material.specular_weight_tex, texture_folder_path);
		}
		if ((uint32_t)default_material.metalness_tex < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, default_material.metalness_tex, texture_folder_path);
		}
		if ((uint32_t)default_material.transmission_weight_tex < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, default_material.transmission_weight_tex, texture_folder_path);
		}
		if ((uint32_t)default_material.normal_mapping_tex < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, default_material.normal_mapping_tex, texture_folder_path);
		}
		if ((uint32_t)default_material.bump_mapping_tex < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, default_material.bump_mapping_tex, texture_folder_path);
		}

		if ((uint32_t)default_material.coat_albedo_tex < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, default_material.coat_albedo_tex, texture_folder_path);
		}
		if ((uint32_t)default_material.coat_weight_tex < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, default_material.coat_weight_tex, texture_folder_path);
		}
		if ((uint32_t)default_material.coat_thickness_tex < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, default_material.coat_thickness_tex, texture_folder_path);
		}
		if ((uint32_t)default_material.coat_roughness_x_tex < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, default_material.coat_roughness_x_tex, texture_folder_path);
		}
		if ((uint32_t)default_material.coat_roughness_y_tex < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, default_material.coat_roughness_y_tex, texture_folder_path);
		}

		if ((uint32_t)default_material.coat_normal_tex < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, default_material.coat_normal_tex, texture_folder_path);
		}

		os << "Begin(DefaultMaterial);" << std::endl;
		save_attribute_name(os, "name", name);
		save_attribute_vector(os, "diffuse_albedo", default_material.diffuse_albedo);
		save_attribute_vector(os, "specular_albedo", default_material.specular_albedo);
		save_attribute_float(os, "roughness_x", default_material.alpha_x);
		save_attribute_float(os, "roughness_y", default_material.alpha_y);
		save_attribute_float(os, "ior_n", default_material.ior_n);
		save_attribute_float(os, "metalness", default_material.metalness);
		save_attribute_float(os, "specular_weight", default_material.specular_weight);
		save_attribute_float(os, "transmission_weight", default_material.transmission_weight);

		save_attribute_int(os, "ior_priority", default_material.ior_priority);

		save_attribute_vector(os, "coat_albedo", default_material.coat_albedo);
		save_attribute_float(os, "coat_weight", default_material.coat_weight);
		save_attribute_float(os, "coat_thickness", default_material.coat_thickness);
		save_attribute_float(os, "coat_ior", default_material.coat_ior);
		save_attribute_float(os, "coat_roughness_x", default_material.coat_roughness_x);
		save_attribute_float(os, "coat_roughness_y", default_material.coat_roughness_y);

		if ((uint32_t)default_material.diffuse_albedo_tex < scene_asset_manager.textures.size()) {
			save_attribute_name(os, "diffuse_albedo_tex", scene_asset_manager.texture_names.at(default_material.diffuse_albedo_tex));
		}
		if ((uint32_t)default_material.alpha_x_tex < scene_asset_manager.textures.size()) {
			save_attribute_name(os, "alpha_x_tex", scene_asset_manager.texture_names.at(default_material.alpha_x_tex));
		}
		if ((uint32_t)default_material.specular_albedo_tex < scene_asset_manager.textures.size()) {
			save_attribute_name(os, "specular_albedo_tex", scene_asset_manager.texture_names.at(default_material.specular_albedo_tex));
		}
		if ((uint32_t)default_material.alpha_y_tex < scene_asset_manager.textures.size()) {
			save_attribute_name(os, "alpha_y_tex", scene_asset_manager.texture_names.at(default_material.alpha_y_tex));
		}
		if ((uint32_t)default_material.specular_weight_tex < scene_asset_manager.textures.size()) {
			save_attribute_name(os, "specular_weight_tex", scene_asset_manager.texture_names.at(default_material.specular_weight_tex));
		}
		if ((uint32_t)default_material.metalness_tex < scene_asset_manager.textures.size()) {
			save_attribute_name(os, "metalness_tex", scene_asset_manager.texture_names.at(default_material.metalness_tex));
		}
		if ((uint32_t)default_material.transmission_weight_tex < scene_asset_manager.textures.size()) {
			save_attribute_name(os, "transmission_weight_tex", scene_asset_manager.texture_names.at(default_material.transmission_weight_tex));
		}

		if ((uint32_t)default_material.normal_mapping_tex < scene_asset_manager.textures.size()) {
			save_attribute_name(os, "normal_mapping_tex", scene_asset_manager.texture_names.at(default_material.normal_mapping_tex));
		}
		if ((uint32_t)default_material.bump_mapping_tex < scene_asset_manager.textures.size()) {
			save_attribute_name(os, "bump_mapping_tex", scene_asset_manager.texture_names.at(default_material.bump_mapping_tex));
		}

		if ((uint32_t)default_material.coat_albedo_tex < scene_asset_manager.textures.size()) {
			save_attribute_name(os, "coat_albedo_tex", scene_asset_manager.texture_names.at(default_material.coat_albedo_tex));
		}
		if ((uint32_t)default_material.coat_weight_tex < scene_asset_manager.textures.size()) {
			save_attribute_name(os, "coat_weight_tex", scene_asset_manager.texture_names.at(default_material.coat_weight_tex));
		}
		if ((uint32_t)default_material.coat_thickness_tex < scene_asset_manager.textures.size()) {
			save_attribute_name(os, "coat_thickness_tex", scene_asset_manager.texture_names.at(default_material.coat_thickness_tex));
		}
		if ((uint32_t)default_material.coat_roughness_x_tex < scene_asset_manager.textures.size()) {
			save_attribute_name(os, "coat_roughness_x_tex", scene_asset_manager.texture_names.at(default_material.coat_roughness_x_tex));
		}
		if ((uint32_t)default_material.coat_roughness_y_tex < scene_asset_manager.textures.size()) {
			save_attribute_name(os, "coat_roughness_y_tex", scene_asset_manager.texture_names.at(default_material.coat_roughness_y_tex));
		}

		if ((uint32_t)default_material.coat_normal_tex < scene_asset_manager.textures.size()) {
			save_attribute_name(os, "coat_normal_tex", scene_asset_manager.texture_names.at(default_material.coat_normal_tex));
			save_attribute_name(os, "coat_normal_tex_type", default_material.coat_normal_tex_type == COAT_NORMAL_TEXTURE_TYPE::NORMAL_TEX ? "normal_map" : "bump_map");
		}

		os << "End(DefaultMaterial);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}
	bool SceneParser::save_light_material(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.materials.size())) {
			return false;
		}

		const auto& light_material = scene_asset_manager.materials.at(index).light_mtl;
		const auto& name = scene_asset_manager.material_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		os << "Begin(LightMaterial);" << std::endl;
		save_attribute_name(os, "name", name);
		save_attribute_vector(os, "light_color", light_material.light_color);
		save_attribute_float(os, "intensity", light_material.intensity);
		os << "End(LightMaterial);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}
	bool SceneParser::save_material(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		const auto& material = scene_asset_manager.materials.at(index);
		if (material.material_type == MATERIAL_TYPE::DEFAULT_MTL) {
			return save_default_material(os, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		else if (material.material_type == MATERIAL_TYPE::LIGHT_MTL) {
			return save_light_material(os, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		else {
			return false;
		}
	}

	bool SceneParser::save_primitive_instance(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t unique_index, const std::filesystem::path& texture_folder_path)
	{
		auto iter = scene_asset_manager.primitive_index_to_name_map.find(unique_index);
		if (iter == scene_asset_manager.primitive_index_to_name_map.end()) {
			return false;
		}
		
		const std::string &name = (*iter).second;
		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		const auto& primitive_instance = scene_asset_manager.primitive_instances.at(scene_asset_manager.primitive_index_to_index_map[unique_index]);
		save_geometry(os, processed_name_set, scene_asset_manager, primitive_instance.geometry_idx, texture_folder_path);
		save_transform(os, processed_name_set, scene_asset_manager, primitive_instance.transform_idx, texture_folder_path);
		if ((uint32_t)primitive_instance.material_idx < (uint32_t)scene_asset_manager.materials.size()) {
			save_material(os, processed_name_set, scene_asset_manager, primitive_instance.material_idx, texture_folder_path);
		}
		if ((uint32_t)primitive_instance.inner_volume_idx < (uint32_t)scene_asset_manager.volumes.size()) {
			save_volume(os, processed_name_set, scene_asset_manager, primitive_instance.inner_volume_idx, texture_folder_path);
		}

		os << "Begin(PrimitiveInstance);" << std::endl;
		save_attribute_name(os, "name", name);
		save_attribute_name(os, "geometry_index", scene_asset_manager.geometry_names.at(primitive_instance.geometry_idx));
		save_attribute_name(os, "transform_index", scene_asset_manager.transform_names.at(primitive_instance.transform_idx));
		if ((uint32_t)primitive_instance.material_idx < (uint32_t)scene_asset_manager.materials.size()) {
			save_attribute_name(os, "material_index", scene_asset_manager.material_names.at(primitive_instance.material_idx));
		}
		if ((uint32_t)primitive_instance.inner_volume_idx < (uint32_t)scene_asset_manager.volumes.size()) {
			save_attribute_name(os, "inner_volume_index", scene_asset_manager.volume_names.at(primitive_instance.inner_volume_idx));
		}
		save_attribute_boolean(os, "treat_as_boundary", primitive_instance.treat_as_boundary);
		os << "End(PrimitiveInstance);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}

	bool SceneParser::save_image_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.textures.size())) {
			return false;
		}

		const auto& image_texture = scene_asset_manager.textures.at(index).image_texture;
		const auto& name = scene_asset_manager.texture_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		processed_name_set.insert(name);

		if (texture_folder_path.empty()) {
			std::cerr << "Error: Fail to save image texture file!" << std::endl;
			return false;
		}

		const std::string &file_path_str = scene_asset_manager.image_texture_files.at(image_texture.file_offset).file_name;

		std::filesystem::path file_path(file_path_str);
		std::filesystem::path  file_name = file_path.filename();
		std::filesystem::path dst_file_path = texture_folder_path / file_name;

		try {
			std::filesystem::copy_file(file_path, dst_file_path, std::filesystem::copy_options::none);
		}
		catch (const std::filesystem::filesystem_error& e) {
			std::cerr << e.what() << std::endl;
		}

		os << "Begin(ImageTexture);" << std::endl;
		save_attribute_name(os, "name", name);
		save_attribute_name(os, "texture_file_name", file_name.string());
		os << "End(ImageTexture);" << "\n" << std::endl;

		return true;
	}
	bool SceneParser::save_constant_float_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.textures.size())) {
			return false;
		}

		const auto& const_texture_float = scene_asset_manager.textures.at(index).constant_texture_float;
		const auto& name = scene_asset_manager.texture_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		os << "Begin(ConstantTextureFloat);" << std::endl;
		save_attribute_name(os, "name", name);
		save_attribute_float(os, "value", const_texture_float.value);
		os << "End(ConstantTextureFloat);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}
	bool SceneParser::save_constant_rgb_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.textures.size())) {
			return false;
		}

		const auto& const_texture_rgb = scene_asset_manager.textures.at(index).constant_texture_rgb;
		const auto& name = scene_asset_manager.texture_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}
	
		os << "Begin(ConstantTextureRgb);" << std::endl;
		save_attribute_name(os, "name", name);
		save_attribute_vector(os, "color", const_texture_rgb.color);
		os << "End(ConstantTextureRgb);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}
	bool SceneParser::save_checker_board_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.textures.size())) {
			return false;
		}

		const auto& checker_board_texture = scene_asset_manager.textures.at(index).checker_board_texture;
		const auto& name = scene_asset_manager.texture_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		if ((uint32_t)checker_board_texture.child_texture_indices.texture_black < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, checker_board_texture.child_texture_indices.texture_black, texture_folder_path);
		}
		if ((uint32_t)checker_board_texture.child_texture_indices.texture_white < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, checker_board_texture.child_texture_indices.texture_white, texture_folder_path);
		}

		os << "Begin(CheckerBoardTexture);" << std::endl;
		save_attribute_name(os, "name", name);
		if ((uint32_t)checker_board_texture.child_texture_indices.texture_black < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "texture_black_idx", scene_asset_manager.texture_names.at(checker_board_texture.child_texture_indices.texture_black));
		}
		if ((uint32_t)checker_board_texture.child_texture_indices.texture_white < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "texture_white_idx", scene_asset_manager.texture_names.at(checker_board_texture.child_texture_indices.texture_white));
		}
		save_attribute_float(os, "frequency", checker_board_texture.frequency);
		os << "End(CheckerBoardTexture);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}
	bool SceneParser::save_noise_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.textures.size())) {
			return false;
		}

		const auto& noise_texture = scene_asset_manager.textures.at(index).noise_texture;
		const auto& name = scene_asset_manager.texture_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		if ((uint32_t)noise_texture.child_texture_indices.texture_black < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, noise_texture.child_texture_indices.texture_black, texture_folder_path);
		}
		if ((uint32_t)noise_texture.child_texture_indices.texture_white < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, noise_texture.child_texture_indices.texture_white, texture_folder_path);
		}

		os << "Begin(NoiseTexture);" << std::endl;
		save_attribute_name(os, "name", name);
		if ((uint32_t)noise_texture.child_texture_indices.texture_black < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "texture_black_idx", scene_asset_manager.texture_names.at(noise_texture.child_texture_indices.texture_black));
		}
		if ((uint32_t)noise_texture.child_texture_indices.texture_white < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "texture_white_idx", scene_asset_manager.texture_names.at(noise_texture.child_texture_indices.texture_white));
		}
		save_attribute_float(os, "frequency", noise_texture.frequency);
		save_attribute_boolean(os, "normalized", noise_texture.normalized);
		os << "End(NoiseTexture);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}
	bool SceneParser::save_noise_fbm_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.textures.size())) {
			return false;
		}

		const auto& noise_texture_fbm = scene_asset_manager.textures.at(index).noise_texture_fbm;
		const auto& name = scene_asset_manager.texture_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		if ((uint32_t)noise_texture_fbm.child_texture_indices.texture_black < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, noise_texture_fbm.child_texture_indices.texture_black, texture_folder_path);
		}
		if ((uint32_t)noise_texture_fbm.child_texture_indices.texture_white < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, noise_texture_fbm.child_texture_indices.texture_white, texture_folder_path);
		}

		os << "Begin(NoiseTextureFBM);" << std::endl;
		save_attribute_name(os, "name", name);
		if ((uint32_t)noise_texture_fbm.child_texture_indices.texture_black < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "texture_black_idx", scene_asset_manager.texture_names.at(noise_texture_fbm.child_texture_indices.texture_black));
		}
		if ((uint32_t)noise_texture_fbm.child_texture_indices.texture_white < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "texture_white_idx", scene_asset_manager.texture_names.at(noise_texture_fbm.child_texture_indices.texture_white));
		}
		save_attribute_float(os, "frequency", noise_texture_fbm.frequency);
		save_attribute_float(os, "lacunarity", noise_texture_fbm.lacunarity);
		save_attribute_float(os, "gain", noise_texture_fbm.gain);
		save_attribute_int(os, "layer_count", noise_texture_fbm.layer_count);
		save_attribute_float(os, "amplitude", noise_texture_fbm.amplitude);
		save_attribute_float(os, "offset", noise_texture_fbm.offset);
		os << "End(NoiseTextureFBM);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}
	bool SceneParser::save_noise_turbulence_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.textures.size())) {
			return false;
		}

		const auto& noise_texture_turbulence = scene_asset_manager.textures.at(index).noise_texture_turbulence;
		const auto& name = scene_asset_manager.texture_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		if ((uint32_t)noise_texture_turbulence.child_texture_indices.texture_black < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, noise_texture_turbulence.child_texture_indices.texture_black, texture_folder_path);
		}
		if ((uint32_t)noise_texture_turbulence.child_texture_indices.texture_white < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, noise_texture_turbulence.child_texture_indices.texture_white, texture_folder_path);
		}

		os << "Begin(NoiseTextureTurbulence);" << std::endl;
		save_attribute_name(os, "name", name);
		if ((uint32_t)noise_texture_turbulence.child_texture_indices.texture_black < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "texture_black_idx", scene_asset_manager.texture_names.at(noise_texture_turbulence.child_texture_indices.texture_black));
		}
		if ((uint32_t)noise_texture_turbulence.child_texture_indices.texture_white < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "texture_white_idx", scene_asset_manager.texture_names.at(noise_texture_turbulence.child_texture_indices.texture_white));
		}
		save_attribute_float(os, "frequency", noise_texture_turbulence.frequency);
		save_attribute_float(os, "lacunarity", noise_texture_turbulence.lacunarity);
		save_attribute_float(os, "gain", noise_texture_turbulence.gain);
		save_attribute_int(os, "layer_count", noise_texture_turbulence.layer_count);
		save_attribute_float(os, "amplitude", noise_texture_turbulence.amplitude);
		save_attribute_float(os, "offset", noise_texture_turbulence.offset);
		os << "End(NoiseTextureTurbulence);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}
	bool SceneParser::save_noise_marble_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.textures.size())) {
			return false;
		}

		const auto& noise_texture_marble = scene_asset_manager.textures.at(index).noise_texture_marble;
		const auto& name = scene_asset_manager.texture_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		if ((uint32_t)noise_texture_marble.child_texture_indices.texture_black < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, noise_texture_marble.child_texture_indices.texture_black, texture_folder_path);
		}
		if ((uint32_t)noise_texture_marble.child_texture_indices.texture_white < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, noise_texture_marble.child_texture_indices.texture_white, texture_folder_path);
		}

		os << "Begin(NoiseTextureMarble);" << std::endl;
		save_attribute_name(os, "name", name);
		if ((uint32_t)noise_texture_marble.child_texture_indices.texture_black < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "texture_black_idx", scene_asset_manager.texture_names.at(noise_texture_marble.child_texture_indices.texture_black));
		}
		if ((uint32_t)noise_texture_marble.child_texture_indices.texture_white < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "texture_white_idx", scene_asset_manager.texture_names.at(noise_texture_marble.child_texture_indices.texture_white));
		}
		save_attribute_float(os, "frequency", noise_texture_marble.frequency);
		save_attribute_float(os, "lacunarity", noise_texture_marble.lacunarity);
		save_attribute_float(os, "gain", noise_texture_marble.gain);
		save_attribute_int(os, "layer_count", noise_texture_marble.layer_count);
		save_attribute_float(os, "variation", noise_texture_marble.variation);
		save_attribute_boolean(os, "x_turb", noise_texture_marble.x_turb);
		save_attribute_boolean(os, "y_turb", noise_texture_marble.y_turb);
		save_attribute_boolean(os, "z_turb", noise_texture_marble.z_turb);
		os << "End(NoiseTextureMarble);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}
	bool SceneParser::save_noise_wood_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.textures.size())) {
			return false;
		}

		const auto& noise_texture_wood = scene_asset_manager.textures.at(index).noise_texture_wood;
		const auto& name = scene_asset_manager.texture_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		if ((uint32_t)noise_texture_wood.child_texture_indices.texture_black < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, noise_texture_wood.child_texture_indices.texture_black, texture_folder_path);
		}
		if ((uint32_t)noise_texture_wood.child_texture_indices.texture_white < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, noise_texture_wood.child_texture_indices.texture_white, texture_folder_path);
		}

		os << "Begin(NoiseTextureWood);" << std::endl;
		save_attribute_name(os, "name", name);
		if ((uint32_t)noise_texture_wood.child_texture_indices.texture_black < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "texture_black_idx", scene_asset_manager.texture_names.at(noise_texture_wood.child_texture_indices.texture_black));
		}
		if ((uint32_t)noise_texture_wood.child_texture_indices.texture_white < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "texture_white_idx", scene_asset_manager.texture_names.at(noise_texture_wood.child_texture_indices.texture_white));
		}
		save_attribute_float(os, "frequency", noise_texture_wood.frequency);
		save_attribute_float(os, "variation", noise_texture_wood.variation);
		os << "End(NoiseTextureWood);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}
	bool SceneParser::save_noise_polka_dot_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.textures.size())) {
			return false;
		}

		const auto& noise_texture_polka_dot = scene_asset_manager.textures.at(index).noise_texture_polka_dot;
		const auto& name = scene_asset_manager.texture_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		if ((uint32_t)noise_texture_polka_dot.child_texture_indices.texture_black < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, noise_texture_polka_dot.child_texture_indices.texture_black, texture_folder_path);
		}
		if ((uint32_t)noise_texture_polka_dot.child_texture_indices.texture_white < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, noise_texture_polka_dot.child_texture_indices.texture_white, texture_folder_path);
		}

		os << "Begin(NoiseTexturePolkaDot);" << std::endl;
		save_attribute_name(os, "name", name);
		if ((uint32_t)noise_texture_polka_dot.child_texture_indices.texture_black < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "texture_black_idx", scene_asset_manager.texture_names.at(noise_texture_polka_dot.child_texture_indices.texture_black));
		}
		if ((uint32_t)noise_texture_polka_dot.child_texture_indices.texture_white < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "texture_white_idx", scene_asset_manager.texture_names.at(noise_texture_polka_dot.child_texture_indices.texture_white));
		}
		save_attribute_float(os, "frequency", noise_texture_polka_dot.frequency);
		save_attribute_float(os, "radius", noise_texture_polka_dot.radius);
		os << "End(NoiseTexturePolkaDot);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}
	bool SceneParser::save_noise_wave_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.textures.size())) {
			return false;
		}

		const auto& noise_texture_wave = scene_asset_manager.textures.at(index).noise_texture_wave;
		const auto& name = scene_asset_manager.texture_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		if ((uint32_t)noise_texture_wave.child_texture_indices.texture_black < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, noise_texture_wave.child_texture_indices.texture_black, texture_folder_path);
		}
		if ((uint32_t)noise_texture_wave.child_texture_indices.texture_white < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, noise_texture_wave.child_texture_indices.texture_white, texture_folder_path);
		}

		os << "Begin(NoiseTextureWave);" << std::endl;
		save_attribute_name(os, "name", name);
		if ((uint32_t)noise_texture_wave.child_texture_indices.texture_black < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "texture_black_idx", scene_asset_manager.texture_names.at(noise_texture_wave.child_texture_indices.texture_black));
		}
		if ((uint32_t)noise_texture_wave.child_texture_indices.texture_white < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "texture_white_idx", scene_asset_manager.texture_names.at(noise_texture_wave.child_texture_indices.texture_white));
		}
		save_attribute_float(os, "freq_0", noise_texture_wave.freq_0);
		save_attribute_float(os, "lacunarity_0", noise_texture_wave.lacunarity_0);
		save_attribute_float(os, "gain_0", noise_texture_wave.gain_0);
		save_attribute_int(os, "layer_count_0", noise_texture_wave.layer_count_0);
		save_attribute_float(os, "freq_1", noise_texture_wave.freq_1);
		save_attribute_float(os, "lacunarity_1", noise_texture_wave.lacunarity_1);
		save_attribute_float(os, "gain_1", noise_texture_wave.gain_1);
		save_attribute_int(os, "layer_count_1", noise_texture_wave.layer_count_1);
		os << "End(NoiseTextureWave);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}
	bool SceneParser::save_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		const auto& texture = scene_asset_manager.textures.at(index);
		if (texture.texture_type == TEXTURE_TYPE::IMAGE_TEXTURE) {
			return save_image_texture(os, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		else if (texture.texture_type == TEXTURE_TYPE::CONSTANT_TEXTURE_FLOAT) {
			return save_constant_float_texture(os, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		else if (texture.texture_type == TEXTURE_TYPE::CONSTANT_TEXTURE_RGB) {
			return save_constant_rgb_texture(os, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		else if (texture.texture_type == TEXTURE_TYPE::SOLID_TEXTURE_CHECKERBOARD) {
			return save_checker_board_texture(os, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		else if (texture.texture_type == TEXTURE_TYPE::SOLID_TEXTURE_NOISE) {
			return save_noise_texture(os, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		else if (texture.texture_type == TEXTURE_TYPE::SOLID_TEXTURE_FBM) {
			return save_noise_fbm_texture(os, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		else if (texture.texture_type == TEXTURE_TYPE::SOLID_TEXTURE_TURBULENCE) {
			return save_noise_turbulence_texture(os, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		else if (texture.texture_type == TEXTURE_TYPE::SOLID_TEXTURE_MARBLE) {
			return save_noise_marble_texture(os, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		else if (texture.texture_type == TEXTURE_TYPE::SOLID_TEXTURE_WOOD) {
			return save_noise_wood_texture(os, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		else if (texture.texture_type == TEXTURE_TYPE::SOLID_TEXTURE_POLKA_DOT) {
			return save_noise_polka_dot_texture(os, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		else if (texture.texture_type == TEXTURE_TYPE::SOLID_TEXTURE_WAVE) {
			return save_noise_wave_texture(os, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		else {
			return false;
		}
	}

	bool SceneParser::save_distant_light(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.distant_lights.size())) {
			return false;
		}

		const auto& distant_light = scene_asset_manager.distant_lights.at(index);
		const auto& name = scene_asset_manager.distant_light_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		save_transform(os, processed_name_set, scene_asset_manager, distant_light.transform_idx, texture_folder_path);

		os << "Begin(DistantLight);" << std::endl;
		save_attribute_name(os, "name", name);
		save_attribute_name(os, "transform_index", scene_asset_manager.transform_names.at(distant_light.transform_idx));
		save_attribute_vector(os, "light_color", distant_light.light_color);
		save_attribute_float(os, "intensity", distant_light.intensity);
		save_attribute_float(os, "theta_max", distant_light.theta_max);
		os << "End(DistantLight);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}

	bool SceneParser::save_volume(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path)
	{
		if (!(index < scene_asset_manager.volumes.size())) {
			return false;
		}

		const auto& volume = scene_asset_manager.volumes.at(index);
		const auto& name = scene_asset_manager.volume_names.at(index);

		if (processed_name_set.find(name) != processed_name_set.end()) {
			return false;
		}

		save_transform(os, processed_name_set, scene_asset_manager, volume.transform_idx, texture_folder_path);
		if ((uint32_t)volume.density_texture_idx < (uint32_t)scene_asset_manager.textures.size()) {
			save_texture(os, processed_name_set, scene_asset_manager, volume.density_texture_idx, texture_folder_path);
		}

		os << "Begin(Volume);" << std::endl;
		save_attribute_name(os, "name", name);
		save_attribute_name(os, "transform_index", scene_asset_manager.transform_names.at(volume.transform_idx));
		save_attribute_vector(os, "sigma_t", volume.sigma_t);
		save_attribute_vector(os, "albedo", volume.albedo);
		save_attribute_float(os, "g", volume.g);
		if ((uint32_t)volume.density_texture_idx < (uint32_t)scene_asset_manager.textures.size()) {
			save_attribute_name(os, "density_texture_index", scene_asset_manager.texture_names.at(volume.density_texture_idx));
		}
		os << "End(Volume);" << "\n" << std::endl;

		processed_name_set.insert(name);

		return true;
	}

	

	bool SceneParser::save_scene(const std::string& file_path, std::shared_ptr<YumeRT::SceneModule> scene_module) 
	{
		std::string _own_file_name = file_path;
		for (auto& c : _own_file_name) {
			if (c == '\\') {
				c = '/';
			}
		}

		const size_t dot_pos = _own_file_name.find('.');
		if (dot_pos == std::string::npos) {
			std::cerr << "Error: invalid file type!" << std::endl;
			return false;
		}

		const std::string postfix(_own_file_name.begin() + dot_pos, _own_file_name.end());
		if (postfix != dsk_file_postfix) {
			std::cerr << "Error: invalid file type!" << std::endl;
			return false;
		}

		std::filesystem::path scene_file_dir(_own_file_name);
		std::filesystem::path parent_dir(scene_file_dir.parent_path());
		if (!std::filesystem::is_directory(parent_dir)) {
			std::filesystem::create_directory(parent_dir);
		}

		std::ofstream ofs;
		ofs << std::fixed;
		ofs.open(_own_file_name);
		if (!ofs.is_open()) {
			std::cerr << "Error: Fail to save file, please check your path!" << std::endl;
			return false;
		}

		printf("Scene file save begin: %s\n", _own_file_name.c_str());
		fflush(stdout);
		const auto save_begin_time = std::chrono::steady_clock::now();

		std::unordered_set<std::string> processed_name_set;
		auto& scene_asset_manager = *scene_module;

		std::filesystem::path texture_folder_path;
		if (!scene_asset_manager.textures.empty()) {
			texture_folder_path = get_texture_folder_from_scene_file(scene_file_dir);

			if (!std::filesystem::is_directory(texture_folder_path)) {
				std::filesystem::create_directory(texture_folder_path);
			}
		}

		save_camera(ofs, processed_name_set, scene_asset_manager, texture_folder_path);

		const size_t primitive_instance_count = scene_asset_manager.primitive_instances.size();
		for (size_t primitive_index = 0; primitive_index < primitive_instance_count; ++primitive_index) {
			auto unique_index = scene_asset_manager.primitive_instances.at(primitive_index).unique_index;
			const std::string &instance_name = scene_asset_manager.primitive_index_to_name_map.at(unique_index);
			printf("Saving primitive instance (%zu/%zu): \"%s\"\n", primitive_index + 1, primitive_instance_count, instance_name.c_str());
			fflush(stdout);
			save_primitive_instance(ofs, processed_name_set, scene_asset_manager, unique_index, texture_folder_path);
		}

		for (size_t index = 0; index < scene_asset_manager.geometries.size(); ++index) {
			save_geometry(ofs, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		for (size_t index = 0; index < scene_asset_manager.transform_states.size(); ++index) {
			save_transform(ofs, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		for (size_t index = 0; index < scene_asset_manager.materials.size(); ++index) {
			save_material(ofs, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		for (size_t index = 0; index < scene_asset_manager.textures.size(); ++index) {
			save_texture(ofs, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		for (size_t index = 0; index < scene_asset_manager.distant_lights.size(); ++index) {
			save_distant_light(ofs, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}
		for (size_t index = 0; index < scene_asset_manager.volumes.size(); ++index) {
			save_volume(ofs, processed_name_set, scene_asset_manager, index, texture_folder_path);
		}

		ofs.close();

		{
			const double save_cost_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - save_begin_time).count();
			printf("Scene file save complete: %zu geometries, %zu transforms, %zu materials, %zu textures, %zu primitive instances, %zu distant lights, %zu volumes, cost %.2f s.\n",
				scene_asset_manager.geometries.size(), scene_asset_manager.transform_states.size(), scene_asset_manager.materials.size(), scene_asset_manager.textures.size(), scene_asset_manager.primitive_instances.size(), scene_asset_manager.distant_lights.size(), scene_asset_manager.volumes.size(), save_cost_seconds);
			fflush(stdout);
		}

		return true;
	}
};