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





