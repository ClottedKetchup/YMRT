#pragma once

#include <driver_types.h>

#include "Ray.h"
#include "MathCommon.h"

namespace YumeRT
{
	struct Camera 
	{
	public:
		__device__ __host__ Camera() 
		{
			InitPerspective();
		}

		__device__ __host__ inline void SetDir(const glm::vec3 &dir)
		{
			w = dir;
			MakeCameraBasis();
		}

		__device__ __host__ inline glm::vec4 WorldToNDC(const glm::vec4 &p)
		{
			assert(p.w != 0.0f);
			float inv_y_extent = 1.0f / tan_half_fov;
			glm::vec4 cp = WorldToCamera(p);
			if (glm::abs(cp.z) < 1E-7f)
			{
				return glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
			}
			if (cp.z > 0.0f) 
			{
				cp.x = -cp.x, cp.y = -cp.y, cp.z = -cp.z;
			}
			cp = Matrix_S(glm::vec3(inv_y_extent * aspect_ratio, inv_y_extent, 1.0f)) * perspective * cp;

			float inv_w = 1.0f / cp.w;
			cp.x *= inv_w, cp.y *= inv_w, cp.z *= inv_w;
			cp.x = cp.x * 0.5f + 0.5f;
			cp.y = cp.y * 0.5f + 0.5f;
			return glm::vec4(cp.x, cp.y, cp.z, 1.0f);
		}

		__device__ __host__ inline glm::vec3 NDCToImagePlane(const glm::vec2 &p)
		{
			float x = p.x * tan_half_fov * aspect_ratio;
			float y = p.y * tan_half_fov;
			return -w + x * u + y * v + position;
		}

		__device__ __host__ inline glm::vec4 WorldToCamera(const glm::vec4 &p)
		{
			glm::vec3 cp = glm::vec3(p.x, p.y, p.z);
			cp -= p.w * position;
			float x = dot(u, cp);
			float y = dot(v, cp);
			float z = dot(w, cp);
			return glm::vec4(x, y, z, p.w);
		}

		__device__ __host__ inline glm::vec4 CameraToWorld(const glm::vec4 &p)
		{
			return glm::vec4(p.x * u + p.y * v + p.z * w + p.w * position, p.w);
		}

		__device__ __host__ inline Ray generateRay(float ndc_x, float ndc_y)
		{
			float height = 2.0f * tan_half_fov;
			float width = height * aspect_ratio;
			glm::vec3 dir = -w + (ndc_x - 0.5f) * width * u + (ndc_y - 0.5f) * height * v;
			return Ray(position, dir, camera_ior, volume_idx);
		}

		__device__ __host__ inline glm::vec3 GetU() const { return u; }

		__device__ __host__ inline glm::vec3 GetV() const { return v; }

		__device__ __host__ inline glm::vec3 GetW() const { return w; }

		__device__ __host__ inline glm::vec3 GetPosition() const
		{
			return position;
		}

		__device__ __host__ inline void SetPosition(const glm::vec3 &pos)
		{
			position = pos;
		}

		__device__ __host__ inline float GetFov() const 
		{
			return field_of_view;
		}

		__device__ __host__ inline void SetFov(float fov)
		{
			field_of_view = fov;
			tan_half_fov = glm::tan(0.5f * field_of_view);
		}

		__device__ __host__ inline float GetLensRadius() const
		{
			return len_radius;
		}

		__device__ __host__ inline void SetLensRadius(float r)
		{
			len_radius = r;
		}

		__device__ __host__ inline float GetIOR() const
		{
			return camera_ior;
		}

		__device__ __host__ inline void SetIOR(float refractive_index)
		{
			camera_ior = refractive_index;
		}

		__device__ __host__ inline float GetVolumeIndex() const
		{
			return volume_idx;
		}

		__device__ __host__ inline void SetVolumeIndex(int idx)
		{
			volume_idx = idx;
		}

		__device__ __host__ inline float GetAspectRatio() const
		{
			return aspect_ratio;
		}

		__device__ __host__ inline void SetAspectRatio(float ratio)
		{
			aspect_ratio = ratio;
		}

		__device__ __host__ inline void MoveForward(float speed)
		{
			position -= w * speed;
		}

		__device__ __host__ inline void MoveBackward(float speed)
		{
			position += w * speed;
		}

		__device__ __host__ inline void MoveLeft(float speed)
		{
			position -= u * speed;
		}

		__device__ __host__ inline void MoveRight(float speed)
		{
			position += u * speed;
		}
		
	private:

		glm::vec3 position;
		glm::vec3 u;
		glm::vec3 v;
		glm::vec3 w;

		float aspect_ratio;
		float field_of_view;
		float len_radius;
		float tan_half_fov;

		float camera_ior = 1.0f;
		int volume_idx = -1;

		glm::mat4 perspective;


		__device__ __host__ inline void InitPerspective()
		{
			float z_near = 1.0f, z_far = 100.f;

			perspective[0][0] = 1.0f;
			perspective[0][1] = 0.0f;
			perspective[0][2] = 0.0f;
			perspective[0][3] = 0.0f;

			perspective[1][0] = 0.0f;
			perspective[1][1] = 1.0f;
			perspective[1][2] = 0.0f;
			perspective[1][3] = 0.0f;

			perspective[2][0] = 0.0f;
			perspective[2][1] = 0.0f;
			perspective[2][2] = z_far / (z_near - z_far);
			perspective[2][3] = -1.0f;

			perspective[3][0] = 0.0f;
			perspective[3][1] = 0.0f;
			perspective[3][2] = (z_near * z_far) / (z_near - z_far);
			perspective[3][3] = 0.0f;
		}

		__device__ __host__ inline void MakeCameraBasis()
		{
			w = glm::normalize(w);
			u = glm::cross(WORLD_UP, w);
			v = glm::cross(w, u);
		}
	};

	__device__ __host__ inline bool IsCameraEqual(const Camera &cam_a, const Camera &cam_b)
	{
		return cam_a.GetPosition() == cam_b.GetPosition() &&
				cam_a.GetW() == cam_b.GetW() &&
				cam_a.GetAspectRatio() == cam_b.GetAspectRatio() &&
				cam_a.GetFov() == cam_b.GetFov() &&
				cam_a.GetLensRadius() == cam_b.GetLensRadius() &&
				cam_a.GetIOR() == cam_b.GetIOR() &&
				cam_a.GetVolumeIndex() == cam_b.GetVolumeIndex();
	}
};