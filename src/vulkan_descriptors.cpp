#include "vulkan_descriptors.h"
#include "shader_processor.h"

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
	if (lifetime == DescriptorLifetime::Static) {
		VkDescriptorSet set;
		bool goodAlloc = staticHandle.Allocate(layout, set);
		return set;
	}
	else {
		VkDescriptorSet set;
		bool goodAlloc = dynamicHandle.Allocate(layout, set);
		return set;
	}

	

	//find pool by dynamic type
	PoolStorage* selectedPool;
	switch (lifetime) {
	case DescriptorLifetime::Static:
		selectedPool = &static_pools;
		break;
	case DescriptorLifetime::PerFrame:
		selectedPool = dynamic_pools[currentFrame];
		break;
	}
	//find vector by descriptor type
	std::vector<DescriptorAllocator*> * selected_vector = selectedPool;

	DescriptorAllocator* selected_allocator;
	//first initialization
	if (selected_vector->size() == 0) {
		selected_allocator = get_allocator();
		selected_vector->push_back(selected_allocator);
	}
	else
	{
		selected_allocator = selected_vector->back();
		if (selected_allocator->current_descriptors >= selected_allocator->max_descriptors) {

			selected_allocator = get_allocator();
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

DescriptorAllocator* DescriptorMegaPool::get_allocator()
{
	if (empty_pools.size() == 0) {
		const int descriptor_size = 512;

		DescriptorAllocator* selected_allocator = new DescriptorAllocator();

		selected_allocator->current_descriptors = 0;
		selected_allocator->max_descriptors = descriptor_size;
		selected_allocator->pool = create_descriptor_pool(device, descriptor_size);

		return selected_allocator;
	}
	else {
		DescriptorAllocator* selected_allocator = empty_pools.back();
		empty_pools.pop_back();
		return selected_allocator;
	}
}

void DescriptorMegaPool::initialize(int numFrames,vk::Device _device)
{
	
	device = _device;

	allocatorPool = vke::DescriptorAllocatorPool::Create(device, numFrames);
	dynamicHandle = allocatorPool->GetAllocator(vke::DescriptorAllocatorLifetime::PerFrame);
	staticHandle = allocatorPool->GetAllocator(vke::DescriptorAllocatorLifetime::Static);
	for (int i = 0; i < numFrames; i++) {
		dynamic_pools.push_back(new PoolStorage);
	}
}

void DescriptorMegaPool::set_frame(int frameNumber)
{
	dynamicHandle.Return();

	allocatorPool->Flip();
	dynamicHandle = allocatorPool->GetAllocator(vke::DescriptorAllocatorLifetime::PerFrame);

	PoolStorage* framePool = dynamic_pools[frameNumber];

	for (DescriptorAllocator* alloc : *framePool) {
		device.resetDescriptorPool(alloc->pool);

		empty_pools.push_back(alloc);
	}
	framePool->clear();

	currentFrame = frameNumber;
}

DescriptorSetBuilder::DescriptorSetBuilder(ShaderEffect* _effect, DescriptorMegaPool* _parentPool)
{
	effect = _effect;
	parentPool = _parentPool;
}

void DescriptorSetBuilder::bind_image(int set, int binding, const vk::DescriptorImageInfo& imageInfo, bool bImageWrite)
{

	for (auto& write : imageWrites) {
		if (write.dstBinding == binding
			&& write.dstSet == set	)
		{
			write.imageInfo = imageInfo;
			return;
		}
	}

	ImageWriteDescriptor newWrite;
	newWrite.dstSet = set;
	newWrite.dstBinding = binding;
	newWrite.descriptorType = bImageWrite ? vk::DescriptorType::eStorageImage :  vk::DescriptorType::eCombinedImageSampler;	
	newWrite.imageInfo = imageInfo; 
	if (bImageWrite) {
		newWrite.imageInfo.imageLayout = vk::ImageLayout::eGeneral;
	}

	imageWrites.push_back(newWrite);
}

void DescriptorSetBuilder::bind_image(const char* name, const vk::DescriptorImageInfo& imageInfo)
{
	if (effect) {
		BindReflection* reflection = effect->get_reflection();

		auto found = reflection->DataBindings.find(name);
		if (found != reflection->DataBindings.end()) {
			BindInfo info = (*found).second;
			bind_image(info.set, info.binding, imageInfo);
		}
	}
}

void DescriptorSetBuilder::bind_buffer(int set, int binding, const vk::DescriptorBufferInfo& bufferInfo, vk::DescriptorType bufferType)
{
	for (auto& write : bufferWrites) {
		if (write.dstBinding == binding
			&& write.dstSet == set)
		{
			write.bufferInfo = bufferInfo;
			return;
		}
	}
	

	BufferWriteDescriptor newWrite;
	newWrite.dstSet = set;
	newWrite.dstBinding = binding;
	newWrite.descriptorType = bufferType;
	newWrite.bufferInfo = bufferInfo;

	bufferWrites.push_back(newWrite);
}

void DescriptorSetBuilder::bind_buffer(const char* name, const vk::DescriptorBufferInfo& bufferInfo)
{	
	if (effect) {
		BindReflection* reflection = effect->get_reflection();

		auto found = reflection->DataBindings.find(name);
		if (found != reflection->DataBindings.end()) {

			BindInfo info = (*found).second;
			bind_buffer(info.set, info.binding, bufferInfo, vk::DescriptorType(info.type));
		}
	}	
}

void DescriptorSetBuilder::update_descriptor(int set, vk::DescriptorSet& descriptor, const vk::Device& device)
{
	std::vector<vk::WriteDescriptorSet> descriptorWrites;
	descriptorWrites.reserve(20);

	for (const ImageWriteDescriptor& imageWrite : imageWrites) {
		if (imageWrite.dstSet == set) {
			vk::WriteDescriptorSet newWrite;

			newWrite.dstSet = descriptor;
			newWrite.dstBinding = imageWrite.dstBinding;
			newWrite.dstArrayElement = 0;
			newWrite.descriptorType = imageWrite.descriptorType;
			newWrite.descriptorCount = 1;
			newWrite.pBufferInfo = nullptr;
			newWrite.pImageInfo = &imageWrite.imageInfo; // Optional
			newWrite.pTexelBufferView = nullptr; // Optional	


			descriptorWrites.push_back(newWrite);
		}
	}

	for (const BufferWriteDescriptor& bufferWrite : bufferWrites) {
		if (bufferWrite.dstSet == set) {
			vk::WriteDescriptorSet newWrite;

			newWrite.dstSet = descriptor;
			newWrite.dstBinding = bufferWrite.dstBinding;
			newWrite.dstArrayElement = 0;
			newWrite.descriptorType = bufferWrite.descriptorType;
			newWrite.descriptorCount = 1;
			newWrite.pBufferInfo = &bufferWrite.bufferInfo;
			newWrite.pImageInfo = nullptr; // Optional
			newWrite.pTexelBufferView = nullptr; // Optional	


			descriptorWrites.push_back(newWrite);
		}
	}

	device.updateDescriptorSets(descriptorWrites, 0);
}

vk::DescriptorSet DescriptorSetBuilder::build_descriptor(int set, DescriptorLifetime lifetime)
{
	vk::DescriptorSetLayout layout = effect->build_descriptor_layouts(parentPool->device)[set];
	vk::DescriptorSet newSet = parentPool->allocate_descriptor(layout, lifetime);

	update_descriptor(set, newSet, parentPool->device);
	return newSet;
}
