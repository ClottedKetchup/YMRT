#pragma once

#include <cuda_runtime.h>
#include <cuda_profiler_api.h>
#include <driver_types.h>

#include <glad/glad.h>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace YumeRT {

#define CUDA_CHECK(call) YumeRT::CudaCheck(call, #call, __FILE__, __LINE__)
#define GL_CHECK(call) { \
	call; \
	YumeRT::GlCheck(#call, __FILE__, __LINE__); \
} \

// note: this function will execute on the cuda default stream.
// would allocate space.
#define UPLOAD_TO_GPU(device_ptr, host_ptr, byte_size) \
{ \
	if(byte_size > 0) \
	{ \
		CUDA_CHECK(cudaMalloc(&device_ptr, byte_size)); \
		CUDA_CHECK(cudaMemcpy(device_ptr, host_ptr, byte_size, cudaMemcpyHostToDevice)); \
	} \
} \

// not allocate space
#define TRANSFER_TO_GPU(device_ptr, host_ptr, byte_size) \
{ \
	if(byte_size > 0) \
	{ \
		assert(host_ptr != nullptr && device_ptr != nullptr); \
		CUDA_CHECK(cudaMemcpy(device_ptr, host_ptr, byte_size, cudaMemcpyHostToDevice)); \
	} \
} \

#define FREE_GPU_RESOURCE(device_ptr) \
{ \
	if(device_ptr != nullptr) \
	{ \
		CUDA_CHECK(cudaFree(device_ptr)); \
		device_ptr = nullptr; \
	} \
} \

#define FREE_STL(STL) \
{ \
	STL = {}; \
} \

	inline void CudaCheck(cudaError_t error, const char* call, const char* file, unsigned int line)
	{
		if (error != cudaSuccess)
		{
			std::stringstream ss;
			ss << "CUDA call ( " << call << " ) failed with error: '"
				<< cudaGetErrorString(error) << "' (" << file << ":" << line << ")\n";
			std::cerr << ss.str() << std::endl;
			throw std::runtime_error(ss.str().c_str());
		}
	}

	inline const char* GetGLErrorString(GLenum error)
	{
		switch (error)
		{
		case GL_NO_ERROR:
			return "No error";
		case GL_INVALID_ENUM:
			return "Invalid enum";
		case GL_INVALID_VALUE:
			return "Invalid value";
		case GL_INVALID_OPERATION:
			return "Invalid operation";
		case GL_OUT_OF_MEMORY:
			return "Out of memory";
		default:
			return "Unknown GL error";
		}
	}

	inline void GlCheck(const char* call, const char* file, unsigned int line)
	{
		GLenum err = glGetError();
		if (err != GL_NO_ERROR)
		{
			std::stringstream ss;
			ss << "GL error " << GetGLErrorString(err) << " at " << file << "("
				<< line << "): " << call << "\n";
			std::cerr << ss.str() << std::endl;
			throw std::runtime_error(ss.str().c_str());
		}
	}
}