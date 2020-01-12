#pragma once
#include <vector>


template<typename T>
struct AlignedBuffer {
	
	AlignedBuffer(size_t default_alignement) : alignement(default_alignement) {

	}

	T& operator[](size_t index) {
		std::byte* ptr = &raw[index * alignement];
		return *(T*)ptr;
	}

	void resize(size_t size) {
		raw.resize(size * alignement);
	}

	size_t get_offset(size_t index) {
		return index * alignement;
	}

	void* get_raw() {
		return  &raw[0];
	}

private:
	std::size_t alignement;
	std::vector<std::byte> raw;
};