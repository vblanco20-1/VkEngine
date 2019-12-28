#include "vulkan_pipelines.h"

vk::Viewport VkPipelineInitializers::build_viewport(uint32_t width, uint32_t height)
{
	vk::Viewport viewport;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	return viewport;
}

vk::PipelineViewportStateCreateInfo VkPipelineInitializers::build_viewport_state(vk::Viewport* viewport, vk::Rect2D* scissor)
{
	vk::PipelineViewportStateCreateInfo viewportState;
	viewportState.viewportCount = 1;
	viewportState.pViewports = viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors =scissor;
	return viewportState;
}

vk::Rect2D VkPipelineInitializers::build_rect2d(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
	vk::Rect2D rect;
	rect.offset = { x, y };
	rect.extent = {width,height};
	return rect;
}

vk::PipelineInputAssemblyStateCreateInfo VkPipelineInitializers::build_input_assembly(vk::PrimitiveTopology topology, bool bPrimitiveRestart)
{
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
	inputAssembly.topology = topology;
	inputAssembly.primitiveRestartEnable = bPrimitiveRestart ? VK_TRUE : VK_FALSE;

	return inputAssembly;
}

vk::PipelineDepthStencilStateCreateInfo VkPipelineInitializers::build_depth_stencil(bool bTestEnable, bool bWriteEnable, vk::CompareOp compareOP)
{
	vk::PipelineDepthStencilStateCreateInfo depthStencil;
	depthStencil.depthTestEnable = bTestEnable ? VK_TRUE : VK_FALSE;
	depthStencil.depthWriteEnable = bWriteEnable ? VK_TRUE : VK_FALSE;
	depthStencil.depthCompareOp = compareOP;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {}; // Optional
	depthStencil.back = {}; // Optional

	return depthStencil;
}

vk::PipelineRasterizationStateCreateInfo VkPipelineInitializers::build_rasterizer(vk::PolygonMode polygonMode)
{
	vk::PipelineRasterizationStateCreateInfo rasterizer;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = polygonMode;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = vk::CullModeFlagBits::eBack;
	rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
	rasterizer.depthBiasEnable = VK_TRUE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	return rasterizer;
}

vk::PipelineMultisampleStateCreateInfo VkPipelineInitializers::build_multisampling()
{
	vk::PipelineMultisampleStateCreateInfo multisampling;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional
	return multisampling;
}

vk::PipelineColorBlendAttachmentState VkPipelineInitializers::build_color_blend_attachment_state()
{
	vk::PipelineColorBlendAttachmentState colorBlendAttachment;
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eOne; // Optional
	colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eZero; // Optional
	colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd; // Optional
	colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;// Optional
	colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero; // Optional
	colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd; // Optional

	return colorBlendAttachment;
}

vk::PipelineColorBlendStateCreateInfo VkPipelineInitializers::build_color_blend(vk::PipelineColorBlendAttachmentState* colorAttachment)
{
	vk::PipelineColorBlendStateCreateInfo colorBlending;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = vk::LogicOp::eCopy; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = colorAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional
	return colorBlending;
}
