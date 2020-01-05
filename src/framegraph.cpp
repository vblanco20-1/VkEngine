#include "framegraph.h"
#include <iostream>
#include <pcheader.h>
#include "vulkan_render.h"


std::array < vk::SubpassDependency, 2> build_basic_subpass_dependencies()
{
	std::array < vk::SubpassDependency, 2> dependencies;
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;

	dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eFragmentShader;
	dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests;

	dependencies[0].srcAccessMask = vk::AccessFlagBits::eShaderRead;
	dependencies[0].dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;

	dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;

	dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests;
	dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;

	dependencies[1].srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
	dependencies[1].dstAccessMask = vk::AccessFlagBits::eShaderRead;
	dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

	return dependencies;
}




void RenderPass::add_image_dependency(std::string name)
{
	image_dependencies.push_back(name);
}

void RenderPass::add_color_attachment(std::string name, const RenderAttachmentInfo& info)
{
	PassAttachment addinfo;
	addinfo.name = name;
	addinfo.info = info;

	color_attachments.push_back(addinfo);
}

void RenderPass::set_depth_attachment(std::string name, const RenderAttachmentInfo& info)
{
	PassAttachment addinfo;
	addinfo.name = name;
	addinfo.info = info;

	depth_attachment = addinfo;
}



vk::AttachmentDescription make_color_attachment(const RenderAttachmentInfo* info, bool bFirstWrite, bool bReadAfter) {

	vk::AttachmentDescription attachment;
	attachment.format = (vk::Format)info->format;
	attachment.samples = vk::SampleCountFlagBits::e1;


	if (bFirstWrite) {
		attachment.loadOp = info->bClear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eDontCare;
	}
	else {
		attachment.loadOp = vk::AttachmentLoadOp::eLoad;
	}


	attachment.storeOp = bReadAfter ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare;


	if (bFirstWrite) {
		attachment.initialLayout = vk::ImageLayout::eUndefined;
	}
	else {
		attachment.initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	}

	attachment.finalLayout = bReadAfter ? vk::ImageLayout::eShaderReadOnlyOptimal : vk::ImageLayout::eUndefined;

	return attachment;
}


vk::AttachmentDescription make_depth_attachment(const RenderAttachmentInfo* info, bool bFirstWrite, bool bReadAfter, bool bWrittenNext) {

	vk::AttachmentDescription attachment;
	attachment.format = (vk::Format)info->format;
	attachment.samples = vk::SampleCountFlagBits::e1;

	if (bFirstWrite) {
		attachment.loadOp = info->bClear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eDontCare;
	}
	else {
		attachment.loadOp = vk::AttachmentLoadOp::eLoad;
	}

	attachment.storeOp = (bReadAfter || bWrittenNext) ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare;


	if (bFirstWrite) {
		attachment.initialLayout = vk::ImageLayout::eUndefined;
	}
	else {
		attachment.initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	}

	if (bWrittenNext) {
		attachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	}
	else {
		attachment.finalLayout = bReadAfter ? vk::ImageLayout::eDepthStencilReadOnlyOptimal : vk::ImageLayout::eUndefined;
	}

	return attachment;
}


void build_render_pass(RenderPass* pass, VulkanEngine* eng)
{
	std::cout << "Pass Building : " << pass->name << "-----------------" << std::endl;

	std::vector<vk::AttachmentReference> references;
	std::vector<vk::AttachmentDescription> attachments;
	int color_attachments = 0;
	int depth_attachments = 0;

	bool bScreenOutput = 0;
	int index = 0;
	for (auto attachment : pass->physical_attachments) {

		vk::AttachmentReference athref;
		athref.attachment = attachment.index;
		athref.layout = attachment.bIsDepth ? vk::ImageLayout::eDepthStencilAttachmentOptimal : vk::ImageLayout::eColorAttachmentOptimal;
		if (attachment.bIsDepth) {
			depth_attachments++;
		}
		else {
			color_attachments++;
		}
		attachments.push_back(attachment.desc);
		references.push_back(athref);
	}


	vk::SubpassDescription subpass;
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = color_attachments;

	subpass.pColorAttachments = (color_attachments) ? references.data() : nullptr;

	subpass.pDepthStencilAttachment = (depth_attachments) ? (references.data() + color_attachments) : nullptr;;

	vk::RenderPassCreateInfo renderPassInfo;
	renderPassInfo.attachmentCount = attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	std::array < vk::SubpassDependency, 2> pass_dependencies = build_basic_subpass_dependencies();
	renderPassInfo.dependencyCount = static_cast<uint32_t>(pass_dependencies.size());
	renderPassInfo.pDependencies = pass_dependencies.data();

	pass->built_pass = eng->device.createRenderPass(renderPassInfo);
	//assert(pass->built_pass != VkRenderPass{});
}

