#pragma once

#include <driver_types.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace YMRT 
{
#define ONE_PI 3.14159265358f
#define TWO_PI 6.28318530716f
#define HALF_PI 1.57079632679f
#define INV_PI 0.318309886183f

#define FLOAT_EPSILON 1.569209E-07f // note: machine epsilon is 1.19209E-07
#define MIN_COLOR_EPSILON 3.7252903E-9f
#define MAX_COLOR_CLAMP 1E16f
#define SHADOW_RAY_CLAMP 0.9995f

#define WORLD_UP glm::vec3(0.0f, 1.0f, 0.0f)

#define YMRT_FLOAT_MAX  1E36f
#define YMRT_FLOAT_MIN  -1E36f

#define EMPTY_UINT32 0xFFFFFFFF
#define EMPTY_UINT16 0xFFFF

#define STAGE_RAY_GEN_BLOCK_SIZE 256
#define STAGE_RAY_TRACE_BLOCK_SIZE 128
#define STAGE_RAY_SHADING_BLOCK_SIZE 128

#define Matrix_T(t) (glm::translate(glm::mat4(1.0f), t))
#define Matrix_S(s) (glm::scale(glm::mat4(1.0f), s))
#define Matrix_R(axis, angle) (glm::rotate(glm::mat4(1.0f), angle, axis))

#define Round_Block_Count(Total_Thread_Count, Block_Thread_Count) (((Block_Thread_Count) + (Total_Thread_Count) - 1) / (Block_Thread_Count))

#define VAR_NAME(var) (#var)

#define PRINT_FLOAT(f) printf("%s: %.4f\n", #f, f)
#define PRINT_INT(d) printf("%s: %d\n", #d, d)
#define PRINT_FLOAT3(v) printf("%s: [%.7f, %.7f, %.7f]\n", #v, v.x, v.y, v.z)

	template<typename T>
	__device__ __host__ inline void Swap(T &a, T &b)
	{
		T temp = a;
		a = b;
		b = temp;
	}

	template<typename T>
	__device__ __host__ inline T Sqr(const T &x)
	{
		return x * x;
	}

	template<typename T>
	__device__ __host__ inline T LinearLerp(const T& x, const T& y, float weight)
	{
		return (1.0f - weight) * x + y * weight;
	}

	__device__ __host__ inline int MaxDimension(const glm::vec3 &v)
	{
		int max_d = 0;
		if (v[1] > v[max_d]) { max_d = 1; }
		if (v[2] > v[max_d]) { max_d = 2; }
		return max_d;
	}

	__device__ __host__ inline float MaxComponent(const glm::vec3 &v)
	{
		return glm::max(v.x, glm::max(v.y, v.z));
	}

	__device__ __host__ inline glm::vec3 Permute(const glm::vec3 &p, int x, int y, int z)
	{
		return glm::vec3(p[x], p[y], p[z]);
	}

	__device__ __host__ inline uint32_t FloatToUint(float f)
	{
		return *((uint32_t*)(&f));
	}

	__device__ __host__ inline float UintToFloat(uint32_t i)
	{
		return *((float*)(&i));
	}

	__device__ __host__ inline int FloatToInt(float f)
	{
		return *((int*)(&f));
	}

	__device__ __host__ inline float IntToFloat(int i)
	{
		return *((float*)(&i));
	}

	__device__ __host__ inline float SafeRcp(float f)
	{
		return glm::abs(f) < 1E-30f ? (f > 0.0f ? 1E30f : -1E30f) : (1.0f / f);
	}

	__device__ __host__ inline glm::vec3 SafeRcp(const glm::vec3 &vec)
	{
		return glm::vec3(SafeRcp(vec.x), SafeRcp(vec.y), SafeRcp(vec.z));
	}

	 __device__ __host__ inline void OrthogonalBasis(glm::vec3 &N, glm::vec3 &T, glm::vec3 &B)
	{
		N = glm::normalize(N);
		if (glm::abs(N.z) > glm::abs(N.x))
		{
			float l = glm::sqrt(glm::max(1.f - N.x * N.x, 0.0f));
			float r = 1.f / l;
			B = glm::vec3(0.f, N.z * r, -N.y * r);
		}
		else
		{
			float l = glm::sqrt(glm::max(1.f - N.z * N.z, 0.0f));
			float r = 1.f / l;
			B = glm::vec3(N.y * r, -N.x * r, 0.f);
		}
		T = glm::cross(B, N);
	}

	 __device__ __host__ inline size_t AlignedMemorySize(size_t memory_required, size_t alignment)
	 {
		 return ((memory_required + (alignment - 1)) & (~(alignment - 1)));
	 }

	 __device__ __host__ inline float SRGBToLinear(float s)
	 {
		 return s > 0.04045 ? glm::pow((s + 0.055) / 1.055, 2.4f) : (s / 12.92f);
	 }

	 __device__ __host__ inline float LinearToSRGB(float l)
	 {
		 return l > 0.0031308f ? (1.055f * glm::pow(l, 1.0f / 2.4f) - 0.055f) : l * 12.92f;
	 }

	 __device__ __host__ inline float RGBToLuminance(const glm::vec3 &rgb) 
	 {
		 return 0.2126f * rgb.r + 0.7152 * rgb.g + 0.0722f * rgb.b;
	 }

	 __device__ __host__ inline constexpr float ErrorGamma(int n)
	 {
		 return (n * FLOAT_EPSILON) / (1.0f - n * FLOAT_EPSILON);
	 }

	 __device__ __host__ inline double DoubleDot(const glm::vec3 &v0, const glm::vec3 &v1)
	 {
		 return (double)v0.x * (double)v1.x + (double)v0.y * (double)v1.y + (double)v0.z * (double)v1.z;
	 }

	 __device__ __host__ inline void DoubleCross(const glm::vec3& v0, const glm::vec3& v1, double result[]) {
		 result[0] = (double)v0.y * (double)v1.z - (double)v0.z * (double)v1.y;
		 result[1] = -((double)v0.x * (double)v1.z - (double)v0.z * (double)v1.x);
		 result[2] = (double)v0.x * (double)v1.y - (double)v0.y * (double)v1.x;
	 }

	 //! Computes a * b - c * d with at most 1.5 ulps of error in the result. See
//! https://pharr.org/matt/blog/2019/11/03/difference-of-floats.html or
//! Claude-Pierre Jeannerod, Nicolas Louvet and Jean-Michel Muller, 2013,
//! Further analysis of Kahan's algorithm for the accurate computation of 2x2
//! determinants, AMS Mathematics of Computation 82:284,
//! https://doi.org/10.1090/S0025-5718-2013-02679-8
	 __device__ __host__ inline float Kahan(float a, float b, float c, float d) {
		 // Uncomment the line below to improve efficiency but reduce accuracy
		 // return a * b - c * d;
		 float cd = c * d;
		 float error = fma(c, d, -cd);
		 float result = fma(a, b, -cd);
		 return result - error;
	 }


	 //! Implements a cross product using Kahan's algorithm for every single entry,
	 //! i.e. the error in each output entry is at most 1.5 ulps
	 __device__ __host__ inline glm::vec3 CrossStable(glm::vec3 lhs, glm::vec3 rhs) {
		 return glm::vec3(
			 Kahan(lhs.y, rhs.z, lhs.z, rhs.y),
			 Kahan(lhs.z, rhs.x, lhs.x, rhs.z),
			 Kahan(lhs.x, rhs.y, lhs.y, rhs.x)
		 );
	 }

	 __device__ __host__ inline glm::vec3 GetTranslate(const glm::mat4 &m)
	 {
		 return glm::vec3(m[3][0], m[3][1], m[3][2]);
	 }

	 __device__ __host__ inline glm::vec3 TransformVector(const glm::mat4 &m, const glm::vec3 &v)
	 {
		 glm::vec3 result_v;
		 result_v[0] = m[0][0] * v[0] + m[1][0] * v[1] + m[2][0] * v[2];
		 result_v[1] = m[0][1] * v[0] + m[1][1] * v[1] + m[2][1] * v[2];
		 result_v[2] = m[0][2] * v[0] + m[1][2] * v[1] + m[2][2] * v[2];
		 return result_v;
	 }

	 __device__ __host__ inline glm::vec3 TransposeTransformVector(const glm::mat4 &m, const glm::vec3 &v)
	 {
		 glm::vec3 result_v;
		 result_v[0] = m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2];
		 result_v[1] = m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2];
		 result_v[2] = m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2];
		 return result_v;
	 }

	 __device__ __host__ inline glm::vec3 TransformPosition(const glm::mat4 &m, const glm::vec3 &p, glm::vec3 *p_error = nullptr)
	 {
		 glm::vec3 result_p;
		 result_p[0] = m[0][0] * p[0] + m[1][0] * p[1] + m[2][0] * p[2] + m[3][0];
		 result_p[1] = m[0][1] * p[0] + m[1][1] * p[1] + m[2][1] * p[2] + m[3][1];
		 result_p[2] = m[0][2] * p[0] + m[1][2] * p[1] + m[2][2] * p[2] + m[3][2];

		 if (p_error != nullptr) {
			 const float x_abs_sum = glm::abs(m[0][0] * p[0]) + glm::abs(m[1][0] * p[1]) + glm::abs(m[2][0] * p[2]) + glm::abs(m[3][0]);
			 const float y_abs_sum = glm::abs(m[0][1] * p[0]) + glm::abs(m[1][1] * p[1]) + glm::abs(m[2][1] * p[2]) + glm::abs(m[3][1]);
			 const float z_abs_sum = glm::abs(m[0][2] * p[0]) + glm::abs(m[1][2] * p[1]) + glm::abs(m[2][2] * p[2]) + glm::abs(m[3][2]);
			 const float x_epsilon = glm::abs(m[0][0]) * (*p_error).x + glm::abs(m[1][0]) * (*p_error).y + glm::abs(m[2][0]) * (*p_error).z;
			 const float y_epsilon = glm::abs(m[0][1]) * (*p_error).x + glm::abs(m[1][1]) * (*p_error).y + glm::abs(m[2][1]) * (*p_error).z;
			 const float z_epsilon = glm::abs(m[0][2]) * (*p_error).x + glm::abs(m[1][2]) * (*p_error).y + glm::abs(m[2][2]) * (*p_error).z;
			 *p_error = (ErrorGamma(3) + 1.0f) * glm::vec3(x_epsilon, y_epsilon, z_epsilon)
				 + ErrorGamma(3) * glm::vec3(x_abs_sum, y_abs_sum, z_abs_sum);
		 }
		 return result_p;
	 }

	 __device__ __host__ inline glm::vec3 TransposeTransformPosition(const glm::mat4 &m, const glm::vec3 &p, glm::vec3 *p_error = nullptr)
	 {
		 glm::vec3 result_p;
		 result_p[0] = m[0][0] * p[0] + m[0][1] * p[1] + m[0][2] * p[2] + m[0][3];
		 result_p[1] = m[1][0] * p[0] + m[1][1] * p[1] + m[1][2] * p[2] + m[1][3];
		 result_p[2] = m[2][0] * p[0] + m[2][1] * p[1] + m[2][2] * p[2] + m[2][3];

		 if (p_error != nullptr) {
			 const float x_abs_sum = glm::abs(m[0][0] * p[0]) + glm::abs(m[0][1] * p[1]) + glm::abs(m[0][2] * p[2]) + glm::abs(m[0][3]);
			 const float y_abs_sum = glm::abs(m[1][0] * p[0]) + glm::abs(m[1][1] * p[1]) + glm::abs(m[1][2] * p[2]) + glm::abs(m[1][3]);
			 const float z_abs_sum = glm::abs(m[2][0] * p[0]) + glm::abs(m[2][1] * p[1]) + glm::abs(m[2][2] * p[2]) + glm::abs(m[2][3]);
			 const float x_epsilon = glm::abs(m[0][0]) * (*p_error).x + glm::abs(m[0][1]) * (*p_error).y + glm::abs(m[0][2]) * (*p_error).z;
			 const float y_epsilon = glm::abs(m[1][0]) * (*p_error).x + glm::abs(m[1][1]) * (*p_error).y + glm::abs(m[1][2]) * (*p_error).z;
			 const float z_epsilon = glm::abs(m[2][0]) * (*p_error).x + glm::abs(m[2][1]) * (*p_error).y + glm::abs(m[2][2]) * (*p_error).z;
			 *p_error = (ErrorGamma(3) + 1.0f) * glm::vec3(x_epsilon, y_epsilon, z_epsilon)
				 + ErrorGamma(3) * glm::vec3(x_abs_sum, y_abs_sum, z_abs_sum);
		 }
		 return result_p;
	 }

	 

	 __device__ __host__ inline float ShinnessToRoughness(float s) {
		return glm::sqrt(glm::sqrt(glm::max(2.0f / (s + 2.0f), 0.0f)));
	 }

	 // method is got by learning pbrt
	 __device__ __host__ inline float NextFloatUp(float f)
	 {
		 if (isinf(f) && f > 0.0f) {
			 return f;
		 }
		 if (f == -0.0f) {
			 f = 0.0f;
		 }
		 uint32_t ui = FloatToUint(f);
		 if (f >= 0.0f) {
			 ++ui;
		 }
		 else {
			 --ui;
		 }
		 return UintToFloat(ui);
	 }

	 __device__ __host__ inline float NextFloatDown(float f)
	 {
		 if (isinf(f) && f < 0.0f) {
			 return f;
		 }
		 if (f == 0.0f) {
			 f = -0.0f;
		 }
		 uint32_t ui = FloatToUint(f);
		 if (f <= 0.0f) {
			 ++ui;
		 }
		 else {
			 --ui;
		 }
		 return UintToFloat(ui);
	 }
};
