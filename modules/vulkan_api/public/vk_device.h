// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vulkan/vulkan_core.h>
#include <vk_initializers.h>
namespace vke {

	namespace Device {

		VkDescriptorPool createDescriptorPool(VkDevice device,VkDescriptorPoolCreateInfo* info);

		void updateDescriptorSets(VkDevice device, Span<VkWriteDescriptorSet> writes);
	};
}