bool FrameGraph::build(struct VulkanEngine* engine)
{
	//grab all render targets
	for (auto pass : passes) {
		for (const auto& coloratch : pass->color_attachments) {

			if (graph_attachments.find(coloratch.name) != graph_attachments.end())
			{
				graph_attachments[coloratch.name].writes++;
				graph_attachments[coloratch.name].last_writer_pass = pass->name;
			}
			else {

				GraphAttachment attachment;
				attachment.info = coloratch.info;
				attachment.name = coloratch.name;
				attachment.bIsDepth = false;
				attachment.writes = 1;
				attachment.reads = 0;
				attachment.usageFlags = (VkImageUsageFlags)vk::ImageUsageFlagBits::eColorAttachment;
				attachment.creator_pass = pass->name;
				attachment.last_writer_pass = pass->name;
				graph_attachments[coloratch.name] = attachment;
			}
		}
		auto& dp = pass->depth_attachment;

		if (dp.name != "") {
			if (graph_attachments.find(dp.name) != graph_attachments.end())
			{
				graph_attachments[dp.name].writes++;
				graph_attachments[dp.name].last_writer_pass = pass->name;
			}
			else {

				GraphAttachment attachment;
				attachment.info = dp.info;
				attachment.name = dp.name;
				attachment.bIsDepth = true;
				attachment.writes = 1;
				attachment.reads = 0;
				attachment.usageFlags = (VkImageUsageFlags)vk::ImageUsageFlagBits::eDepthStencilAttachment;
				attachment.creator_pass = pass->name;
				attachment.last_writer_pass = pass->name;
				graph_attachments[dp.name] = attachment;
			}
		}

	}

	//find reads
	for (auto pass : passes) {
		for (const auto& image : pass->image_dependencies) {
			if (graph_attachments.find(image) != graph_attachments.end())
			{
				graph_attachments[image].last_writer_pass = pass->name;
				graph_attachments[image].reads++;
				graph_attachments[image].usageFlags |= (VkImageUsageFlags)vk::ImageUsageFlagBits::eSampled;
			}
		}
	}

	for (auto [n, v] : graph_attachments) {
		std::cout << (v.bIsDepth ? " Depth Attachment: " : " Color Attachment: ") << n << " -- Reads " << v.reads << " Writes " << v.writes << std::endl;
	}


	//make attachments real
	for (auto pass : passes) {
		int attachmentIndex = 0;
		//color attachments
		for (const auto& ath : pass->color_attachments) {

			GraphAttachment* graphAth = &graph_attachments[ath.name];
			bool bAccessedAsReadAfter = graphAth->reads > 0 && graphAth->last_read_pass != pass->name;
			bool first_use = graphAth->creator_pass == pass->name;

			RenderPass::PhysicalAttachment physAttachment;
			physAttachment.bIsDepth = false;
			physAttachment.desc = make_color_attachment(&ath.info, first_use, bAccessedAsReadAfter);
			physAttachment.index = attachmentIndex;
			physAttachment.name = ath.name;


			pass->clearValues.push_back(ath.info.clearValue);

			pass->physical_attachments.push_back(physAttachment);

			//if (ath.info.bClear) {
			//	ath.
			//}

			attachmentIndex++;
		}

		if (pass->depth_attachment.name != "") {

			GraphAttachment* graphAth = &graph_attachments[pass->depth_attachment.name];
			bool bAccessedAsReadAfter = graphAth->reads > 0 && graphAth->last_read_pass != pass->name;
			bool first_use = graphAth->creator_pass == pass->name;
			bool bNextWrite = graphAth->last_writer_pass != pass->name;

			RenderPass::PhysicalAttachment physAttachment;
			physAttachment.bIsDepth = true;
			physAttachment.desc = make_depth_attachment(&pass->depth_attachment.info, first_use, bAccessedAsReadAfter, bNextWrite);
			physAttachment.index = attachmentIndex;
			physAttachment.name = pass->depth_attachment.name;


			pass->clearValues.push_back(pass->depth_attachment.info.clearValue);

			pass->physical_attachments.push_back(physAttachment);

			attachmentIndex++;
		}
	}

	//one sampler to rule them all
	vk::SamplerCreateInfo sampler;
	sampler.magFilter = vk::Filter::eNearest;
	sampler.minFilter = vk::Filter::eNearest;
	sampler.mipmapMode = vk::SamplerMipmapMode::eNearest;
	sampler.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	sampler.borderColor = vk::BorderColor::eFloatOpaqueWhite;

	VkSampler mainSampler = engine->device.createSampler(sampler);

	//build images
	for (auto [n, atch] : graph_attachments)
	{
		GraphAttachment& v = graph_attachments[n];
		vk::ImageCreateInfo imageCreateInfo;
		imageCreateInfo.imageType = vk::ImageType::e2D;

		if (v.info.size_class == SizeClass::SwapchainRelative) {
			imageCreateInfo.extent.width = v.info.size_x * this->swapchainSize.width;
			imageCreateInfo.extent.height = v.info.size_y * this->swapchainSize.height;
		}
		else {
			imageCreateInfo.extent.width = v.info.size_x;
			imageCreateInfo.extent.height = v.info.size_y;
		}
		v.real_height = imageCreateInfo.extent.height;
		v.real_width = imageCreateInfo.extent.width;

		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
		imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
		imageCreateInfo.format = (vk::Format)v.info.format;
		imageCreateInfo.usage = (vk::ImageUsageFlagBits)v.usageFlags;

		VmaAllocationCreateInfo vmaallocInfo = {};
		vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		vmaallocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkImageCreateInfo imnfo1 = imageCreateInfo;

		VkImage image;

		vmaCreateImage(engine->allocator, &imnfo1, &vmaallocInfo, &image, &v.imageAlloc, nullptr);

		v.image = image;

		vk::ImageViewCreateInfo imageViewInfo;
		imageViewInfo.viewType = vk::ImageViewType::e2D;
		imageViewInfo.format = (vk::Format)v.info.format;
		imageViewInfo.subresourceRange = {};
		imageViewInfo.subresourceRange.aspectMask = v.bIsDepth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 1;
		imageViewInfo.image = image;

		v.descriptor.imageView = engine->device.createImageView(imageViewInfo);
		v.descriptor.imageLayout = VkImageLayout(v.bIsDepth ? vk::ImageLayout::eDepthStencilReadOnlyOptimal : vk::ImageLayout::eShaderReadOnlyOptimal);
		v.descriptor.sampler = mainSampler;
	}

	//make render passes
	for (int i = 0; i < passes.size(); i++) {
		RenderPass* pass = passes[i];
		build_render_pass(pass, engine);

		//framebuffers
		std::vector<vk::ImageView> FBAttachments;
		//FBAttachments.resize(pass->physical_attachments.size());

		int width;
		int height;

		for (auto attachment : pass->physical_attachments) {

			FBAttachments.push_back(graph_attachments[attachment.name].descriptor.imageView);

			width = graph_attachments[attachment.name].real_width;
			height = graph_attachments[attachment.name].real_height;
		}

		pass->render_height = height;
		pass->render_width = width;

		vk::FramebufferCreateInfo fbufCreateInfo;
		fbufCreateInfo.renderPass = pass->built_pass;
		fbufCreateInfo.attachmentCount = FBAttachments.size();
		fbufCreateInfo.pAttachments = FBAttachments.data();
		fbufCreateInfo.width = width;
		fbufCreateInfo.height = height;
		fbufCreateInfo.layers = 1;

		std::cout << "building framebuffer for" << pass->name << std::endl;
		pass->framebuffer = engine->device.createFramebuffer(fbufCreateInfo);
	}

	std::cout << "framegraph built";

	for (auto [n, atch] : graph_attachments)
	{
		//leaks
		std::string* alloc = new std::string(n);
		attachmentNames.push_back(alloc->c_str());
	}

	return true;
}

