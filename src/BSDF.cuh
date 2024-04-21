#pragma once

#include <cuda_runtime.h>
#include <vector_types.h>
#include <driver_types.h>

#include "MathCommon.h"
#include "SampleUtilities.h"
#include "SceneDefines.h"
#include "Material.h"

#include "Texture.h"
#include "TextureEval.cuh"
#include "Ray.h"

namespace YumeRT
{
#define MAX_BSDF_COUNT 4

	__device__ __host__ inline float CosTheta2(const glm::vec3 &w) { return w.z * w.z; }
	__device__ __host__ inline float CosTheta(const glm::vec3 &w) { return w.z; }
	__device__ __host__ inline float SinTheta2(const glm::vec3 &w) 
	{ 
		return glm::max(1.0f - CosTheta2(w), 0.0f);
	}
	__device__ __host__ inline float SinTheta(const glm::vec3 &w)
	{
		return glm::sqrt(SinTheta2(w));
	}
	__device__ __host__ inline float TanTheta2(const glm::vec3 &w)
	{
		return SinTheta2(w) / glm::max(CosTheta2(w), 1E-8f);
	}
	__device__ __host__ inline float TanTheta(const glm::vec3 &w)
	{
		return SinTheta(w) / glm::max(CosTheta(w), 1E-8f);
	}
	__device__ __host__ inline float CosPhi(const glm::vec3 &w)
	{
		float sinTheta = SinTheta(w);
		return sinTheta > 0.0f ? w.x / sinTheta : 1.0f;
	}
	__device__ __host__ inline float CosPhi2(const glm::vec3 &w)
	{
		return CosPhi(w) * CosPhi(w);
	}
	__device__ __host__ inline float SinPhi(const glm::vec3 &w)
	{
		float sinTheta = SinTheta(w);
		return sinTheta > 0.0f ? w.y / sinTheta : 1.0f;
	}
	__device__ __host__ inline float SinPhi2(const glm::vec3 &w)
	{
		return SinPhi(w) * SinPhi(w);
	}
	__device__ __host__ inline bool SameHemisphere(const glm::vec3 &wo, const glm::vec3 &wi)
	{
		return wo.z * wi.z > 0.0f;
	}
	__device__ __host__ inline glm::vec3 Reflect(const glm::vec3 &wo, const glm::vec3 &n)
	{
		return -wo + 2.0f * n * glm::dot(n, wo);
	}
	__device__ __host__ inline bool Refract(const glm::vec3 &wo, const glm::vec3 &n, float ni, float nt, glm::vec3 *wi)
	{
		// may be here are some bug
		float cosThetaI = glm::max(glm::dot(wo, n), 0.0f);
		float cosThetaI2 = Sqr(cosThetaI);
		float sinThetaI2 = 1.0f - cosThetaI2;
		float cosThetaT2 = 1.0f - Sqr(ni / nt) * sinThetaI2;
		if (!(cosThetaT2 > 0.0f)) 
		{ 
			return false;
		}
		float cosThetaT = glm::sqrt(cosThetaT2);
		*wi = glm::normalize( -(ni / nt) * (wo - n * cosThetaI) - n * cosThetaT);
		return true;
	}

