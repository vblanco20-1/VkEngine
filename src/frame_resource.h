
#pragma once
template<typename T, int NFrames>
struct FrameResource {
	T& get(int frame) {
		return items[frame % NFrames];
	}
	T items[NFrames];
	static constexpr int num = NFrames;
};