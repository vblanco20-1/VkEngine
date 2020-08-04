// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#include <vk_device.h>


VkDescriptorPool vke::Device::createDescriptorPool(VkDevice device,VkDescriptorPoolCreateInfo* info)
{
	VkDescriptorPool descriptorPool;
	vkCreateDescriptorPool(device, info, nullptr, &descriptorPool);
	return descriptorPool;
}

void vke::Device::updateDescriptorSets(VkDevice device, Span<VkWriteDescriptorSet> writes)
{
	vkUpdateDescriptorSets(device, writes.count(), writes.first, 0, nullptr);
}