	// eta = nt / ni
	__device__ __host__ inline float FresnelDielectricDielectric(float eta, float cos_theta)
	{
		cos_theta = glm::max(0.0f, cos_theta);
		float SinTheta2 = 1.f - cos_theta * cos_theta;

		float t0 = glm::sqrt(glm::max(1.0f - (SinTheta2 / (eta * eta)), 0.0f));
		float t1 = eta * t0;
		float t2 = eta * cos_theta;

		float rs = (cos_theta - t1) / glm::max(cos_theta + t1, 1E-12f);
		float rp = (t0 - t2) / glm::max(t0 + t2, 1E-12f);

		return 0.5 * (rs * rs + rp * rp);
	}
	__device__ __host__ inline glm::vec3 FresnelDieletricConductor(const glm::vec3 &Eta, const glm::vec3 &Etak, float CosTheta)
	{
		float CosTheta2 = CosTheta * CosTheta;
		float SinTheta2 = 1.0f - CosTheta2;
		glm::vec3 Eta2 = Eta * Eta;
		glm::vec3 Etak2 = Etak * Etak;

		glm::vec3 t0 = Eta2 - Etak2 - SinTheta2;
		glm::vec3 a2plusb2 = sqrt(t0 * t0 + 4.0f * Eta2 * Etak2);
		glm::vec3 t1 = a2plusb2 + CosTheta2;
		glm::vec3 a = sqrt(0.5f * (a2plusb2 + t0));
		glm::vec3 t2 = 2.0f * a * CosTheta;
		glm::vec3 Rs = (t1 - t2) / (t1 + t2);

		glm::vec3 t3 = CosTheta2 * a2plusb2 + SinTheta2 * SinTheta2;
		glm::vec3 t4 = t2 * SinTheta2;
		glm::vec3 Rp = Rs * (t3 - t4) / (t3 + t4);

		return 0.5f * (Rp + Rs);
	}
	__device__ __host__ inline float FresnelR0(float n /* nt / ni */)
	{
		return Sqr(n - 1.0f) / Sqr(n + 1.0f);
	}
	__device__ __host__ inline float FresnelSchlick(float r0, float cos_theta)
	{
		return r0 + (1.f - r0) * Sqr(Sqr(cos_theta)) * cos_theta;
	}
	__device__ __host__ inline void EdgeTintToConductiveFresnel(const glm::vec3 &reflectivity, const glm::vec3 &edgeTint, glm::vec3 *ior_n, glm::vec3 *ior_k)
	{
		glm::vec3 r = glm::clamp(reflectivity, glm::vec3(0.f), glm::vec3(0.99f));
		glm::vec3 g = edgeTint;
		glm::vec3 r_sqrt = glm::sqrt(r);
		glm::vec3 n_min = (glm::vec3(1.0f) - r) / (glm::vec3(1.0f) + r);
		glm::vec3 n_max = (glm::vec3(1.0f) + r_sqrt) / (glm::vec3(1.0f) - r_sqrt);
		*ior_n = glm::mix(n_max, n_min, g);
		glm::vec3 k2 = ((*ior_n + glm::vec3(1.0f)) * (*ior_n + glm::vec3(1.0f)) * r - (*ior_n - glm::vec3(1.0f)) * (*ior_n - glm::vec3(1.0f))) / (glm::vec3(1.0f) - r);
		k2 = glm::max(k2, glm::vec3(0.0));
		*ior_k = glm::sqrt(k2);
	}

