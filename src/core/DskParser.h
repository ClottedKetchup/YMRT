#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cctype>

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

	bool parsing_scene(const std::string &file_path, std::shared_ptr<YumeRT::SceneModule> scene_module);
};