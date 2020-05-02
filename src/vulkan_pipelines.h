#pragma once
#include "vulkan_types.h"

#include "vulkan_render.h"

namespace VkPipelineInitializers {

	vk::Viewport build_viewport(uint32_t width, uint32_t height);

	vk::PipelineViewportStateCreateInfo build_viewport_state(vk::Viewport* viewport, vk::Rect2D* scissor);

	vk::Rect2D build_rect2d(int32_t x, int32_t y, uint32_t width, uint32_t height);

	vk::PipelineInputAssemblyStateCreateInfo build_input_assembly(vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList, bool bPrimitiveRestart = false);

	vk::PipelineDepthStencilStateCreateInfo build_depth_stencil(bool bTestEnable, bool bWriteEnable, vk::CompareOp compareOP = vk::CompareOp::eLessOrEqual);

	vk::PipelineRasterizationStateCreateInfo build_rasterizer(vk::PolygonMode polygonMode =vk::PolygonMode::eFill);

	vk::PipelineMultisampleStateCreateInfo build_multisampling();

	vk::PipelineColorBlendAttachmentState build_color_blend_attachment_state();

	vk::PipelineColorBlendStateCreateInfo build_color_blend(vk::PipelineColorBlendAttachmentState* colorAttachments, int attachmentCount);

	vk::PipelineVertexInputStateCreateInfo build_empty_vertex_input();
}
struct ShaderEffect;


struct GraphicsPipelineBuilder {
	struct VulkanInfos {
		vk::Viewport viewport;
		vk::Rect2D scissor;
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
		vk::PipelineDepthStencilStateCreateInfo depthStencil;
		vk::PipelineRasterizationStateCreateInfo rasterizer;
		vk::PipelineMultisampleStateCreateInfo multisampling;
		std::vector< vk::DynamicState> dynamicStates;
		std::vector < vk::PipelineColorBlendAttachmentState> colorAttachmentStates;
	};

	VulkanInfos data;

	vk::Pipeline build_pipeline(vk::Device device, vk::RenderPass renderPass, uint32_t subpass, ShaderEffect* shaderEffect);
};

struct ComputePipelineBuilder {
	struct VulkanInfos {
	};

	vk::Pipeline build_pipeline(vk::Device device,ShaderEffect* shaderEffect);
};