	__device__ __host__ inline float D(const glm::vec3& wh, float alpha_x, float alpha_y)
	{
		float r_x = alpha_x;
		float r_y = alpha_y;
		float deno = ONE_PI * r_x * r_y * Sqr(Sqr(wh.x / r_x) + Sqr(wh.y / r_y) + Sqr(wh.z));
		return 1.0f / glm::max(deno, 1E-12f);
	}
	__device__ __host__ inline float Lambda(const glm::vec3& w, float alpha_x, float alpha_y)
	{
		if (glm::abs(w.z) < 1E-8f) { return 0.0f; }
		float r_x = alpha_x;
		float r_y = alpha_y;
		return (-1.0f + glm::sqrt(1.0f + (Sqr(w.x * r_x) + Sqr(w.y * r_y)) / Sqr(w.z))) * 0.5f;
	}
	__device__ __host__ inline float G1(const glm::vec3 &w, float alpha_x, float alpha_y)
	{
		return 1.0f / (1.0f + Lambda(w, alpha_x, alpha_y));
	}
	__device__ __host__ inline float G2(const glm::vec3 &wo, const glm::vec3 &wi, float alpha_x, float alpha_y)
	{
		return 1.0f / (1.0f + Lambda(wo, alpha_x, alpha_y) + Lambda(wi, alpha_x, alpha_y));
	}
	__device__ __host__ inline glm::vec3 SampleWh(const glm::vec3 &wo, float u0, float u1, float alpha_x, float alpha_y)
	{
		glm::vec3 wh = glm::normalize(glm::vec3(alpha_x * wo.x, alpha_y * wo.y, wo.z));

		float lensq = wh.x * wh.x + wh.y * wh.y;
		glm::vec3 T1 = lensq > 0 ? glm::vec3(-wh.y, wh.x, 0) * glm::inversesqrt(lensq) : glm::vec3(1, 0, 0);
		glm::vec3 T2 = cross(wh, T1);

		float r = glm::sqrt(glm::max(u0, 0.0f));
		float phi = 2.0f * ONE_PI * u1;
		float t1 = r * glm::cos(phi);
		float t2 = r * glm::sin(phi);
		float s = 0.5 * (1.0 + wh.z);
		t2 = (1.0 - s) * glm::sqrt(1.0 - t1 * t1) + s * t2;

		glm::vec3 Nh = t1 * T1 + t2 * T2 + glm::sqrt(glm::max(0.0f, 1.0f - t1 * t1 - t2 * t2)) * wh;
		glm::vec3 Ne = glm::normalize(glm::vec3(alpha_x * Nh.x, alpha_y * Nh.y, glm::max(0.0f, Nh.z)));
		return Ne;
	}

	enum BSDF_TYPE
	{
		LAMBERT = 0,
		MICROFACET_REFLECTION = 1,
		MICROFACET_TRANSMISSION = 2
	};

	struct Lambert
	{
		// https://www.researchgate.net/profile/Helen-Hu-3/publication/255580911_A_Coupled_Matte-Specular_Reflection_Model/links/57c717b008ae9d64047e95d0/A-Coupled-Matte-Specular-Reflection-Model.pdf
		glm::vec3 diffuse_albedo;

		// all the direction in shading_frame
		__device__ __host__ inline float PDF(const glm::vec3 &wo, const glm::vec3 &wi)
		{
			return SameHemisphere(wo, wi) ? glm::max(CosTheta(wi), 0.0f) * INV_PI : 0.0f;
		}
		__device__ __host__ inline glm::vec3 Eval(const glm::vec3 &wo, const glm::vec3 &wi, float *pdf)
		{
			if (!SameHemisphere(wo, wi)) 
			{
				*pdf = 0.0f;
				return glm::vec3(0.0f);
			}
			float cos_theta = glm::max(wi.z, 0.0f);
			*pdf = cos_theta * INV_PI;
			return diffuse_albedo *  cos_theta * INV_PI ; 
		}
		__device__ __host__ inline bool Sample(float u0, float u1, const glm::vec3 &wo, glm::vec3 *weight, glm::vec3 *wi, float *pdf)
		{
			*wi = SampleCosine(u0, u1);
			float cos_theta = glm::max((*wi).z, 0.0f);
			*pdf =  cos_theta * INV_PI;
			*weight = diffuse_albedo ; 
			return true;
		}
	};
	struct MicrofacetReflection
	{
		glm::vec3 specular_albedo;
		glm::vec3 nt;
		glm::vec3 kt;
		float ni;
		float alpha_x;
		float alpha_y;
		int is_metal;

