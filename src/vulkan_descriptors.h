#pragma once
#include "vulkan_types.h"
#include "descriptor_allocator.h"

 
struct DescriptorUBOParams {
	VkBuffer buffer;
	size_t range;
};

struct DescriptorImageParams {
	VkImageLayout layout;
	VkImageView view;
	VkSampler sampler;


};
enum class DescriptorLifetime {
	Static,
	PerFrame
};


struct DescriptorMegaPool;
struct ShaderEffect;
struct DescriptorSetBuilder {

	struct ImageWriteDescriptor {
		int dstSet;
		int dstBinding;
		VkDescriptorType descriptorType;
		VkDescriptorImageInfo imageInfo;
		VkDescriptorImageInfo* image_array{nullptr};
		int image_count;
	};

	struct BufferWriteDescriptor {
		int dstSet;
		int dstBinding;
		VkDescriptorType descriptorType;
		VkDescriptorBufferInfo bufferInfo;
		VkWriteDescriptorSetAccelerationStructureKHR accelinfo;
	};

	DescriptorSetBuilder(ShaderEffect* _effect,DescriptorMegaPool *_parentPool);

	ShaderEffect* effect;
	DescriptorMegaPool* parentPool;


	void bind_image_array(int set, int binding, VkDescriptorImageInfo* images, int count);

	void bind_image(int set, int binding,const VkDescriptorImageInfo& imageInfo, bool bImageWrite = false);
	void bind_image(const char* name,const VkDescriptorImageInfo& imageInfo);

	void bind_buffer(int set, int binding,const VkDescriptorBufferInfo& bufferInfo, VkDescriptorType bufferType);
	void bind_buffer(const char* name, const VkDescriptorBufferInfo& bufferInfo);

	void bind_raystructure(int set, int binding, const VkWriteDescriptorSetAccelerationStructureKHR& info);

	void update_descriptor(int set, VkDescriptorSet& descriptor,const VkDevice& device);
	VkDescriptorSet build_descriptor(int set, DescriptorLifetime lifetime);

	std::vector<ImageWriteDescriptor> imageWrites;
	std::vector<BufferWriteDescriptor> bufferWrites;
};


struct DescriptorAllocator {
	int max_descriptors;
	int current_descriptors;
	VkDescriptorPool pool;
};
struct DescriptorMegaPool {

	VkDescriptorSet allocate_descriptor(VkDescriptorSetLayout layout, DescriptorLifetime lifetime, void* pNext = nullptr);
	
	void initialize(int numFrames, VkDevice _device);
	void set_frame(int frameNumber);

	vke::DescriptorAllocatorHandle dynamicHandle;
	vke::DescriptorAllocatorHandle staticHandle;

	vke::DescriptorAllocatorPool* dynamicAllocatorPool;
	vke::DescriptorAllocatorPool* staticAllocatorPool;

	VkDevice device;
};


inline VkDescriptorBufferInfo make_buffer_info(VkBuffer buffer, size_t size, uint32_t offset = 0) {
	VkDescriptorBufferInfo info{};
	info.buffer = buffer;
	info.offset = offset;
	info.range = size;
	return info;
}
template<typename T>
VkDescriptorBufferInfo make_buffer_info(const AllocatedBuffer& allocbuffer, uint32_t offset = 0) {
	return make_buffer_info(allocbuffer.buffer, sizeof(T), offset);
}