RenderPass* FrameGraph::add_pass(std::string pass_name, std::function<void(vk::CommandBuffer, RenderPass*)> execution, PassType type)
{
	pass_definitions[pass_name] = RenderPass();

	RenderPass* ptr = &pass_definitions[pass_name];
	ptr->name = pass_name;
	ptr->owner = this;
	ptr->draw_callback = execution;
	passes.push_back(ptr);

	return ptr;
}

RenderPass* FrameGraph::get_pass(std::string pass_name)
{
	return &pass_definitions[pass_name];
}

VkDescriptorImageInfo FrameGraph::get_image_descriptor(std::string name)
{
	return get_attachment(name)->descriptor;
}

FrameGraph::GraphAttachment* FrameGraph::get_attachment(std::string name)
{
	return &graph_attachments[name];
}



void FrameGraph::execute(vk::CommandBuffer cmd)
{
	for (auto pass : passes) {

		if (pass->draw_callback)
		{

			vk::RenderPassBeginInfo renderPassInfo;
			renderPassInfo.renderPass = pass->built_pass;
			renderPassInfo.framebuffer = pass->framebuffer;
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent.width = pass->render_width;
			renderPassInfo.renderArea.extent.height = pass->render_height;

			renderPassInfo.clearValueCount = pass->clearValues.size();
			renderPassInfo.pClearValues = (vk::ClearValue*)pass->clearValues.data();

			cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

			pass->draw_callback(cmd, pass);

			cmd.endRenderPass();
		}
	}
}

void RenderAttachmentInfo::set_clear_color(std::array<float, 4> col)
{
	clearValue.color = vk::ClearColorValue{ col };
	bClear = true;
}

void RenderAttachmentInfo::set_clear_depth(float depth, uint32_t stencil)
{
	clearValue.depthStencil = { depth, stencil };
	bClear = true;
}