		__device__ __host__ inline float PDF(const glm::vec3 &wo, const glm::vec3 &wi)
		{
			if (!SameHemisphere(wo, wi) || wo.z < 0.f || wi.z < 0.f) { return 0.0f; }
			glm::vec3 wh = wi + wo;
			float len = glm::length(wh);
			if (glm::abs(len) < 1E-8f) { return 0.0f; }
			wh /= len;
			return D(wh, alpha_x, alpha_y) * G1(wo, alpha_x, alpha_y) * 0.25f / glm::max(wo.z, 1E-8f);
		}
		__device__ __host__ inline glm::vec3 Eval(const glm::vec3 &wo, const glm::vec3 &wi, float *pdf)
		{
			if (!SameHemisphere(wo, wi) || wo.z < 0.f || wi.z < 0.f)
			{
				*pdf = 0.0f;
				return glm::vec3(0.0f);
			}
			glm::vec3 wh = wi + wo;
			float len = glm::length(wh);
			if (glm::abs(len) < 1E-8f)
			{
				*pdf = 0.0f;
				return glm::vec3(0.0f);
			}
			wh /= len;
			float D_term = D(wh, alpha_x, alpha_y);
			float G1_term = G1(wo, alpha_x, alpha_y);
			float G2_term = G2(wo, wi, alpha_x, alpha_y);
			const float i_ni = SafeRcp(ni);
			const float cos_theta = glm::max(glm::dot(wo, wh), 0.0f);
			const glm::vec3 fr = is_metal ? FresnelDieletricConductor(nt * i_ni, kt * i_ni, cos_theta) : glm::vec3(FresnelDielectricDielectric(nt.x * i_ni, cos_theta));
			*pdf = D_term * G1_term * 0.25f / glm::max(wo.z, 1E-8f);
			return specular_albedo * D_term * G2_term * 0.25f * fr / glm::max(wo.z, 1E-8f);
		}
		__device__ __host__ inline bool Sample(float u0, float u1, const glm::vec3 &wo, glm::vec3 *weight, glm::vec3 *wi, float *pdf)
		{
			glm::vec3 wh = SampleWh(wo, u0, u1, alpha_x, alpha_y);
			if (!SameHemisphere(wo, wh) || glm::dot(wo, wh) < 0.0f)
			{
				return false;
			}
			*wi = Reflect(wo, wh);
			if (!SameHemisphere(*wi, wo))
			{
				return false;
			}
			float D_term = D(wh, alpha_x, alpha_y);
			float G1_term = G1(wo, alpha_x, alpha_y);
			float G2_term = G2(wo, *wi, alpha_x, alpha_y);
			const float i_ni = SafeRcp(ni);
			const float cos_theta = glm::max(glm::dot(wo, wh), 0.0f);
			const glm::vec3 fr = is_metal ? FresnelDieletricConductor(nt * i_ni, kt * i_ni, cos_theta) : glm::vec3(FresnelDielectricDielectric(nt.x * i_ni, cos_theta));
			*pdf = D_term * G1_term * 0.25f / glm::max(wo.z, 1E-8f);
			*weight = specular_albedo * G2_term * fr / G1_term;
			return true;
		}
	};
	struct MicrofacetTransmission
	{
		glm::vec3 transmission_albedo;
		float ni, nt;
		float alpha_x, alpha_y;

		__device__ __host__ inline float PDF(const glm::vec3 &wo, const glm::vec3 &wi) 
		{
			if (SameHemisphere(wo, wi))
			{
				return 0.0f;
			}
			glm::vec3 wh = -(ni * wo + nt * wi);
			if (!SameHemisphere(wo, wh))
			{
				return 0.0f;
			}
			float len = glm::length(wh);
			if (glm::abs(len) < 1E-8f) { return 0.0f; }
			wh /= len;

			float D_term = D(wh, alpha_x, alpha_y);
			float G1_term = G1(wo, alpha_x, alpha_y);
			float dwhdwi = Sqr(nt) * glm::abs(glm::dot(wi, wh)) / Sqr(ni * glm::dot(wo, wh) + nt * glm::dot(wi, wh));
			return D_term * G1_term * glm::abs(glm::dot(wo, wh)) * dwhdwi / glm::max(glm::abs(wo.z), 1E-8f);
		}

