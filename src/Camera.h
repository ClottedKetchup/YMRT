#pragma once

#include <driver_types.h>

#include "SceneDefines.h"
#include "Ray.h"
#include "MathCommon.h"

namespace YumeRT
{
#define CAMERA_COUNT 2
#define RENDERING_CAMERA_INDEX 0
#define EDITOR_CAMERA_INDEX 1

	struct Camera 
	{
	public:
		int m_width;
		int m_height;

		__device__ __host__ inline Camera(): m_width(1), m_height(1),
			position(0.0f), u(1.0f, 0.0f, 0.0f), v(0.0f, 1.0f, 0.0f), w(0.0f, 0.0f, 1.0f),
			aspect_ratio(1.0f), field_of_view(glm::radians(45.0f)), len_radius(1.0f), tan_half_fov(glm::tan(glm::radians(22.5f)))
		{

		}

		__device__ __host__ inline void SetDir(const glm::vec3 &dir)
		{
			w = dir;
			MakeCameraBasis();
		}

		__device__ __host__ inline glm::vec4 WorldToNDC(const glm::vec4 &p) const
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
			cp = Matrix_S(glm::vec3(inv_y_extent * (1.0f / aspect_ratio), inv_y_extent, 1.0f)) * GetPerspective() * cp;

			float inv_w = 1.0f / cp.w;
			cp.x *= inv_w, cp.y *= inv_w, cp.z *= inv_w;
			cp.x = cp.x * 0.5f + 0.5f;
			cp.y = cp.y * 0.5f + 0.5f;
			return glm::vec4(cp.x, cp.y, cp.z, 1.0f);
		}

