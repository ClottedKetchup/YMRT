#pragma once

#include <chrono>

namespace YumeRT {
	struct DuskTimer {
	public:
		DuskTimer() = default;
		inline void Start() {
			start_time = std::chrono::high_resolution_clock::now();
		}
		inline float Stop() {
			auto stop_time = std::chrono::high_resolution_clock::now();
			return float(std::chrono::duration_cast<std::chrono::microseconds>(stop_time - start_time).count()) / 1000.f;
		}
	private:
		std::chrono::high_resolution_clock::time_point start_time;
	};
};