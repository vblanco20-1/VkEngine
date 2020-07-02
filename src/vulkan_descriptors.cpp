#include "vulkan_descriptors.h"
#include "shader_processor.h"
#include <vk_device.h> 
#include <vk_initializers.h> 
#define ARRAYSIZE(_ARR)          ((int)(sizeof(_ARR) / sizeof(*_ARR))) 
VkDescriptorPool create_descriptor_pool(VkDevice device, int count) {
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

	VkDescriptorPoolCreateInfo pool_info = vkinit::descriptor_pool_create_info({ pool_sizes ,ARRAYSIZE(pool_sizes) }, 
												count, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

	VkDescriptorPool descriptorPool = vke::Device::createDescriptorPool(device,&pool_info);

	return descriptorPool;
}
VkDescriptorSet DescriptorMegaPool::allocate_descriptor(VkDescriptorSetLayout layout, DescriptorLifetime lifetime, void* pNext )
{
	
	if (lifetime == DescriptorLifetime::Static) {
		VkDescriptorSet set;
		bool goodAlloc = staticHandle.Allocate(layout, set, pNext);
		return set;
	}
	else {
		VkDescriptorSet set;
		bool goodAlloc = dynamicHandle.Allocate(layout, set, pNext);
		return set;
	}
}

void DescriptorMegaPool::initialize(int numFrames, VkDevice _device)
{	
	device = _device; 

	dynamicAllocatorPool = vke::DescriptorAllocatorPool::Create(device, numFrames);
	staticAllocatorPool = vke::DescriptorAllocatorPool::Create(device, 1);
	dynamicHandle = dynamicAllocatorPool->GetAllocator();
	staticHandle = staticAllocatorPool->GetAllocator();	
}

void DescriptorMegaPool::set_frame(int frameNumber)
{
	dynamicAllocatorPool->Flip();
	dynamicHandle = dynamicAllocatorPool->GetAllocator();
}

DescriptorSetBuilder::DescriptorSetBuilder(ShaderEffect* _effect, DescriptorMegaPool* _parentPool)
{
	effect = _effect;
	parentPool = _parentPool;
}

void DescriptorSetBuilder::bind_image_array(int set, int binding, VkDescriptorImageInfo* images, int count)
{
	ImageWriteDescriptor newWrite;
	newWrite.dstSet = set;
	newWrite.dstBinding = binding;
	newWrite.descriptorType = (VkDescriptorType)vk::DescriptorType::eCombinedImageSampler;
	newWrite.image_array = images;
	newWrite.image_count = count;
	//newWrite.imageInfo = imageInfo;
	//if (bImageWrite) {
	//	newWrite.imageInfo.imageLayout = vk::ImageLayout::eGeneral;
	//}

	imageWrites.push_back(newWrite);
}

void DescriptorSetBuilder::bind_image(int set, int binding, const VkDescriptorImageInfo& imageInfo, bool bImageWrite)
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
	newWrite.descriptorType = (VkDescriptorType)(bImageWrite ? vkf::DescriptorType::eStorageImage :  vkf::DescriptorType::eCombinedImageSampler);
	newWrite.imageInfo = imageInfo; 
	if (bImageWrite) {
		newWrite.imageInfo.imageLayout = (VkImageLayout) vk::ImageLayout::eGeneral;
	}

	imageWrites.push_back(newWrite);
}

void DescriptorSetBuilder::bind_image(const char* name, const VkDescriptorImageInfo& imageInfo)
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

void DescriptorSetBuilder::bind_buffer(int set, int binding, const VkDescriptorBufferInfo& bufferInfo, VkDescriptorType bufferType)
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

void DescriptorSetBuilder::bind_buffer(const char* name, const VkDescriptorBufferInfo& bufferInfo)
{	
	if (effect) {
		BindReflection* reflection = effect->get_reflection();

		auto found = reflection->DataBindings.find(name);
		if (found != reflection->DataBindings.end()) {

			BindInfo info = (*found).second;
			bind_buffer(info.set, info.binding, bufferInfo, info.type);
		}
	}	
}

void DescriptorSetBuilder::bind_raystructure(int set, int binding, const VkWriteDescriptorSetAccelerationStructureKHR& info)
{

	BufferWriteDescriptor newWrite;
	newWrite.dstSet = set;
	newWrite.dstBinding = binding;
	newWrite.descriptorType = (VkDescriptorType) vkf::DescriptorType::eAccelerationStructureKHR;
	newWrite.accelinfo = info;

	bufferWrites.push_back(newWrite);
}

void DescriptorSetBuilder::update_descriptor(int set, VkDescriptorSet& descriptor, const VkDevice& device)
{
	std::vector<VkWriteDescriptorSet> descriptorWrites;
	descriptorWrites.reserve(20);

	for (ImageWriteDescriptor& imageWrite : imageWrites) {
		if (imageWrite.dstSet == set) {
			VkWriteDescriptorSet newWrite = vkinit::descriptor_write_image(descriptor,imageWrite.dstBinding,&imageWrite.imageInfo,imageWrite.descriptorType);
		

			//special case for the image arrays
			if(imageWrite.image_array != nullptr)
			{
				newWrite.descriptorCount = imageWrite.image_count;
				newWrite.pImageInfo =imageWrite.image_array; 
			}

			descriptorWrites.push_back(newWrite);
		}
	}

	for ( BufferWriteDescriptor& bufferWrite : bufferWrites) {
		if (bufferWrite.dstSet == set) {
			VkWriteDescriptorSet newWrite = vkinit::descriptor_write_buffer(descriptor, bufferWrite.dstBinding, &bufferWrite.bufferInfo, bufferWrite.descriptorType);

		
			if (bufferWrite.descriptorType == vkf::DescriptorType::eAccelerationStructureKHR) 
			{
				newWrite.pBufferInfo = nullptr;
				newWrite.pNext = &bufferWrite.accelinfo;
			}
			else 
			{
				newWrite.pBufferInfo = &bufferWrite.bufferInfo;
			}

			descriptorWrites.push_back(newWrite);
		}
	}
	vke::Device::updateDescriptorSets(device, { descriptorWrites.data(), descriptorWrites.size() });
}

VkDescriptorSet DescriptorSetBuilder::build_descriptor(int set, DescriptorLifetime lifetime)
{
	
	vk::DescriptorSetLayout layout = effect->build_descriptor_layouts(parentPool->device)[set];

	void* pNext = nullptr;
	uint32_t counts[1];
	counts[0] = 4096; // Set 0 has a variable count descriptor with a maximum of 32 elements
//
	VkDescriptorSetVariableDescriptorCountAllocateInfo set_counts = {};
	set_counts.pNext = nullptr;
	set_counts.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
	set_counts.descriptorSetCount = 1;//counts.size();
	set_counts.pDescriptorCounts = counts;//counts.data();


	for (const ImageWriteDescriptor& imageWrite : imageWrites) {
		if (imageWrite.image_array != nullptr && imageWrite.dstSet == set)
		{
			counts[0] = imageWrite.image_count;
			pNext = &set_counts;
		}
	}


	VkDescriptorSet newSet = parentPool->allocate_descriptor(layout, lifetime,pNext);

	update_descriptor(set, newSet, parentPool->device);
	return newSet;
}
