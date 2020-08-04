#pragma once


//very simple versioned array with holes. The handles are guaranteed safe
//array indices are stable
template<typename T>
struct SlotBuffer {
	struct Handle {
		uint32_t version;
		uint32_t index;

		static Handle from_u64(uint64_t num) {
			uint32_t mask = numeric_limits<uint32_t>::max();

			uint64_t index = num & mask;
			uint64_t version = num >> 32 & mask;

			Handle newHandle;
			newHandle.version = version;
			newHandle.index = index;
			return newHandle;
		}

		static uint64_t to_u64(Handle handle) {
			uint64_t num = 0;
			num |= uint64_t(handle.index);
			num |= uint64_t(handle.version) << 32;
			return num;
		}
	};

	Handle add(const T& item) {
		Handle new_handle;
		new_handle.index = 0;

		if (free_indices.size() > 0)
		{
			new_handle.index = free_indices.back();
			free_indices.pop_back();	

			versions[new_handle.index]++;

			new_handle.version = versions[new_handle.index];

			//init properly
			data[new_handle.index].T(item);

		}
		else {
			new_handle.version = 1;
			versions.push_back(new_handle.version);
			data.push_back(item);
			
		}
		return new_handle;
	}
	bool valid(Handle handle) {
		if (handle.index < versions.size())
		{
			return handle.version == versions[handle.index];
		}
		else {
			return false;
		}
	}

	bool alive(uint32_t index)
	{
		//due to the way we control the version number, 
		//destroyed objects allways have even version
		return versions[index] % 2 == 0;
	}
	void remove(Handle handle) {
		if (valid(handle))
		{
			versions[handle.index]++;
			free_indices.push_back(handle.index);
			//call destructor
			data[handle.index].~T();
		}
	}

	T& get(Handle handle) {
		return data[handle.index];
	}

	bool valid(uint64_t handle) {return valid(Handle::from_u64(handle)); }

	void remove(uint64_t handle) {remove(Handle::from_u64(handle));	}
	
	T& get(uint64_t handle){return get(Handle::from_u64(handle));}


	std::vector<uint32_t> versions;
	std::vector<T> data;

	std::vector<uint32_t> free_indices;
};