#pragma once

#include <cuda_runtime.h>
#include <vector_types.h>
#include <driver_types.h>

#include "MathCommon.h"
#include "SampleUtilities.h"
#include "SceneDefines.h"
#include "Material.h"

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
		float nt, ni, specular_weight;

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
			// float eta = nt / ni;
			// float fr = FresnelDielectricDielectric(eta, cos_theta);
			*pdf = cos_theta * INV_PI;
			return diffuse_albedo *  cos_theta * INV_PI ; /** (1.0f - fr * specular_weight)*/
		}
		__device__ __host__ inline bool Sample(float u0, float u1, const glm::vec3 &wo, glm::vec3 *weight, glm::vec3 *wi, float *pdf)
		{
			*wi = SampleCosine(u0, u1);
			float cos_theta = glm::max((*wi).z, 0.0f);
			*pdf =  cos_theta * INV_PI;
			// float eta = nt / ni;
			// float fr = FresnelDielectricDielectric(eta, cos_theta);
			*weight = diffuse_albedo ; /** (1.0f - fr * specular_weight)*/
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
			glm::vec3 fr = is_metal ? FresnelDieletricConductor(nt / ni, kt / ni, glm::dot(wo, wh)) : glm::vec3(1.0f);
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
			glm::vec3 fr = is_metal ? FresnelDieletricConductor(nt / ni, kt / ni, glm::dot(wo, wh)) : glm::vec3(1.0f);
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

			*pdf = D_term * G1_term * glm::abs(glm::dot(wo, wh)) * dwhdwi / glm::max(glm::abs(wo.z), 1E-8f);
			return transmission_albedo * glm::abs(glm::dot(wo, wh) / glm::max(glm::abs(wo.z), 1E-8f)) * G2_term * D_term * dwhdwi * Sqr(ni / nt);
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

			*pdf = D_term * G1_term * glm::abs(glm::dot(wo, wh)) * dwhdwi / glm::max(glm::abs(wo.z), 1E-8f);
			*weight = transmission_albedo * Sqr(ni / nt) * G2_term / G1_term;
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

		__device__ __host__ BSDF() {};
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

	enum BOUNCE_TYPE
	{
		REFLECTION = 0,
		TRANSMISSION = 1
	};

	struct UberBSDF
	{
		glm::vec3 tangent, bitangent, normal;

		BSDF bsdfs[4];

		int back_side;
		float metalness;
		float specular_weight;
		float transmission_weight;

		float alpha_x;
		float alpha_y;
		float in_IOR;
		float out_IOR;
		
		float weights[4];
		bool component_inited[4];

		__device__ __host__ UberBSDF()
		{
			component_inited[0] = false;
			component_inited[1] = false;
			component_inited[2] = false;
			component_inited[3] = false;
		}


		__device__ __host__ inline void InitShadingSpace(const glm::vec3 &hit_normal, const glm::vec3 &hit_tangent)
		{
			normal = hit_normal;
			tangent = glm::normalize(hit_tangent - normal * glm::dot(hit_tangent, normal));
			bitangent = glm::cross(normal, tangent);
		}

		__device__ __host__ inline void InitBSDFSettings(const Material &mtl, const Ray &ray_in, int hit_back, float prim_outer_ior)
		{
			back_side = hit_back;

			metalness = mtl.surface_material.metalness;
			transmission_weight = mtl.surface_material.transmission_weight;
			specular_weight = mtl.surface_material.specular_weight;

			in_IOR = glm::max(ray_in.ray_ior, 0.001f);
			out_IOR = glm::max(hit_back? prim_outer_ior : mtl.surface_material.ior_n, 0.001f);

			alpha_x = Sqr(glm::clamp(mtl.surface_material.alpha_x, 0.001f, 0.98f));
			alpha_y = Sqr(glm::clamp(mtl.surface_material.alpha_y, 0.001f, 0.98f));

			float eta = out_IOR / in_IOR;
			float fr = FresnelDielectricDielectric(eta, glm::max(glm::dot(normal, -ray_in.direction), 0.0f));
			
			fr *= mtl.surface_material.specular_weight;
			// TO verify: is this right?
			weights[0] = metalness;
			weights[1] = (1.0f - metalness) * fr;
			weights[2] = (1.0f - metalness) * (1.0 - fr) * transmission_weight;
			weights[3] = (1.0f - metalness) * (1.0 - fr) * (1.0f - transmission_weight);

			// F(n) can be seen as the average of F(h), so this could be a reasonable approximation

			if (weights[0] > 0.0f)
			{
				glm::vec3 metal_n, metal_k;
				EdgeTintToConductiveFresnel(mtl.surface_material.diffuse_albedo, mtl.surface_material.specular_albedo, &metal_n, &metal_k);

				bsdfs[0].bsdf_type = MICROFACET_REFLECTION;
				bsdfs[0].microfacet_reflect.specular_albedo = glm::vec3(1.0f);
				bsdfs[0].microfacet_reflect.alpha_x = alpha_x;
				bsdfs[0].microfacet_reflect.alpha_y = alpha_y;
				bsdfs[0].microfacet_reflect.nt = metal_n;
				bsdfs[0].microfacet_reflect.kt = metal_k;
				bsdfs[0].microfacet_reflect.ni = in_IOR;
				bsdfs[0].microfacet_reflect.is_metal = (int)true;

				component_inited[0] = true;
			}

			if (weights[1] > 0.0f)
			{
				bsdfs[1].bsdf_type = MICROFACET_REFLECTION;
				bsdfs[1].microfacet_reflect.specular_albedo = mtl.surface_material.specular_albedo;
				bsdfs[1].microfacet_reflect.alpha_x = alpha_x;
				bsdfs[1].microfacet_reflect.alpha_y = alpha_y;
				bsdfs[1].microfacet_reflect.nt = glm::vec3(out_IOR);
				bsdfs[1].microfacet_reflect.kt = glm::vec3(0.0f);
				bsdfs[1].microfacet_reflect.ni = in_IOR;
				bsdfs[1].microfacet_reflect.is_metal = (int)false;

				component_inited[1] = true;
			}

			if (weights[2] > 0.0f)
			{
				bsdfs[2].bsdf_type = MICROFACET_TRANSMISSION;
				bsdfs[2].microfacet_transmit.transmission_albedo = mtl.surface_material.specular_albedo;
				bsdfs[2].microfacet_transmit.alpha_x = alpha_x;
				bsdfs[2].microfacet_transmit.alpha_y = alpha_y;
				bsdfs[2].microfacet_transmit.ni = in_IOR;
				bsdfs[2].microfacet_transmit.nt = out_IOR;
				
				component_inited[2] = true;
			}

			if (weights[3] > 0.0f)
			{
				bsdfs[3].bsdf_type = LAMBERT;
				bsdfs[3].lambert.diffuse_albedo = mtl.surface_material.diffuse_albedo;
				bsdfs[3].lambert.ni = in_IOR;
				bsdfs[3].lambert.nt = out_IOR;
				bsdfs[3].lambert.specular_weight = mtl.surface_material.specular_weight;
				component_inited[3] = true;
			}
		}

		__device__ __host__ inline glm::vec3 ShadingToWorld(const glm::vec3 &w)
		{
			return w.x * tangent + w.y * bitangent + w.z * normal;
		}

		__device__ __host__ inline glm::vec3 WorldToShading(const glm::vec3 &w)
		{
			return glm::vec3(glm::dot(w, tangent), glm::dot(w, bitangent), glm::dot(w, normal));
		}

		__device__ __host__ inline BSDF* SampleOneBSDF(float u0, float *pdf)
		{
			if (u0 < weights[0])
			{
				*pdf = weights[0];
				return GetMetalReflection();
			}
			else if (u0 < weights[0] + weights[1])
			{
				*pdf = weights[1];
				return GetSpecularReflection();
			}
			else if (u0 < weights[0] + weights[1] + weights[2])
			{
				*pdf = weights[2];
				return GetSpecularTransmission();
			}
			else 
			{
				*pdf = weights[3];
				return GetDiffuse();
			}
		}

		// this function is for direct lighting
		__device__ __host__ inline int SelectSingleBSDF(float u0, float *pdf)
		{
			if (u0 < weights[0])
			{
				*pdf = weights[0];
				return component_inited[0] ? 0 : -1;
			}
			else if (u0 < weights[0] + weights[1])
			{
				*pdf = weights[1];
				return component_inited[1] ? 1 : -1;
			}
			else if (u0 < weights[0] + weights[1] + weights[2])
			{
				*pdf = weights[2];
				return component_inited[2] ? 2 : -1;
			}
			else
			{
				*pdf = weights[3];
				return component_inited[3] ? 3 : -1;
			}
		}

		__device__ __host__ inline glm::vec3 EvalSingleBSDF(int selected_bsdf_idx, const glm::vec3 &wo, const glm::vec3 &wi, float *pdf)
		{
			glm::vec3 bsdf_weight = bsdfs[selected_bsdf_idx].Eval(wo, wi, pdf);

			if (selected_bsdf_idx == 3)
			{
				constexpr float k_coeff = 21.f / 20.f;
				float k = k_coeff * (1.0f - FresnelR0(out_IOR / in_IOR));
				float cos_theta = glm::max(wi.z, 0.f);
				float out_fr = FresnelDielectricDielectric(out_IOR / in_IOR, cos_theta);
				bsdf_weight *= (1.0f - out_fr * specular_weight) * k;
			}

			return bsdf_weight;
		}

		__device__ __host__ inline bool SampleSingleBSDF(int selected_bsdf_idx, float u0, float u1, const glm::vec3 &wo, glm::vec3 *weight, glm::vec3 *wi, float *pdf)
		{
			bool sample_valid = bsdfs[selected_bsdf_idx].Sample(u0, u1, wo, weight, wi, pdf);
			
			if (selected_bsdf_idx == 3)
			{
				constexpr float k_coeff = 21.f / 20.f;
				float k = k_coeff * (1.0f - FresnelR0(out_IOR / in_IOR));
				float cos_theta = glm::max((*wi).z, 0.f);
				float out_fr = FresnelDielectricDielectric(out_IOR / in_IOR, cos_theta);
				*weight *= (1.0f - out_fr * specular_weight) * k;
			}

			return sample_valid;
		}

		__device__ __host__ inline bool SampleIndirectMix(float u0, float u1,
			const glm::vec3 &wo,
			glm::vec3 *weight,
			glm::vec3 *wi,
			float *pdf,
			BOUNCE_TYPE *bounce_type)
		{
			int selected_bsdf_idx = -1;
			float bsdf_select_weight = 0.0f;

			if (u0 < weights[0])
			{
				u0 = u0 / weights[0];
				bsdf_select_weight = weights[0];
				selected_bsdf_idx = component_inited[0] ? 0 : -1;
			}
			else if (u0 < weights[0] + weights[1])
			{
				u0 = (u0 - weights[0]) / weights[1];
				bsdf_select_weight = weights[1];
				selected_bsdf_idx = component_inited[1] ? 1 : -1;
			}
			else if (u0 < weights[0] + weights[1] + weights[2])
			{
				u0 = (u0 - weights[0] - weights[1]) / weights[2];
				bsdf_select_weight = weights[2];
				selected_bsdf_idx = component_inited[2] ? 2 : -1;
			}
			else
			{
				u0 = (u0 - weights[0] - weights[1] - weights[2]) / weights[3];
				bsdf_select_weight = weights[3];
				selected_bsdf_idx = component_inited[3] ? 3 : -1;
			}

			if (selected_bsdf_idx == -1) { return false; }

			float bsdf_wi_pdf = 0.0f;
			bool sample_valid = SampleSingleBSDF(selected_bsdf_idx, u0, u1, wo, weight, wi, &bsdf_wi_pdf);
			if (!sample_valid || (bsdf_wi_pdf * bsdf_select_weight) == 0.0f) { return false; }

			float deno = 1.0f / (bsdf_wi_pdf * bsdf_select_weight);

			float mix_pdf = 1.0f;
			for (int i = 0; i < 4; ++i)
			{
				if (component_inited[i] && i != selected_bsdf_idx)
				{
					BSDF &bsdf = bsdfs[i];
					float pdf = bsdf.PDF(wo, *wi);
					mix_pdf += weights[i] * pdf * deno;
				}
			}
			if (glm::abs(mix_pdf) == 0.0f) { return false; }

			*pdf = 1.0f / (mix_pdf * bsdf_wi_pdf * bsdf_select_weight);
			*weight /= mix_pdf;
			return true;
		}

		__device__ __host__ inline BSDF* GetMetalReflection()
		{
			return component_inited[0] ? (&bsdfs[0]) : nullptr;
		}

		__device__ __host__ inline BSDF* GetSpecularReflection()
		{
			return component_inited[1] ? (&bsdfs[1]) : nullptr;
		}

		__device__ __host__ inline BSDF* GetSpecularTransmission()
		{
			return component_inited[2] ? (&bsdfs[2]) : nullptr;
		}

		__device__ __host__ inline BSDF* GetDiffuse()
		{
			return component_inited[3] ? (&bsdfs[3]) : nullptr;
		}
	};

};