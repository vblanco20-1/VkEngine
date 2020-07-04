#include <array>
#include <stdint.h>

struct LinearAlloc {
	struct DataChunk {
		constexpr static size_t ChunkSize = 4080;
		std::array<char, ChunkSize> bytes;
		uint16_t last{0};
		DataChunk* next{nullptr};
	};

	void clear() {
		DataChunk* it = firstChunk;
		while (it != nullptr) {
			DataChunk* next = it->next;
			delete it;
			it = next;
		}		
		firstChunk = nullptr;
		lastChunk = nullptr;
	};
	

	void* allocate(size_t size);

	template<typename T, typename... Args>
	T* construct(Args...);

	DataChunk* first() { return firstChunk; };
private:

	void add_chunk() {
		DataChunk* newchnk = new DataChunk;
		if (firstChunk == nullptr) {
			firstChunk = newchnk;
		}
		if (lastChunk != nullptr) {
			lastChunk->next = newchnk;
		}
		lastChunk = newchnk;
	}

	DataChunk* firstChunk{nullptr};
	DataChunk* lastChunk{nullptr};
};

inline void* LinearAlloc::allocate(size_t size) {
	if (lastChunk == nullptr) {
		add_chunk();
	}
	else {
		if (lastChunk->last + size > DataChunk::ChunkSize) {
			add_chunk();
		}
	}

	void* ptr = &lastChunk->bytes[lastChunk->last];
	lastChunk->last += (uint16_t)size;
	return ptr;
};
template<typename T, typename...Args>
inline T* LinearAlloc::construct(Args...)
{
	void* ptr = allocate(sizeof(T));
	T* object = new(ptr) T(Args...);
	return object;
}
