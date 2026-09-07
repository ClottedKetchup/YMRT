#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cctype>
#include <iomanip>
#include <filesystem>

#include "core/SceneModule.h"

namespace YumeRT {
	enum STATEMENT_TYPE
	{
		NONE = 0,
		TYPE_BRACKET = 1,
		ATTRIBUTE_ASSIGN = 2,
	};

	struct StatementToken
	{
		STATEMENT_TYPE type;
		size_t line;
		std::string left_line;
		std::string right_line;

		StatementToken(const STATEMENT_TYPE type, const size_t line, const std::string& s_left, const std::string& s_right) : type(type), line(line), left_line(s_left), right_line(s_right) {}
	};

	struct SceneParser
	{
	public:
		SceneParser() = default;
		bool parsing_scene(const std::string& file_path, std::shared_ptr<YumeRT::SceneModule> scene_module);
		bool save_scene(const std::string& file_path, std::shared_ptr<YumeRT::SceneModule> scene_module);

	private:

		bool save_camera(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const std::filesystem::path& texture_folder_path);

		bool save_mesh(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
		bool save_cube(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
		bool save_sphere(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
		bool save_geometry(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);

		bool save_transform(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);

		bool save_default_material(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
		bool save_light_material(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
		bool save_material(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);

		bool save_primitive_instance(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t unique_index, const std::filesystem::path& texture_folder_path);

		bool save_image_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
		bool save_constant_float_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
		bool save_constant_rgb_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
		bool save_checker_board_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
		bool save_noise_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
		bool save_noise_fbm_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
		bool save_noise_turbulence_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
		bool save_noise_marble_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
		bool save_noise_wood_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
		bool save_noise_polka_dot_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
		bool save_noise_wave_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
		bool save_texture(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);

		bool save_distant_light(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);

		bool save_volume(std::ofstream& os, std::unordered_set<std::string>& processed_name_set, SceneModule& scene_asset_manager, const uint32_t index, const std::filesystem::path& texture_folder_path);
	};

	
};