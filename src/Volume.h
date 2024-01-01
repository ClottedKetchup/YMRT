#pragma once

#include <driver_types.h>

#include "MathCommon.h"

namespace YumeRT
{
	struct Volume 
	{
		glm::vec3 sigma_s;
		glm::vec3 sigma_a;

		__device__ __host__ Volume() {}
	};

};