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
vk::DescriptorSet DescriptorMegaPool::allocate_descriptor(vk::DescriptorSetLayout layout, DescriptorLifetime lifetime, void* pNext )
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

void DescriptorMegaPool::initialize(int numFrames,vk::Device _device)
{	
	device = _device; 

	dynamicAllocatorPool = vke::DescriptorAllocatorPool::Create(device, numFrames);
	staticAllocatorPool = vke::DescriptorAllocatorPool::Create(device, 1);
	dynamicHandle = dynamicAllocatorPool->GetAllocator();
	staticHandle = staticAllocatorPool->GetAllocator();	
}

void DescriptorMegaPool::set_frame(int frameNumber)
{
	//dynamicHandle.Return();

	dynamicAllocatorPool->Flip();
	dynamicHandle = dynamicAllocatorPool->GetAllocator();
}

DescriptorSetBuilder::DescriptorSetBuilder(ShaderEffect* _effect, DescriptorMegaPool* _parentPool)
{
	effect = _effect;
	parentPool = _parentPool;
}

void DescriptorSetBuilder::bind_image_array(int set, int binding, vk::DescriptorImageInfo* images, int count)
{
	ImageWriteDescriptor newWrite;
	newWrite.dstSet = set;
	newWrite.dstBinding = binding;
	newWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
	newWrite.image_array = images;
	newWrite.image_count = count;
	//newWrite.imageInfo = imageInfo;
	//if (bImageWrite) {
	//	newWrite.imageInfo.imageLayout = vk::ImageLayout::eGeneral;
	//}

	imageWrites.push_back(newWrite);
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

void DescriptorSetBuilder::bind_raystructure(int set, int binding, const vk::WriteDescriptorSetAccelerationStructureKHR& info)
{

	BufferWriteDescriptor newWrite;
	newWrite.dstSet = set;
	newWrite.dstBinding = binding;
	newWrite.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
	newWrite.accelinfo = info;

	bufferWrites.push_back(newWrite);
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

			
			newWrite.pBufferInfo = nullptr;

			if (imageWrite.image_array == nullptr)
			{
				newWrite.descriptorCount = 1;
				newWrite.pImageInfo = &imageWrite.imageInfo; // Optional
			}
			else
			{
				newWrite.descriptorCount = imageWrite.image_count;
				newWrite.pImageInfo =imageWrite.image_array; // Optional
			}
		
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
			if (bufferWrite.descriptorType == vk::DescriptorType::eAccelerationStructureKHR) {
				newWrite.pBufferInfo = nullptr;
				newWrite.pNext = &bufferWrite.accelinfo;
			}
			else {
				newWrite.pBufferInfo = &bufferWrite.bufferInfo;
			}
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
	//std::vector<uint32_t> counts;
	//for (int i = 0; i < 10; i++)
	//{
	//	//counts.push_back(effect->privData->bindingSets[set].)
	//	//counts.push_back(4096);
	//}
	



	vk::DescriptorSet newSet = parentPool->allocate_descriptor(layout, lifetime,pNext);

	update_descriptor(set, newSet, parentPool->device);
	return newSet;
}
