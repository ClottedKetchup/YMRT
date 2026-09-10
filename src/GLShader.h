#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <string>
#include <fstream>
#include <sstream>

#include <iostream>

namespace YMRT
{
	class GLVertexArray
	{
	public:
		GLVertexArray() = default;
		~GLVertexArray() 
		{
			if (vao)
			{
				glDeleteVertexArrays(1, &vao);
			}
		}

		inline void InitVAO() 
		{
			if (!vao) 
			{
				glGenVertexArrays(1, &vao);
			}
		}

		inline void Bind()
		{
			glBindVertexArray(vao);
		}

		inline void UnBind()
		{
			glBindVertexArray(0);
		}

	private:
		GLuint vao = 0;
	};

	class GLShader
	{
	public:
		GLShader() = default;
		~GLShader()
		{
			if (program)
			{
				glDeleteProgram(program);
			}
		}

		inline void InitShader(const std::string &vs, const std::string &fs)
		{
			vert = CompileShader(vs, VERTEX);
			frag = CompileShader(fs, FRAGMENT);

			program = glCreateProgram();
			glAttachShader(program, vert);
			glAttachShader(program, frag);
			glLinkProgram(program);

			glDeleteShader(vert);
			glDeleteShader(frag);
		}
		
		inline void Use()
		{
			glUseProgram(program);
		}

		GLuint program;
		GLuint vert;
		GLuint frag;

	public:
		enum ShaderType
		{
			VERTEX, FRAGMENT
		};
		inline GLuint CompileShader(const std::string& shader_name, ShaderType shader_type)
		{
			std::ifstream fs;
			std::stringstream ss;
			std::string s;
			const char* source = nullptr;
			fs.exceptions(std::ios::failbit | std::ios::badbit);
			try
			{
				fs.open(shader_name, std::ios::binary | std::ios::in);
				ss << fs.rdbuf();
				fs.close();
				s = ss.str();
				source = s.c_str();
			}
			catch (std::ifstream::failure e)
			{
				switch (shader_type)
				{
				case VERTEX:
					std::cout << "Fail to load vertex shader!" << std::endl;
					break;
				case FRAGMENT:
					std::cout << "Fail to load fragment shader!" << std::endl;
					break;
				default:
					break;
				}

			}

			GLuint subshader = 0;

			switch (shader_type)
			{
			case VERTEX:
			{
				subshader = glCreateShader(GL_VERTEX_SHADER);
				break;
			}
			case FRAGMENT:
			{
				subshader = glCreateShader(GL_FRAGMENT_SHADER);
				break;
			}
			default:
				break;
			}

			GLchar infoLog[512];
			GLint success;
			glShaderSource(subshader, 1, &source, nullptr);
			glCompileShader(subshader);
			glGetShaderiv(subshader, GL_COMPILE_STATUS, &success);
			if (!success)
			{
				glGetShaderInfoLog(subshader, 512, nullptr, infoLog);

				switch (shader_type)
				{
				case VERTEX:
					std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED" << std::endl;
					break;
				case FRAGMENT:
					std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED" << std::endl;
					break;
				default:
					break;
				}

				std::cout << infoLog << std::endl;

				return 0;
			}
			return subshader;
		}
	};
};