#include "cubemap_loader.h"
#include "vulkan_render.h"
#include "shader_processor.h"
#include "vulkan_pipelines.h"

const vk::Format cubemapFormat = vk::Format::eR32G32B32A32Sfloat;

struct CubemapOffscreenFramebuffer{
	vk::Image image;
	vk::ImageView view;
	//vk::DeviceMemory memory;
	vk::Framebuffer framebuffer;
};
void setImageLayout(
	VkCommandBuffer cmdbuffer,
	VkImage image,
	VkImageLayout oldImageLayout,
	VkImageLayout newImageLayout,
	VkImageSubresourceRange subresourceRange,
	VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
{
	// Create an image barrier object
	VkImageMemoryBarrier imageMemoryBarrier = vk::ImageMemoryBarrier{};
	imageMemoryBarrier.oldLayout = oldImageLayout;
	imageMemoryBarrier.newLayout = newImageLayout;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange = subresourceRange;

	// Source layouts (old)
	// Source access mask controls actions that have to be finished on the old layout
	// before it will be transitioned to the new layout
	switch (oldImageLayout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		// Image layout is undefined (or does not matter)
		// Only valid as initial layout
		// No flags required, listed only for completeness
		imageMemoryBarrier.srcAccessMask = 0;
		break;

	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		// Image is preinitialized
		// Only valid as initial layout for linear images, preserves memory contents
		// Make sure host writes have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		// Image is a color attachment
		// Make sure any writes to the color buffer have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		// Image is a depth/stencil attachment
		// Make sure any writes to the depth/stencil buffer have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		// Image is a transfer source 
		// Make sure any reads from the image have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		// Image is a transfer destination
		// Make sure any writes to the image have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		// Image is read by a shader
		// Make sure any shader reads from the image have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;
	default:
		// Other source layouts aren't handled (yet)
		break;
	}

	// Target layouts (new)
	// Destination access mask controls the dependency for the new image layout
	switch (newImageLayout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		// Image will be used as a transfer destination
		// Make sure any writes to the image have been finished
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		// Image will be used as a transfer source
		// Make sure any reads from the image have been finished
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		// Image will be used as a color attachment
		// Make sure any writes to the color buffer have been finished
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		// Image layout will be used as a depth/stencil attachment
		// Make sure any writes to depth/stencil buffer have been finished
		imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		// Image will be read in a shader (sampler, input attachment)
		// Make sure any writes to the image have been finished
		if (imageMemoryBarrier.srcAccessMask == 0)
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;
	default:
		// Other source layouts aren't handled (yet)
		break;
	}

	// Put barrier inside setup command buffer
	vkCmdPipelineBarrier(
		cmdbuffer,
		srcStageMask,
		dstStageMask,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemoryBarrier);
}

void setImageLayout(
	VkCommandBuffer cmdbuffer,
	VkImage image,
	VkImageAspectFlags aspectMask,
	VkImageLayout oldImageLayout,
	VkImageLayout newImageLayout,
	VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
{
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = aspectMask;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = 1;
	setImageLayout(cmdbuffer, image, oldImageLayout, newImageLayout, subresourceRange, srcStageMask, dstStageMask);
}

struct CubemapBlitVertex {
	glm::vec3 pos;

	static vk::VertexInputBindingDescription getBindingDescription() {
		vk::VertexInputBindingDescription bindingDescription;
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(CubemapBlitVertex);
		bindingDescription.inputRate = vk::VertexInputRate::eVertex;
		return bindingDescription;
	}

	static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions() {
		std::array<vk::VertexInputAttributeDescription, 4> attributeDescriptions;
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
		attributeDescriptions[0].offset = offsetof(CubemapBlitVertex, pos);

		return attributeDescriptions;
	}

	static vk::PipelineVertexInputStateCreateInfo getPipelineCreateInfo() {
		static auto bindingDescription = CubemapBlitVertex::getBindingDescription();
		static auto attributeDescriptions = CubemapBlitVertex::getAttributeDescriptions();

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
		return vertexInputInfo;
	}
};


void CubemapLoader::initialize(VulkanEngine* engine)
{
	const int32_t dim = 64;
	eng = engine;

	// FB, Att, RP, Pipe, etc.
	vk::AttachmentDescription attDesc = {};
	// Color attachment
	attDesc.format = cubemapFormat;
	attDesc.samples = vk::SampleCountFlagBits::e1;
	attDesc.loadOp = vk::AttachmentLoadOp::eClear;
	attDesc.storeOp = vk::AttachmentStoreOp::eStore;
	attDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	attDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attDesc.initialLayout = vk::ImageLayout::eUndefined;
	attDesc.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
	vk::AttachmentReference colorReference = { 0, vk::ImageLayout::eColorAttachmentOptimal };

	vk::SubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;

	// Use subpass dependencies for layout transitions
	std::array<vk::SubpassDependency, 2> dependencies;
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
	dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
	dependencies[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
	dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
	dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
	dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
	dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

	// Renderpass
	vk::RenderPassCreateInfo renderPassCI ={};
	renderPassCI.attachmentCount = 1;
	renderPassCI.pAttachments = &attDesc;
	renderPassCI.subpassCount = 1;
	renderPassCI.pSubpasses = &subpassDescription;
	renderPassCI.dependencyCount = 2;
	renderPassCI.pDependencies = dependencies.data();

	cubemapRenderPass = eng->device.createRenderPass(renderPassCI);


	//SHADERS
	irradianceRenderEffect = new ShaderEffect();

	irradianceRenderEffect->add_shader_from_file(MAKE_ASSET_PATH("shaders/cubemap/filtercube.vert"));
	irradianceRenderEffect->add_shader_from_file(MAKE_ASSET_PATH("shaders/cubemap/irradiancecube.frag"));

	irradianceRenderEffect->build_effect(eng->device);

	reflectionRenderEffect = new ShaderEffect();

	reflectionRenderEffect->add_shader_from_file(MAKE_ASSET_PATH("shaders/cubemap/filtercube.vert"));
	reflectionRenderEffect->add_shader_from_file(MAKE_ASSET_PATH("shaders/cubemap/prefilterenvmap.frag"));

	reflectionRenderEffect->build_effect(eng->device);
	
	//PIPELINE
	GraphicsPipelineBuilder pipBuilder = GraphicsPipelineBuilder();
	pipBuilder.data.vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{}; //CubemapBlitVertex::getPipelineCreateInfo();
	pipBuilder.data.inputAssembly = VkPipelineInitializers::build_input_assembly(vk::PrimitiveTopology::eTriangleList);
	pipBuilder.data.viewport = VkPipelineInitializers::build_viewport(dim, dim);
	pipBuilder.data.scissor = VkPipelineInitializers::build_rect2d(0, 0, dim, dim);
	pipBuilder.data.multisampling = VkPipelineInitializers::build_multisampling();
	pipBuilder.data.depthStencil = VkPipelineInitializers::build_depth_stencil(false, false, vk::CompareOp::eAlways);
	pipBuilder.data.rasterizer = VkPipelineInitializers::build_rasterizer();
	pipBuilder.data.rasterizer.cullMode = vk::CullModeFlagBits::eNone;
	//1 color attachments
	pipBuilder.data.colorAttachmentStates.push_back(VkPipelineInitializers::build_color_blend_attachment_state());
	
	pipBuilder.data.dynamicStates = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};
		

	//cubemapRenderEffect->set_manual_push_constants(pushConstantRanges.data(), 1);

	irradianceRenderPipeline = pipBuilder.build_pipeline(eng->device, cubemapRenderPass, 0, irradianceRenderEffect);
	reflectionRenderPipeline = pipBuilder.build_pipeline(eng->device, cubemapRenderPass, 0, reflectionRenderEffect);
}

TextureResource CubemapLoader::load_cubemap(const char* path, int32_t dim, CubemapFilterMode mode)
{	
	const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

	vk::ImageCreateInfo imageCI ={};
	imageCI.imageType = vk::ImageType::e2D;
	imageCI.format = cubemapFormat;
	imageCI.extent.width = dim;
	imageCI.extent.height = dim;
	imageCI.extent.depth = 1;
	imageCI.mipLevels = numMips;
	imageCI.arrayLayers = 6;
	imageCI.samples = vk::SampleCountFlagBits::e1;
	imageCI.tiling = vk::ImageTiling::eOptimal;
	imageCI.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
	imageCI.flags = vk::ImageCreateFlagBits::eCubeCompatible;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vmaallocInfo.requiredFlags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;

	VkImage img;
	VkImageCreateInfo imnfo = imageCI;
	VmaAllocation alloc;

	vmaCreateImage(eng->allocator, &imnfo, &vmaallocInfo, &img, &alloc, nullptr);

	vk::Image cubemapImage = img;
	vk::ImageViewCreateInfo viewInfo;
	viewInfo.image = img;
	viewInfo.viewType = vk::ImageViewType::eCube ;
	viewInfo.format = cubemapFormat;
	viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = numMips;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 6;

    vk::ImageView view = eng->device.createImageView(viewInfo);
	vk::ImageView cubemapView = view;

	vk::SamplerCreateInfo samplerCI;
	samplerCI.magFilter = vk::Filter::eLinear;
	samplerCI.minFilter = vk::Filter::eLinear;
	samplerCI.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	samplerCI.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	samplerCI.addressModeW = vk::SamplerAddressMode::eClampToEdge;

	samplerCI.anisotropyEnable = VK_TRUE;
	samplerCI.maxAnisotropy = 1;

	samplerCI.borderColor = vk::BorderColor::eFloatOpaqueWhite;
	samplerCI.unnormalizedCoordinates = VK_FALSE;

	samplerCI.compareEnable = VK_FALSE;
	samplerCI.compareOp = vk::CompareOp::eAlways;

	samplerCI.mipmapMode = vk::SamplerMipmapMode::eLinear;
	samplerCI.mipLodBias = 0.0f;
	samplerCI.minLod = 0.0f;
	samplerCI.maxLod = static_cast<float>(numMips);

	vk::Sampler sampler = eng->device.createSampler(samplerCI);

	create_offscreen_framebuffer(dim);

	//render

	vk::ClearValue clearValues[1];
	clearValues[0].color = vk::ClearColorValue { std::array<float, 4>{ 0.0f, 0.0f, 0.2f, 0.0f } };

	vk::RenderPassBeginInfo renderPassBeginInfo = {};
	// Reuse render pass from example pass
	renderPassBeginInfo.renderPass = cubemapRenderPass;
	renderPassBeginInfo.framebuffer = offscreen->framebuffer;
	renderPassBeginInfo.renderArea.extent.width = dim;
	renderPassBeginInfo.renderArea.extent.height = dim;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = clearValues;

	std::vector<glm::mat4> matrices = {
		// POSITIVE_X
		glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_X
		glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// POSITIVE_Y
		glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_Y
		glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// POSITIVE_Z
		glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_Z
		glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
	};

	vk::CommandBuffer cmd = eng->beginSingleTimeCommands();


	vk::Viewport viewport;
	viewport.height = (float)dim;
	viewport.width = (float)dim;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	cmd.setViewport(0, viewport);

	vk::Rect2D scissor;
	scissor.extent.height = (float)dim;
	scissor.extent.width = (float)dim;

	cmd.setScissor(0, scissor);

	vk::ImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = numMips;
	subresourceRange.layerCount = 6;

	//eng->cmd_transitionImageLayout(cmd, img, cubemapFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, subresourceRange);
	setImageLayout(
		cmd,
		cubemapImage,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		subresourceRange);
	// Pipeline layout
	struct PushBlock {
		glm::mat4 mvp;
		// Sampling deltas
		float deltaPhi = (2.0f * float(M_PI)) / 180.0f;
		float deltaTheta = (0.5f * float(M_PI)) / 64.0f;
	} irradiancePushBlock;

	struct PushBlock2 {
		glm::mat4 mvp;
		float roughness;
		uint32_t numSamples = 512u;
	} reflectionPushBlock;
	
	ShaderEffect* renderEffect;
	if (mode == CubemapFilterMode::IRRADIANCE) {
		renderEffect = irradianceRenderEffect;
	}
	else {
		renderEffect = reflectionRenderEffect;
	}
	vk::PipelineLayout pipelineLayout= renderEffect->build_pipeline_layout(eng->device);
	
	

	DescriptorSetBuilder setBuilder{ renderEffect,&eng->descriptorMegapool };

	std::pair<TextureResource, TextureResourceMetadata> envMap = eng->load_texture_resource(path,true);

	vk::DescriptorImageInfo envMapImage = {};
	envMapImage.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	envMapImage.imageView = envMap.first.imageView;
	envMapImage.sampler = envMap.first.textureSampler;

	setBuilder.bind_image(0, 0, envMapImage);


	

	vk::DescriptorSet envMapSet = setBuilder.build_descriptor(0, DescriptorLifetime::Static);
	
	for (uint32_t m = 0; m < numMips; m++) {
		for (uint32_t f = 0; f < 6; f++) {
			viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
			viewport.height = static_cast<float>(dim * std::pow(0.5f, m));


			reflectionPushBlock.roughness = (float)m / (float)(numMips - 1);

			cmd.setViewport(0, viewport);

			// Render scene from cube face's point of view
			cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);		
			
			if (mode == CubemapFilterMode::IRRADIANCE) {
				// Update shader push constant block
				irradiancePushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

				cmd.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushBlock), &irradiancePushBlock);

				cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, irradianceRenderPipeline);
			}
			else {
				
				// Update shader push constant block
				reflectionPushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

				cmd.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushBlock2), &reflectionPushBlock);

				cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, reflectionRenderPipeline);
				
			}
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &envMapSet, 0, nullptr);

			VkDeviceSize offsets[1] = { 0 };

			cmd.draw(12*3, 1, 0, 0);

			cmd.endRenderPass();

			setImageLayout(
				cmd,
				offscreen->image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			// Copy region for transfer from framebuffer to cube face
			VkImageCopy copyRegion = {};

			copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.srcSubresource.baseArrayLayer = 0;
			copyRegion.srcSubresource.mipLevel = 0;
			copyRegion.srcSubresource.layerCount = 1;
			copyRegion.srcOffset = { 0, 0, 0 };

			copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.dstSubresource.baseArrayLayer = f;
			copyRegion.dstSubresource.mipLevel = m;
			copyRegion.dstSubresource.layerCount = 1;
			copyRegion.dstOffset = { 0, 0, 0 };

			copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
			copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
			copyRegion.extent.depth = 1;

			cmd.copyImage(offscreen->image, vk::ImageLayout::eTransferSrcOptimal, cubemapImage,
				vk::ImageLayout::eTransferDstOptimal, { copyRegion });

			// Transform framebuffer color attachment back 
			setImageLayout(			
				cmd,
				offscreen->image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		}
	}

	//eng->cmd_transitionImageLayout(cmd, cubemapImage, cubemapFormat, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, subresourceRange);
	setImageLayout(
		cmd,
		cubemapImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		subresourceRange);

	eng->endSingleTimeCommands(cmd);

	TextureResource loaded;
	loaded.image.image = cubemapImage;
	loaded.image.allocation = alloc;
	loaded.imageView = cubemapView;
	loaded.textureSampler = sampler;

	return loaded;
}

