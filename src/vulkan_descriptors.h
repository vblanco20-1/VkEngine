#pragma once
#include "vulkan_types.h"


struct DescriptorUBOParams {
	vk::Buffer buffer;
	size_t range;
};

struct DescriptorImageParams {
	vk::ImageLayout layout;
	vk::ImageView view;
	vk::Sampler sampler;


};

struct DescriptorSetBuilder {

	void initialize(vk::Device device, vk::DescriptorPool pool);
	void add_uniform_buffer(int Binding , DescriptorUBOParams Params);
	void build();

};

enum class DescriptorLifetime {
	Static,
	PerFrame
};

struct DescriptorAllocator {
	int max_descriptors;
	int current_descriptors;
	vk::DescriptorPool pool;
};
struct DescriptorMegaPool {

	using PoolStorage = std::vector<DescriptorAllocator*>;

	vk::DescriptorSet allocate_descriptor(vk::DescriptorSetLayout layout, DescriptorLifetime lifetime);

	PoolStorage static_pools;
	PoolStorage dynamic_pools;
	vk::Device device;
};





