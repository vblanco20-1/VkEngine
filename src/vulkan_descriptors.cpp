#include "vulkan_descriptors.h"

#define IM_ARRAYSIZE(_ARR)          ((int)(sizeof(_ARR) / sizeof(*_ARR))) 
vk::DescriptorPool create_descriptor_pool(vk::Device device, int count) {
	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, count },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, count },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, count },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, count },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, count },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, count },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, count },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, count },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, count },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, count },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, count }
	};
	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = count;
	pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;	

	vk::DescriptorPool descriptorPool = device.createDescriptorPool(vk::DescriptorPoolCreateInfo(pool_info));

	return descriptorPool;
}
vk::DescriptorSet DescriptorMegaPool::allocate_descriptor(vk::DescriptorSetLayout layout, DescriptorLifetime lifetime)
{
	//find pool by dynamic type
	PoolStorage* selectedPool;
	switch (lifetime) {
	case DescriptorLifetime::Static:
		selectedPool = &static_pools;
		break;
	case DescriptorLifetime::PerFrame:
		selectedPool = &dynamic_pools;
		break;
	}
	//find vector by descriptor type
	std::vector<DescriptorAllocator*> * selected_vector = selectedPool;

	DescriptorAllocator* selected_allocator;
	//first initialization
	if (selected_vector->size() == 0) {
		const int initial_descriptor_size = 64;
		selected_allocator = new DescriptorAllocator();

		selected_allocator->current_descriptors = 0;
		selected_allocator->max_descriptors = initial_descriptor_size;
		selected_allocator->pool = create_descriptor_pool(device, initial_descriptor_size);
		
		selected_vector->push_back(selected_allocator);

	}
	else
	{
		selected_allocator = selected_vector->back();
		if (selected_allocator->current_descriptors == selected_allocator->max_descriptors) {
			DescriptorAllocator* lastAllocator = selected_allocator;


			int descriptor_size = lastAllocator->max_descriptors * 2;
			if (descriptor_size > 1024) {
				descriptor_size = 1024;
			}

			selected_allocator = new DescriptorAllocator();

			selected_allocator->current_descriptors = 0;
			selected_allocator->max_descriptors = descriptor_size;
			selected_allocator->pool = create_descriptor_pool(device,descriptor_size);

			selected_vector->push_back(selected_allocator);
		}
	}

	vk::DescriptorSetAllocateInfo allocInfo;
	allocInfo.descriptorPool = selected_allocator->pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	vk::DescriptorSet newSet;
	selected_allocator->current_descriptors++;

	device.allocateDescriptorSets(&allocInfo, &newSet);

	return newSet;
}