void CubemapLoader::create_offscreen_framebuffer(int32_t dim)
{
	const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

	offscreen = new CubemapOffscreenFramebuffer();

	vk::ImageCreateInfo imageCI = {};
	imageCI.imageType = vk::ImageType::e2D;
	imageCI.format = cubemapFormat;
	imageCI.extent.width = dim;
	imageCI.extent.height = dim;
	imageCI.extent.depth = 1;
	imageCI.mipLevels = numMips;
	imageCI.arrayLayers = 6;
	imageCI.samples = vk::SampleCountFlagBits::e1;
	imageCI.tiling = vk::ImageTiling::eOptimal;
	imageCI.initialLayout = vk::ImageLayout::eUndefined;
	imageCI.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
	imageCI.flags = vk::ImageCreateFlagBits::eCubeCompatible;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vmaallocInfo.requiredFlags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;

	VkImage img;
	VkImageCreateInfo imnfo = imageCI;
	VmaAllocation alloc;

	vmaCreateImage(eng->allocator, &imnfo, &vmaallocInfo, &img, &alloc, nullptr);

	offscreen->image = img;

	vk::ImageViewCreateInfo colorImageView = {};
	colorImageView.viewType = vk::ImageViewType::e2D;
	colorImageView.format = cubemapFormat;
	colorImageView.flags = vk::ImageViewCreateFlagBits{};
	colorImageView.subresourceRange = {};
	colorImageView.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	colorImageView.subresourceRange.baseMipLevel = 0;
	colorImageView.subresourceRange.levelCount = 1;
	colorImageView.subresourceRange.baseArrayLayer = 0;
	colorImageView.subresourceRange.layerCount = 1;
	colorImageView.image = offscreen->image;

	offscreen->view = eng->device.createImageView(colorImageView);

	vk::FramebufferCreateInfo fbufCreateInfo;
	fbufCreateInfo.renderPass = cubemapRenderPass;
	fbufCreateInfo.attachmentCount = 1;
	fbufCreateInfo.pAttachments = &offscreen->view;
	fbufCreateInfo.width = dim;
	fbufCreateInfo.height = dim;
	fbufCreateInfo.layers = 1;

	offscreen->framebuffer = eng->device.createFramebuffer(fbufCreateInfo);

	auto cmd = eng->beginSingleTimeCommands();

	setImageLayout(
		cmd,
		offscreen->image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	//eng->cmd_transitionImageLayout(cmd, offscreen->image, cubemapFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

	eng->endSingleTimeCommands(cmd);
}
