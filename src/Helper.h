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
do \
{ \
	const size_t upload_byte_size = (byte_size); \
	if(upload_byte_size > 0) \
	{ \
		CUDA_CHECK(cudaMalloc(&(device_ptr), upload_byte_size)); \
		CUDA_CHECK(cudaMemcpy((device_ptr), (host_ptr), upload_byte_size, cudaMemcpyHostToDevice)); \
	} \
} while(0)

// not allocate space
#define TRANSFER_TO_GPU(device_ptr, host_ptr, byte_size) \
do \
{ \
	const auto transfer_device_pointer = (device_ptr); \
	const auto transfer_host_pointer = (host_ptr); \
	const size_t transfer_byte_size = (byte_size); \
	if(transfer_byte_size > 0) \
	{ \
		assert(transfer_host_pointer != nullptr && transfer_device_pointer != nullptr); \
		CUDA_CHECK(cudaMemcpy(transfer_device_pointer, transfer_host_pointer, transfer_byte_size, cudaMemcpyHostToDevice)); \
	} \
} while(0)

// not allocate space
#define DOWNLOAD_FROM_GPU(device_ptr, host_ptr, byte_size) \
do \
{ \
	const auto download_device_pointer = (device_ptr); \
	const auto download_host_pointer = (host_ptr); \
	const size_t download_byte_size = (byte_size); \
	if(download_byte_size > 0) \
	{ \
		assert(download_host_pointer != nullptr && download_device_pointer != nullptr); \
		CUDA_CHECK(cudaMemcpy(download_host_pointer, download_device_pointer, download_byte_size, cudaMemcpyDeviceToHost)); \
	} \
} while(0)

#ifndef NDEBUG
	#define PRINT_GPU_FREE_MEMORY(memory_status) \
	do \
	{ \
		size_t free_memory = 0, total_memory = 0; \
		cudaMemGetInfo(&free_memory, &total_memory); \
		printf("GPU free memory %s: %zu MB (%s:%d)\n", memory_status, free_memory / (1024 * 1024), __FILE__, __LINE__); \
		fflush(stdout); \
	} while(0)
#else
	#define PRINT_GPU_FREE_MEMORY(memory_status)
#endif

#define FREE_GPU_RESOURCE(device_ptr) \
do \
{ \
	if((device_ptr) != nullptr) \
	{ \
		CUDA_CHECK(cudaFree((device_ptr))); \
		(device_ptr) = nullptr; \
	} \
} while(0)

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