		__device__ __host__ inline glm::vec3 Eval(const glm::vec3 &wo, const glm::vec3 &wi, float *pdf)
		{
			if (SameHemisphere(wo, wi))
			{
				*pdf = 0.0f;
				return glm::vec3(0.0f);
			}
			glm::vec3 wh = -(ni * wo + nt * wi);
			if (!SameHemisphere(wo, wh))
			{
				*pdf = 0.0f;
				return glm::vec3(0.0f);
			}
			float len = glm::length(wh);
			if (glm::abs(len) < 1E-8f)
			{ 
				*pdf = 0.0f;
				return glm::vec3(0.0f);
			}
			wh /= len;

			float D_term = D(wh, alpha_x, alpha_y);
			float G1_term = G1(wo, alpha_x, alpha_y);
			float G2_term = G2(wo, wi, alpha_x, alpha_y);
			float dwhdwi = Sqr(nt) * glm::abs(glm::dot(wi, wh)) / glm::max(Sqr(ni * glm::dot(wo, wh) + nt * glm::dot(wi, wh)), 1E-12f);
			const float i_ni = SafeRcp(ni);
			const float cos_theta = glm::max(glm::dot(wo, wh), 0.0f);
			const float fr = FresnelDielectricDielectric(nt * i_ni, cos_theta);

			*pdf = D_term * G1_term * glm::abs(glm::dot(wo, wh)) * dwhdwi / glm::max(glm::abs(wo.z), 1E-8f);
			return transmission_albedo * (1.0f - fr) * glm::abs(glm::dot(wo, wh) / glm::max(glm::abs(wo.z), 1E-8f)) * G2_term * D_term * dwhdwi * Sqr(ni / nt);
		}

		__device__ __host__ inline bool Sample(float u0, float u1, const glm::vec3 &wo, glm::vec3 *weight, glm::vec3 *wi, float *pdf)
		{
			glm::vec3 wh = SampleWh(wo, u0, u1, alpha_x, alpha_y);
			if (!SameHemisphere(wo, wh) || glm::dot(wo, wh) < 0.0f)
			{
				return false;
			}

			if (!Refract(wo, wh, ni, nt, wi))
			{
				return false;
			}

			if (SameHemisphere(*wi, wo))
			{
				return false;
			}

			float D_term = D(wh, alpha_x, alpha_y);
			float G1_term = G1(wo, alpha_x, alpha_y);
			float G2_term = G2(*wi, wo, alpha_x, alpha_y);
			float dwhdwi = Sqr(nt) * glm::abs(glm::dot(*wi, wh)) / glm::max(Sqr(ni * glm::dot(wo, wh) + nt * glm::dot(*wi, wh)), 1E-12f);
			const float i_ni = SafeRcp(ni);
			const float cos_theta = glm::max(glm::dot(wo, wh), 0.0f);
			const float fr = FresnelDielectricDielectric(nt * i_ni, cos_theta);

			*pdf = D_term * G1_term * glm::abs(glm::dot(wo, wh)) * dwhdwi / glm::max(glm::abs(wo.z), 1E-8f);
			*weight = transmission_albedo * (1.0f - fr) * Sqr(ni / nt) * G2_term / G1_term;
			return true;
		}
	};
	struct BSDF
	{
		uint32_t bsdf_type;
		union 
		{
			Lambert lambert;
			MicrofacetReflection microfacet_reflect;
			MicrofacetTransmission microfacet_transmit;
		};

		__device__ __host__ inline BSDF() {};
		__device__ __host__ inline float PDF(const glm::vec3 &wo, const glm::vec3 &wi)
		{
			if (bsdf_type == LAMBERT)
			{
				return lambert.PDF(wo, wi);
			}
			else if (bsdf_type == MICROFACET_REFLECTION)
			{
				return microfacet_reflect.PDF(wo, wi);
			}
			else if (bsdf_type == MICROFACET_TRANSMISSION)
			{
				return microfacet_transmit.PDF(wo, wi);
			}
			else 
			{
				return 0.0f;
			}
		}
		__device__ __host__ inline glm::vec3 Eval(const glm::vec3 &wo, const glm::vec3 &wi, float *pdf)
		{
			if (bsdf_type == LAMBERT)
			{
				return lambert.Eval(wo, wi, pdf);
			}
			else if (bsdf_type == MICROFACET_REFLECTION)
			{
				return microfacet_reflect.Eval(wo, wi, pdf);
			}
			else if (bsdf_type == MICROFACET_TRANSMISSION)
			{
				return microfacet_transmit.Eval(wo, wi, pdf);
			}
			else 
			{
				return glm::vec3(0.0f);
			}
		}
		__device__ __host__ inline bool Sample(float u0, float u1, const glm::vec3 &wo, glm::vec3 *weight, glm::vec3 *wi, float *pdf)
		{
			if (bsdf_type == LAMBERT)
			{
				return lambert.Sample(u0, u1, wo, weight, wi, pdf);
			}
			else if (bsdf_type == MICROFACET_REFLECTION)
			{
				return microfacet_reflect.Sample(u0, u1, wo, weight, wi, pdf);
			}
			else if (bsdf_type == MICROFACET_TRANSMISSION)
			{
				return microfacet_transmit.Sample(u0, u1, wo, weight, wi, pdf);
			}
			else 
			{
				return false;
			}
		}
	};