		__device__ __host__ inline glm::vec4 TextureCoordinatesDifferential(const Ray &main_ray, 
							const glm::vec3 &hit_position, 
							const glm::vec3 &hit_shading_normal, 
							const glm::vec3 &hit_geometry_normal, 
							const glm::vec3 &hit_dpdu,
							const glm::vec3 &hit_dpdv,
							const int screen_width, 
							const int screen_height,
							const float scale_x = 1.0f,
							const float scale_y = 1.0f) const
		{
			glm::vec3 look_at = position - hit_position;
			const float distance = glm::length(look_at);
			look_at *= SafeRcp(distance);
			
			glm::vec3 right = glm::normalize(u - look_at * glm::dot(u, look_at));
			glm::vec3 up = glm::cross(look_at, right);
			
			const float image_plane_height = 2.0f * tan_half_fov;
			const float y_offset = image_plane_height * scale_y / float(screen_height);
			const float image_plane_width = image_plane_height * aspect_ratio;
			const float x_offset = image_plane_width * scale_x / float(screen_width);

			auto local_to_world = [&look_at, &right, &up](const glm::vec3 &v)->glm::vec3 
			{
				return v.x * right + v.y	* up + v.z * look_at;
			};
			auto world_to_local = [&look_at, &right, &up](const glm::vec3 &v)->glm::vec3
			{
				return glm::vec3(glm::dot(v, right), glm::dot(v, up), glm::dot(v, look_at));
			};
			auto min_abs = [](float f, float threshold)->float
			{
				return glm::abs(f) < threshold ? (f > 0.0f ? threshold : -threshold) : f;
			};

			glm::vec3 dx_direction = glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f) + right * x_offset);
			glm::vec3 dy_direction = glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f) + up * y_offset);
			glm::vec3 local_shading_normal = world_to_local(hit_shading_normal);
			glm::vec3 local_position = glm::vec3(0.0f, 0.0f, -distance);

			// rays shot from origin
			float tx = glm::dot(local_shading_normal, local_position) / min_abs(glm::dot(local_shading_normal, dx_direction), 1E-4f);
			glm::vec3 dpdx = (dx_direction * tx - local_position) * SafeRcp(scale_x);
			float ty = glm::dot(local_shading_normal, local_position) / min_abs(glm::dot(local_shading_normal, dy_direction), 1E-4f);
			glm::vec3 dpdy = (dy_direction * ty - local_position) * SafeRcp(scale_y);

			dpdx = local_to_world(dpdx);
			dpdy = local_to_world(dpdy);

			int dim[2];
			glm::vec3 abs_n = glm::abs(hit_geometry_normal);
			if (abs_n.z > abs_n.y && abs_n.z > abs_n.x)
			{
				dim[0] = 0, dim[1] = 1;
			}
			else if (abs_n.y > abs_n.x)
			{
				dim[0] = 0, dim[1] = 2;
			}
			else
			{
				dim[0] = 1, dim[1] = 2;
			}

			float dudx, dudy, dvdx, dvdy;
			float m00 = hit_dpdu[dim[0]];
			float m01 = hit_dpdv[dim[0]];
			float m10 = hit_dpdu[dim[1]];
			float m11 = hit_dpdv[dim[1]];
			float determinant = m00 * m11 - m10 * m01;
			if (glm::abs(determinant) < 1E-7f)
			{
				return glm::vec4(0.0f);
			}
			else
			{
				const float i_det = 1.0f / determinant;
				dudx = m11 * dpdx[dim[0]] - m01 * dpdx[dim[1]];
				dvdx = -m10 * dpdx[dim[0]] + m00 * dpdx[dim[1]];
				dudy = m11 * dpdy[dim[0]] - m01 * dpdy[dim[1]];
				dvdy = -m10 * dpdy[dim[0]] + m00 * dpdy[dim[1]];
				return glm::vec4(dudx, dvdx, dudy, dvdy) * i_det;
			}
		}

		__device__ __host__ inline glm::vec3 NDCToImagePlane(const glm::vec2 &p)
		{
			float x = p.x * tan_half_fov * aspect_ratio;
			float y = p.y * tan_half_fov;
			return -w + x * u + y * v + position;
		}

		__device__ __host__ inline glm::vec4 WorldToCamera(const glm::vec4 &p) const
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

		__device__ __host__ inline Ray GenerateRay(float ndc_x, float ndc_y) const
		{
			float height = 2.0f * tan_half_fov;
			float width = height * aspect_ratio;
			glm::vec3 dir = -w + (ndc_x - 0.5f) * width * u + (ndc_y - 0.5f) * height * v;
			return Ray(position, dir);
		}

		__device__ __host__ inline RayDifferential GenerateRayDifferential(const Ray& main_ray, 
			float ndc_x, float ndc_y, 
			int screen_width, int screen_height, 
			float scale_x = 1.0f, float scale_y = 1.0f) const
		{
			float height = 2.0f * tan_half_fov;
			float pixel_spacing_y = 1.0f / float(screen_height);
			float width = height * aspect_ratio;
			float pixel_spacing_x = 1.0f / float(screen_width);
			
			RayDifferential ray_differential;
			ray_differential.direction_x = glm::normalize(-w + (ndc_x - 0.5f + pixel_spacing_x * scale_x) * width * u + (ndc_y - 0.5f) * height * v);
			ray_differential.origin_x = main_ray.origin;
			ray_differential.scale_x = glm::max(0.0001f, scale_x);
			ray_differential.direction_y = glm::normalize(-w + (ndc_x - 0.5f) * width * u + (ndc_y - 0.5f + pixel_spacing_y * scale_y) * height * v);
			ray_differential.origin_y = main_ray.origin;
			ray_differential.scale_y = glm::max(0.0001f, scale_y);

			return ray_differential;
		}

		__device__ __host__ inline const RayTransfer& GetRayTransfer() const {
			return camera_ray_transfer;
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

		__host__  void InitCameraRayTransfer(const Scene& scene);

	private:

		glm::vec3 position;
		glm::vec3 u;
		glm::vec3 v;
		glm::vec3 w;

		float aspect_ratio;
		float field_of_view;
		float len_radius;
		float tan_half_fov;

		

		RayTransfer camera_ray_transfer;

		__device__ __host__ inline glm::mat4 GetPerspective() const
		{
			float z_near = 1.0f, z_far = 100.f;

			glm::mat4 perspective;

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

			return perspective;
		}

		__device__ __host__ inline void MakeCameraBasis()
		{
			w = w * SafeRcp(glm::length(w));
			
			double m_u[3];
			DoubleCross(WORLD_UP, w, m_u);
			u = glm::vec3(m_u[0], m_u[1], m_u[2]) * SafeRcp(glm::sqrt(m_u[0] * m_u[0] + m_u[1] * m_u[1] + m_u[2] * m_u[2]));

			double m_v[3];
			DoubleCross(w, u, m_v);
			v = glm::vec3(m_v[0], m_v[1], m_v[2]) * SafeRcp(glm::sqrt(m_v[0] * m_v[0] + m_v[1] * m_v[1] + m_v[2] * m_v[2]));
		}
	};

	__device__ __host__ inline bool IsCameraEqual(const Camera &cam_a, const Camera &cam_b)
	{
		return cam_a.GetPosition() == cam_b.GetPosition() &&
				cam_a.GetW() == cam_b.GetW() &&
				cam_a.GetAspectRatio() == cam_b.GetAspectRatio() &&
				cam_a.GetFov() == cam_b.GetFov() &&
				cam_a.GetLensRadius() == cam_b.GetLensRadius();
	}
};