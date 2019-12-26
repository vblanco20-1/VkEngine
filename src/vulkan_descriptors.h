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
		vk::DescriptorType descriptorType;
		vk::DescriptorImageInfo imageInfo;
	};

	struct BufferWriteDescriptor {
		int dstSet;
		int dstBinding;
		vk::DescriptorType descriptorType;
		vk::DescriptorBufferInfo bufferInfo;
	};

	DescriptorSetBuilder(ShaderEffect* _effect,DescriptorMegaPool *_parentPool);

	ShaderEffect* effect;
	DescriptorMegaPool* parentPool;


	void bind_image(int set, int binding,const vk::DescriptorImageInfo& imageInfo);
	void bind_image(const char* name,const vk::DescriptorImageInfo& imageInfo);

	void bind_buffer(int set, int binding,const vk::DescriptorBufferInfo& bufferInfo, vk::DescriptorType bufferType);
	void bind_buffer(const char* name, const vk::DescriptorBufferInfo& bufferInfo);

	void update_descriptor(int set, vk::DescriptorSet& descriptor,const vk::Device& device);
	vk::DescriptorSet build_descriptor(int set, DescriptorLifetime lifetime);

	std::vector<ImageWriteDescriptor> imageWrites;
	std::vector<BufferWriteDescriptor> bufferWrites;
};


struct DescriptorAllocator {
	int max_descriptors;
	int current_descriptors;
	vk::DescriptorPool pool;
};
struct DescriptorMegaPool {

	using PoolStorage = std::vector<DescriptorAllocator*>;

	vk::DescriptorSet allocate_descriptor(vk::DescriptorSetLayout layout, DescriptorLifetime lifetime);
	DescriptorAllocator* get_allocator();

	void initialize(int numFrames, vk::Device _device);
	void set_frame(int frameNumber);

	int currentFrame;
	PoolStorage static_pools;
	//one per frame
	std::vector<PoolStorage*> dynamic_pools;

	PoolStorage empty_pools;

	vk::Device device;
};