	struct DefaultMtlBSDF 
	{
		BSDF bsdfs[4];
		float weights[4];
		int bsdf_count;

		__device__ __host__ inline DefaultMtlBSDF() : weights{ 0.0f, 0.0f, 0.0f, 0.0f }, bsdf_count(0){}
		__device__ __host__ inline void InitBSDFSettings(const Material &mtl,
			const Texture *textures,
			const ImageTileCache &Image_tile_cache,
			const TextureCoordinate &texture_coordinate,
			const Ray &ray_in,
			const glm::vec3 &normal,
			int hit_back,
			float prim_outer_ior) 
		{
			auto fetch_color = [&](uint32_t tex_idx, const glm::vec3 &default_color)->glm::vec3{
				return tex_idx != INVALID_UINT_32 ? TextureEval(textures[tex_idx], textures, Image_tile_cache, texture_coordinate) : default_color;
			};
			auto fetch_float = [&](uint32_t tex_idx, const float default_float)->float {
				return tex_idx != INVALID_UINT_32 ? TextureEval(textures[tex_idx], textures, Image_tile_cache, texture_coordinate).x : default_float;
			};

			const DefaultMtl& default_mtl = mtl.default_mtl;

			const float i_ior = glm::max(ray_in.ray_ior, 0.0001f);
			const float o_ior = glm::max(hit_back ? prim_outer_ior : default_mtl.ior_n, 0.0001f);
			const float eta = o_ior * SafeRcp(i_ior);
			const float fr = FresnelDielectricDielectric(eta, glm::max(glm::dot(normal, -ray_in.direction), 0.0f));

			const float metalness = fetch_float(default_mtl.metalness_tex, default_mtl.metalness);
			const float alpha_x = Sqr(glm::clamp(fetch_float(default_mtl.alpha_x_tex, default_mtl.alpha_x), 0.001f, 0.999f));
			const float alpha_y = Sqr(glm::clamp(fetch_float(default_mtl.alpha_y_tex, default_mtl.alpha_y), 0.001f, 0.999f));
			const float specular_weight = fetch_float(default_mtl.specular_weight_tex, default_mtl.specular_weight);
			const float transmission_weight = fetch_float(default_mtl.transmission_weight_tex, default_mtl.transmission_weight);
			const glm::vec3 diffuse_albedo = fetch_color(default_mtl.diffuse_albedo_tex, default_mtl.diffuse_albedo);
			const glm::vec3 specular_albedo = fetch_color(default_mtl.specular_albedo_tex, default_mtl.specular_albedo);
			
			assert(bsdf_count == 0);

			bsdf_count = 0;
			const float weight_0 = metalness;
			if (weight_0 > 0.0f)
			{
				glm::vec3 metal_n, metal_k;
				EdgeTintToConductiveFresnel(diffuse_albedo, specular_albedo, &metal_n, &metal_k);
				bsdfs[bsdf_count].bsdf_type = MICROFACET_REFLECTION;
				bsdfs[bsdf_count].microfacet_reflect.specular_albedo = glm::vec3(1.0f);
				bsdfs[bsdf_count].microfacet_reflect.alpha_x = alpha_x;
				bsdfs[bsdf_count].microfacet_reflect.alpha_y = alpha_y;
				bsdfs[bsdf_count].microfacet_reflect.nt = metal_n;
				bsdfs[bsdf_count].microfacet_reflect.kt = metal_k;
				bsdfs[bsdf_count].microfacet_reflect.ni = i_ior;
				bsdfs[bsdf_count].microfacet_reflect.is_metal = (int)true;
				
				weights[bsdf_count] = weight_0;
				++bsdf_count;
			}

			const float weight_1 = (1.0f - metalness) * fr * specular_weight;
			if (weight_1 > 0.0f)
			{
				bsdfs[bsdf_count].bsdf_type = MICROFACET_REFLECTION;
				bsdfs[bsdf_count].microfacet_reflect.specular_albedo = specular_albedo;
				bsdfs[bsdf_count].microfacet_reflect.alpha_x = alpha_x;
				bsdfs[bsdf_count].microfacet_reflect.alpha_y = alpha_y;
				bsdfs[bsdf_count].microfacet_reflect.nt = glm::vec3(o_ior);
				bsdfs[bsdf_count].microfacet_reflect.kt = glm::vec3(0.0f);
				bsdfs[bsdf_count].microfacet_reflect.ni = i_ior;
				bsdfs[bsdf_count].microfacet_reflect.is_metal = (int)false;

				weights[bsdf_count] = weight_1;
				++bsdf_count;
			}

			const float weight_2 = (1.0f - metalness) * (1.0 - fr * specular_weight) * transmission_weight;
			if (weight_2 > 0.0f) 
			{
				bsdfs[bsdf_count].bsdf_type = MICROFACET_TRANSMISSION;
				bsdfs[bsdf_count].microfacet_transmit.transmission_albedo = specular_albedo;
				bsdfs[bsdf_count].microfacet_transmit.alpha_x = alpha_x;
				bsdfs[bsdf_count].microfacet_transmit.alpha_y = alpha_y;
				bsdfs[bsdf_count].microfacet_transmit.ni = i_ior;
				bsdfs[bsdf_count].microfacet_transmit.nt = o_ior;

				weights[bsdf_count] = weight_2;
				++bsdf_count;
			}

			const float weight_3 = (1.0f - metalness) * (1.0 - fr * specular_weight) *(1.0f - transmission_weight);
			if (weight_3 > 0.0f)
			{
				bsdfs[bsdf_count].bsdf_type = LAMBERT;
				bsdfs[bsdf_count].lambert.diffuse_albedo = diffuse_albedo;
				
				weights[bsdf_count] = weight_3;
				++bsdf_count;
			}
		}
		__device__ __host__ inline glm::vec3 EvalWi(const glm::vec3 &wo, const glm::vec3 &wi, float *pdf)
		{
			glm::vec3 mix_bsdf_weight(0.0f);
			float mix_pdf = 0.0f;
			for (int i = 0; i < bsdf_count; ++i) 
			{
				float sample_pdf = 0.0f;
				mix_bsdf_weight += weights[i] * bsdfs[i].Eval(wo, wi, &sample_pdf);
				mix_pdf += weights[i] * sample_pdf;
			}
			*pdf = mix_pdf;
			return mix_bsdf_weight;
		}
		__device__ __host__ inline bool SampleWi(const glm::vec3 &wo,
			glm::vec3 *weight,
			glm::vec3 *wi,
			float *pdf,
			float u0, float u1, float u2)
		{
			float cdf = 0.0f;
			int selected_bsdf_idx = -1;
			for (int i = 0; i < bsdf_count; ++i)
			{
				if (u0 < cdf + weights[i])
				{
					selected_bsdf_idx = i;
					break;
				}
				cdf += weights[i];
			}
			if (selected_bsdf_idx == -1) { selected_bsdf_idx = bsdf_count - 1; }
			if (selected_bsdf_idx == -1) { return false; }

			float bsdf_wi_pdf = 0.0f;
			bool sample_valid = bsdfs[selected_bsdf_idx].Sample(u1, u2, wo, weight, wi, &bsdf_wi_pdf);
			if (!sample_valid || !(bsdf_wi_pdf * weights[selected_bsdf_idx] > 0.0f)) { return false; }

			float mix_pdf = bsdf_wi_pdf * weights[selected_bsdf_idx];
			glm::vec3 mix_bsdf = (*weight) * bsdf_wi_pdf * weights[selected_bsdf_idx];
			for (int i = 0; i < bsdf_count; ++i)
			{
				if (i == selected_bsdf_idx) { continue; }
				BSDF &bsdf = bsdfs[i];
				float wi_pdf;
				mix_bsdf += weights[i] * bsdf.Eval(wo, *wi, &wi_pdf);
				mix_pdf += weights[i] * wi_pdf;
			}
			
			*pdf = mix_pdf;
			*weight = mix_bsdf * SafeRcp(mix_pdf);
			return true;
		}
	};
	struct MaterialBSDF 
	{
		glm::vec3 tangent; 
		glm::vec3 bitangent;
		glm::vec3 normal;

