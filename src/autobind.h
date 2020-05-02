#include <unordered_map>
#include "vulkan/vulkan_core.h"

class VulkanEngine;
struct DescriptorSetBuilder;
struct AutobindState {
	VulkanEngine* Engine;


	bool fill_descriptor(DescriptorSetBuilder* builder);


	bool find_image(const char* name, VkDescriptorImageInfo& outInfo);

	std::unordered_map<std::string, VkDescriptorImageInfo> image_infos;
	std::unordered_map<std::string, VkDescriptorBufferInfo> buffer_infos;
};