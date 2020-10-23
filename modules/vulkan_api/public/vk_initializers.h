// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_flags.h>


namespace vke {
	template<typename T>
	struct Span {
		T* first;
		T* last;
		uint32_t count() const {
			return static_cast<uint32_t>( last - first);
		}
		Span(T* object) : first(object) {
			last = object + 1;
		}
		Span(T* _first, size_t _count) :first(_first) {
			last = first + _count;
		}
		Span(T* _first, T* _last) :first(_first) , last(_last) {};
		Span() : first(nullptr), last(nullptr){			
		}
	};
}

namespace vkinit {

	VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(vkf::ShaderStageFlagBits stage, VkShaderModule module);

	VkPipelineLayoutCreateInfo pipeline_layout_create_info();

	VkCommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolResetFlags flags = 0);

	VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags = 0);

	VkFramebufferCreateInfo framebuffer_create_info(VkRenderPass renderPass, VkExtent2D extent);

	VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags = 0);

	VkSemaphoreCreateInfo semaphore_create_info();

	VkSubmitInfo submit_info(VkCommandBuffer* cmd);

	VkPresentInfoKHR present_info();

	VkRenderPassBeginInfo renderpass_begin_info(VkRenderPass renderPass, VkExtent2D windowExtent, VkFramebuffer framebuffer);

	VkPipelineMultisampleStateCreateInfo multisampling_state_create_info();

	VkPipelineRasterizationStateCreateInfo rasterization_state_create_info(VkPolygonMode polygonMode);

	VkPipelineColorBlendAttachmentState color_blend_attachment_state();

	VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info(VkPrimitiveTopology topology);

	VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info();

	VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp);

	VkImageCreateInfo image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);

	VkDescriptorSetAllocateInfo descriptor_allocate_info(VkDescriptorPool pool, VkDescriptorSetLayout* layout);

	VkDescriptorPoolCreateInfo descriptor_pool_create_info(vke::Span<VkDescriptorPoolSize> poolSizes,uint32_t maxSets, VkDescriptorPoolCreateFlags flags = 0);


	VkWriteDescriptorSet descriptor_write_buffer(VkDescriptorSet dstSet, uint32_t dstBinding , VkDescriptorBufferInfo * pBufferInfo, VkDescriptorType type);

	VkWriteDescriptorSet descriptor_write_image(VkDescriptorSet dstSet, uint32_t dstBinding, VkDescriptorImageInfo* pImageInfo, VkDescriptorType type);

	VkImageViewCreateInfo image_view_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

	VkSamplerCreateInfo sampler_create_info(VkFilter filters, VkSamplerAddressMode samplerAdressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);

	//template<typename T>
	//VkDescriptorBufferInfo descriptor_buffer_info(const AllocatedBuffer &buffer,VkDeviceSize  offset = 0) {
	//
	//	VkDescriptorBufferInfo bufferInfo;
	//	bufferInfo.buffer = buffer._buffer;
	//	bufferInfo.offset = offset;
	//	bufferInfo.range = sizeof(T);
	//	return bufferInfo;
	//}
}

