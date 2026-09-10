#pragma once

#include <chrono>

namespace YMRT {
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
		inline void StopAndPrint() {
			auto stop_time = std::chrono::high_resolution_clock::now();
			const float milliseconds = float(std::chrono::duration_cast<std::chrono::microseconds>(stop_time - start_time).count()) / 1000.f;
			printf("Time consumed: %.6f ms.\n", milliseconds);
		}
	private:
		std::chrono::high_resolution_clock::time_point start_time;
	};
};