		uint32_t material_type;
		union 
		{
			DefaultMtlBSDF default_mtl_bsdf;
		};

		__device__ __host__ inline MaterialBSDF(): material_type(INVALID_UINT_32){}
		__device__ __host__ inline void InitShadingSpace(const glm::vec3 &hit_normal, const glm::vec3 &hit_tangent)
		{
			normal = hit_normal;
			tangent = glm::normalize(hit_tangent - normal * glm::dot(hit_tangent, normal));
			bitangent = glm::cross(normal, tangent);
		}
		__device__ __host__ inline glm::vec3 ShadingToWorld(const glm::vec3 &w)
		{
			return w.x * tangent + w.y * bitangent + w.z * normal;
		}
		__device__ __host__ inline glm::vec3 WorldToShading(const glm::vec3 &w)
		{
			return glm::vec3(glm::dot(w, tangent), glm::dot(w, bitangent), glm::dot(w, normal));
		}
		__device__ __host__ inline void InitBSDFSettings(const Material &mtl,
			const Texture *textures,
			const ImageTileCache &Image_tile_cache,
			const TextureCoordinate &texture_coordinate,
			const Ray &ray_in,
			int hit_back,
			float prim_outer_ior) 
		{
			material_type = mtl.material_type;
			
			assert(material_type != INVALID_UINT_32);
			if (material_type == DEFAULT_MTL) 
			{
				default_mtl_bsdf.InitBSDFSettings(mtl, textures, Image_tile_cache, texture_coordinate, ray_in, normal, hit_back, prim_outer_ior);
			}
			else if (material_type == LIGHT_MTL)
			{
			
			}
			else 
			{
			
			}
		}
		__device__ __host__ inline glm::vec3 EvalWi(const glm::vec3 &wo, const glm::vec3 &wi, float *pdf)
		{
			assert(material_type != INVALID_UINT_32);
			if (material_type == DEFAULT_MTL)
			{
				return default_mtl_bsdf.EvalWi(wo, wi, pdf);
			}
			else if (material_type == LIGHT_MTL)
			{
				return glm::vec3(0.0f);
			}
			else
			{
				return glm::vec3(0.0f);
			}
		}
		__device__ __host__ inline bool SampleWi(const glm::vec3 &wo,
			glm::vec3 *weight,
			glm::vec3 *wi,
			float *pdf,
			float u0, float u1, float u2) 
		{
			assert(material_type != INVALID_UINT_32);
			if (material_type == DEFAULT_MTL)
			{
				return default_mtl_bsdf.SampleWi(wo, weight, wi, pdf, u0, u1, u2);
			}
			else if (material_type == LIGHT_MTL)
			{
				return false;
			}
			else
			{
				return false;
			}
		}
	};
};