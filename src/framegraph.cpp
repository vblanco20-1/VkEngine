#include "framegraph.h"
#include <iostream>
#include <pcheader.h>
#include "vulkan_render.h"
#include <vk_flags.h>
#include <vk_initializers.h>

std::array < VkSubpassDependency, 2> build_basic_subpass_dependencies()
{
	std::array < VkSubpassDependency, 2> dependencies;
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;

	dependencies[0].srcStageMask = (VkPipelineStageFlags)vkf::PipelineStageFlagBits::eFragmentShader;
	dependencies[0].dstStageMask = (VkPipelineStageFlags)vkf::PipelineStageFlagBits::eEarlyFragmentTests;

	dependencies[0].srcAccessMask = (VkAccessFlagBits)vkf::AccessFlagBits::eShaderRead;
	dependencies[0].dstAccessMask = (VkAccessFlagBits)vkf::AccessFlagBits::eDepthStencilAttachmentWrite;

	dependencies[0].dependencyFlags = (VkDependencyFlags)vkf::DependencyFlagBits::eByRegion;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;

	dependencies[1].srcStageMask = (VkPipelineStageFlags)vkf::PipelineStageFlagBits::eLateFragmentTests;
	dependencies[1].dstStageMask = (VkPipelineStageFlags)vkf::PipelineStageFlagBits::eFragmentShader;

	dependencies[1].srcAccessMask = (VkAccessFlagBits)vkf::AccessFlagBits::eDepthStencilAttachmentWrite;
	dependencies[1].dstAccessMask = (VkAccessFlagBits)vkf::AccessFlagBits::eShaderRead;
	dependencies[1].dependencyFlags = (VkDependencyFlags)vkf::DependencyFlagBits::eByRegion;

	return dependencies;
}




bool RenderPass::find_image_access(std::string name, RenderGraphImageAccess & outAccess)
{

	for (const ImageAttachment &im : image_dependencies)
	{
		if (im.name.compare(name) == 0) {
			if (im.access == RenderGraphResourceAccess::Read) {
				outAccess = RenderGraphImageAccess::Read; 
				return true;
			}
			else {
				outAccess= RenderGraphImageAccess::Write; 
				return true;
			}
		}
	}
	for (const PassAttachment &im : color_attachments)
	{
		if (im.name.compare(name) == 0) {
			if (type == PassType::Graphics) {
				outAccess = RenderGraphImageAccess::RenderTargetColor;
			}
			else {
				outAccess = RenderGraphImageAccess::Write;
			}
			
			return true;
		}
	}
	if (depth_attachment.name.compare(name) == 0) {
		outAccess = RenderGraphImageAccess::RenderTargetDepth; 
		return true;
	}

	return false;
}

void RenderPass::add_image_dependency(std::string name, RenderGraphResourceAccess accessMode)
{
	image_dependencies.push_back({ name,accessMode });
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

	VkAttachmentDescription attachment = {};
	attachment.format = info->format;
	attachment.samples = (VkSampleCountFlagBits)vkf::SampleCountFlagBits::e1;


	if (bFirstWrite) {
		attachment.loadOp = (VkAttachmentLoadOp)(info->bClear ? vkf::AttachmentLoadOp::eClear : vkf::AttachmentLoadOp::eDontCare);
	}
	else {
		attachment.loadOp = (VkAttachmentLoadOp)vkf::AttachmentLoadOp::eLoad;
	}


	attachment.storeOp = bReadAfter ? (VkAttachmentStoreOp)vkf::AttachmentStoreOp::eStore : (VkAttachmentStoreOp)vkf::AttachmentStoreOp::eDontCare;


	if (bFirstWrite) {
		attachment.initialLayout = (VkImageLayout)vkf::ImageLayout::eUndefined;
	}
	else {
		attachment.initialLayout = (VkImageLayout)vkf::ImageLayout::eShaderReadOnlyOptimal;
	}

	attachment.finalLayout = bReadAfter ? (VkImageLayout)vkf::ImageLayout::eShaderReadOnlyOptimal : (VkImageLayout)vkf::ImageLayout::eColorAttachmentOptimal;

	return attachment;
}


VkAttachmentDescription make_depth_attachment(const RenderAttachmentInfo* info, bool bFirstWrite, bool bReadAfter, bool bWrittenNext) {

	VkAttachmentDescription attachment = {};
	
	attachment.format = info->format;
	attachment.samples = (VkSampleCountFlagBits)vkf::SampleCountFlagBits::e1;

	if (bFirstWrite) {
		attachment.loadOp = info->bClear ? (VkAttachmentLoadOp)vkf::AttachmentLoadOp::eClear : (VkAttachmentLoadOp)vkf::AttachmentLoadOp::eDontCare;
	}
	else {
		attachment.loadOp = (VkAttachmentLoadOp)vkf::AttachmentLoadOp::eLoad;
	}

	auto storeOp = (bReadAfter || bWrittenNext) ? vkf::AttachmentStoreOp::eStore : vkf::AttachmentStoreOp::eDontCare;
	attachment.storeOp = (VkAttachmentStoreOp)storeOp;


	if (bFirstWrite) {
		attachment.initialLayout = +vkf::ImageLayout::eUndefined;
	}
	else {
		attachment.initialLayout = +vkf::ImageLayout::eDepthStencilAttachmentOptimal;
	}

	if (bWrittenNext) {
		attachment.finalLayout = +vkf::ImageLayout::eDepthStencilAttachmentOptimal;
	}
	else {
		auto finalLayout = bReadAfter ? vkf::ImageLayout::eDepthStencilReadOnlyOptimal : vkf::ImageLayout::eDepthStencilAttachmentOptimal;
		attachment.finalLayout = +finalLayout;
	}

	return attachment;
}

int FrameGraph::find_attachment_user(const FrameGraph::GraphAttachmentUsages&usages, std::string name) {
	for (int i = 0; i < usages.users.size(); i++) {

		if (passes[usages.users[i]]->name.compare(name) == 0) {
			return i;
		}
	}
	return-1;
}

void FrameGraph::build_render_pass(RenderPass* pass, VulkanEngine* eng)
{
	int attachmentIndex = 0;
	for (const auto& ath : pass->color_attachments) {

		GraphAttachment* graphAth = &graph_attachments[ath.name];

		int useridx = find_attachment_user(graphAth->uses, pass->name);
		int numusers = graphAth->uses.users.size();
		bool bAccessedAsReadAfter = false;
		//if (useridx + 1 < numusers) {

		if (graphAth->uses.usages[(useridx + 1) % numusers] == RenderGraphImageAccess::Read) {
			bAccessedAsReadAfter = true;
		}
		//}

		bool first_use = (useridx == 0);

		RenderPass::PhysicalAttachment physAttachment;
		physAttachment.bIsDepth = false;
		physAttachment.desc = make_color_attachment(&ath.info, first_use, bAccessedAsReadAfter);
		physAttachment.index = attachmentIndex;
		physAttachment.name = ath.name;


		pass->clearValues.push_back(ath.info.clearValue);

		pass->physical_attachments.push_back(physAttachment);

		attachmentIndex++;
	}

	if (pass->depth_attachment.name != "") {

		GraphAttachment* graphAth = &graph_attachments[pass->depth_attachment.name];

		int useridx = find_attachment_user(graphAth->uses, pass->name);
		int numusers = graphAth->uses.users.size();
		bool bAccessedAsReadAfter = false;
		bool bNextWrite = false;
		bool first_use = (useridx == 0);
		if (useridx + 1 < numusers) {

			if (graphAth->uses.usages[useridx + 1] == RenderGraphImageAccess::Read) {
				bAccessedAsReadAfter = true;
			}
			else if (graphAth->uses.usages[useridx + 1] == RenderGraphImageAccess::RenderTargetDepth) {
				bNextWrite = true;
			}
		}

		RenderPass::PhysicalAttachment physAttachment;
		physAttachment.bIsDepth = true;
		physAttachment.desc = make_depth_attachment(&pass->depth_attachment.info, first_use, bAccessedAsReadAfter, bNextWrite);
		physAttachment.index = attachmentIndex;
		physAttachment.name = pass->depth_attachment.name;


		pass->clearValues.push_back(pass->depth_attachment.info.clearValue);

		pass->physical_attachments.push_back(physAttachment);

		attachmentIndex++;
	}

	std::cout << "Pass Building : " << pass->name << "-----------------" << std::endl;

	std::vector<VkAttachmentReference> references;
	std::vector<VkAttachmentDescription> attachments;
	int color_attachments = 0;
	int depth_attachments = 0;

	bool bScreenOutput = 0;
	int index = 0;
	for (auto attachment : pass->physical_attachments) {

		VkAttachmentReference athref = {};

		athref.attachment = attachment.index;
		athref.layout = (VkImageLayout)(attachment.bIsDepth ? vkf::ImageLayout::eDepthStencilAttachmentOptimal : vkf::ImageLayout::eColorAttachmentOptimal);
		if (attachment.bIsDepth) {
			depth_attachments++;
		}
		else {
			color_attachments++;
		}
		attachments.push_back(attachment.desc);
		references.push_back(athref);
	}


	VkSubpassDescription subpass = {};

	subpass.pipelineBindPoint = (VkPipelineBindPoint)vkf::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = color_attachments;

	subpass.pColorAttachments = (color_attachments) ? references.data() : nullptr;

	subpass.pDepthStencilAttachment = (depth_attachments) ? (references.data() + color_attachments) : nullptr;;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	std::array < VkSubpassDependency, 2> pass_dependencies = build_basic_subpass_dependencies();
	renderPassInfo.dependencyCount = static_cast<uint32_t>(pass_dependencies.size());
	renderPassInfo.pDependencies = pass_dependencies.data();

	pass->built_pass = eng->device.createRenderPass(renderPassInfo);

	//framebuffers
	std::vector<VkImageView> FBAttachments;

	uint32_t width = 0;
	uint32_t height = 0;

	for (auto attachment : pass->physical_attachments) {

		FBAttachments.push_back(graph_attachments[attachment.name].descriptor.imageView);

		width = graph_attachments[attachment.name].real_width;
		height = graph_attachments[attachment.name].real_height;
	}

	pass->render_height = height;
	pass->render_width = width;

	VkFramebufferCreateInfo fbufCreateInfo = vkinit::framebuffer_create_info(pass->built_pass, {width,height});	

	fbufCreateInfo.attachmentCount = FBAttachments.size();
	fbufCreateInfo.pAttachments = FBAttachments.data();
	fbufCreateInfo.width = width;
	fbufCreateInfo.height = height;
	fbufCreateInfo.layers = 1;

	std::cout << "building framebuffer for" << pass->name << std::endl;
	pass->framebuffer = eng->device.createFramebuffer(fbufCreateInfo);
}

void FrameGraph::build_compute_barriers(RenderPass* pass, VulkanEngine* eng)
{
	for (const auto& ath : pass->color_attachments) {
		GraphAttachment* graphAth = &graph_attachments[ath.name];

		VkImageSubresourceRange range = {};
		range.aspectMask = +vkf::ImageAspectFlagBits::eColor;
		range.baseMipLevel = 0;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		int useridx = find_attachment_user(graphAth->uses, pass->name);
		int numusers = graphAth->uses.users.size();

		bool bFirstUse = useridx == 0;

		VkImageLayout current_layout = +vkf::ImageLayout::eGeneral;
	
		

		//we only add barrier when using the image for the first time, barriers are handled in the other cases
		if (bFirstUse) {

			// START BARRIER
			VkImageMemoryBarrier startbarrier = {};
			startbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			startbarrier.oldLayout = +vkf::ImageLayout::eUndefined;
			startbarrier.newLayout = current_layout;

			startbarrier.dstAccessMask = +vkf::AccessFlagBits::eShaderWrite;
			startbarrier.srcAccessMask = +vkf::AccessFlagBits{};

			startbarrier.image = graphAth->image;
			startbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			startbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			startbarrier.subresourceRange = range;
			pass->startBarriers.push_back(startbarrier);
		}

		

		//find what is the next user
		if (useridx + 1 < numusers) {

			VkImageLayout next_layout = +vkf::ImageLayout::eUndefined;

			RenderGraphImageAccess nextUsage = graphAth->uses.usages[useridx + 1];
			RenderPass* nextUser = passes[graphAth->uses.users[useridx + 1]];
		
			if (nextUser->type == PassType::Graphics) {
				if (nextUsage == RenderGraphImageAccess::Read) {
					next_layout = +vkf::ImageLayout::eShaderReadOnlyOptimal;
				}
			}
			else {
				if (nextUsage == RenderGraphImageAccess::Read) {
					next_layout = +vkf::ImageLayout::eShaderReadOnlyOptimal;
				}
				else {
					next_layout = +vkf::ImageLayout::eGeneral;
				}
			}		
			

			// END BARRIER
			VkImageMemoryBarrier endbarrier = {};
			endbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			endbarrier.oldLayout = current_layout;
			endbarrier.newLayout = next_layout;

			endbarrier.dstAccessMask = +vkf::AccessFlagBits::eShaderRead;
			endbarrier.srcAccessMask = +vkf::AccessFlagBits::eShaderWrite;

			endbarrier.image = graphAth->image;
			endbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			endbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			endbarrier.subresourceRange = range;

			pass->endBarriers.push_back(endbarrier);
		}
	}
}

bool FrameGraph::build( VulkanEngine* engine)
{
	owner = engine;

	for (auto pass : passes) {
		for (const auto& coloratch : pass->color_attachments) {
			if (graph_attachments.find(coloratch.name) == graph_attachments.end())
			{
				GraphAttachment attachment;
				attachment.info = coloratch.info;
				attachment.name = coloratch.name;
				
				graph_attachments[coloratch.name] = attachment;
			}
		}
		auto& dp = pass->depth_attachment;
		if (dp.name != "") {
			if (graph_attachments.find(dp.name) == graph_attachments.end())
			{	
				GraphAttachment attachment;
				attachment.info = dp.info;
				attachment.name = dp.name;
				
				graph_attachments[dp.name] = attachment;
			}
		}
	}

	//grab all render targets
	for (auto &[name,attachment] : graph_attachments) {
		
		std::cout << "----------------------------" << std::endl;
		for (int i = 0; i < passes.size(); i++) {
			RenderPass* pass = passes[i];
			RenderGraphImageAccess access;
			if (pass->find_image_access(name, access))
			{
				attachment.uses.usages.push_back(access);
				attachment.uses.users.push_back(i);

				std::cout << "attachment: " << name << " used by " << pass->name << " as ";
				switch (access) {
				case RenderGraphImageAccess::Write:
					std::cout << "write";
					break;
				case RenderGraphImageAccess::Read:
					std::cout << "read";
					break;
				case RenderGraphImageAccess::RenderTargetColor:
					std::cout << "render target";
					break;
				case RenderGraphImageAccess::RenderTargetDepth:
					std::cout << "depth target";
					break;								
				}
				std::cout << std::endl;
			}
		}
		std::cout << std::endl;
	}
	for (auto [name, attachment] : graph_attachments) {
		std::cout << (is_used_as_depth(&attachment) ? " Depth Attachment: " : " Color Attachment: ") << name << " -- Reads " << get_reads(&attachment) << " Writes " << get_writes(&attachment) << std::endl;
	}

	//calculate usage flags
	for (auto &[name, attachment] : graph_attachments) {
		VkImageUsageFlags flags{0};
		

		for (int i = 0; i < attachment.uses.usages.size(); i++) {
			RenderGraphImageAccess usage = attachment.uses.usages[i];
			RenderPass* pass = passes[attachment.uses.users[i]];
			
			switch (usage) {
			case RenderGraphImageAccess::Write:
				if (pass->type == PassType::Compute) {
					flags |= +vkf::ImageUsageFlagBits::eStorage;
				}
				break;
			case RenderGraphImageAccess::Read:
				flags |= +vkf::ImageUsageFlagBits::eSampled;
				break;
			case RenderGraphImageAccess::RenderTargetColor:
				flags |= +vkf::ImageUsageFlagBits::eColorAttachment;
				break;
			case RenderGraphImageAccess::RenderTargetDepth:
				flags |= +vkf::ImageUsageFlagBits::eDepthStencilAttachment;
				break;				
			}			
		}

		attachment.usageFlags = flags;
	}
	
	//one sampler to rule them all
	VkSamplerCreateInfo sampler = vkinit::sampler_create_info(+vkf::Filter::eLinear);
	sampler.magFilter = +vkf::Filter::eLinear;
	sampler.minFilter = +vkf::Filter::eLinear;
	sampler.mipmapMode = +vkf::SamplerMipmapMode::eLinear;
	//sampler.magFilter = vk::Filter::eNearest;
	//sampler.minFilter = vk::Filter::eNearest;
	//sampler.mipmapMode = vk::SamplerMipmapMode::eNearest;
	sampler.addressModeU = +vkf::SamplerAddressMode::eMirroredRepeat;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;//vk::BorderColor::eFloatOpaqueWhite;

	VkSamplerCreateInfo samplerInfo = sampler;
	VkSampler mainSampler;// = engine->device.createSampler(sampler);
	vkCreateSampler(engine->device, &samplerInfo, nullptr, &mainSampler);
	//build images
	for (auto [n, atch] : graph_attachments)
	{
		GraphAttachment& v = graph_attachments[n];
		VkImageCreateInfo imageCreateInfo = vkinit::image_create_info(v.info.format, v.usageFlags,VkExtent3D{0,0,0});
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;//+vkf::ImageType::e2D;

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
		imageCreateInfo.samples = +vkf::SampleCountFlagBits::e1;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;//+vkf::ImageTiling::eOptimal;
		imageCreateInfo.format = v.info.format;
		imageCreateInfo.usage = v.usageFlags;

		VmaAllocationCreateInfo vmaallocInfo = {};
		vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		vmaallocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkImageCreateInfo imnfo1 = imageCreateInfo;

		VkImage image;

		vmaCreateImage(engine->allocator, &imnfo1, &vmaallocInfo, &image, &v.imageAlloc, nullptr);

		v.image = image;

		VkImageViewCreateInfo imageViewInfo = vkinit::image_view_create_info(v.info.format,image);
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;//vk::ImageViewType::e2D;
		imageViewInfo.format = v.info.format;
		imageViewInfo.subresourceRange = VkImageSubresourceRange{};
		imageViewInfo.subresourceRange.aspectMask = is_used_as_depth(&v) ? +vkf::ImageAspectFlagBits::eDepth : +vkf::ImageAspectFlagBits::eColor;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 1;
		imageViewInfo.image = image;

		v.descriptor.imageView = engine->device.createImageView(imageViewInfo);
		v.descriptor.imageLayout = is_used_as_depth(&v)  ? +vkf::ImageLayout::eDepthStencilReadOnlyOptimal : +vkf::ImageLayout::eShaderReadOnlyOptimal;
		v.descriptor.sampler = mainSampler;
	}

	//make render passes
	for (int i = 0; i < passes.size(); i++) {
		RenderPass* pass = passes[i];
		if (pass->type == PassType::Graphics) {
			build_render_pass(pass, engine);
		}
		else if (pass->type == PassType::Compute) {
			build_compute_barriers(pass, engine);
		}
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

RenderPass* FrameGraph::add_pass(std::string pass_name, std::function<void(VkCommandBuffer, RenderPass*)> execution, PassType type, bool bPerformSubmit /*= false*/)
{
	pass_definitions[pass_name] = RenderPass();

	RenderPass* ptr = &pass_definitions[pass_name];
	ptr->name = pass_name;
	ptr->owner = this;
	ptr->draw_callback = execution;
	ptr->type = type;
	ptr->perform_submit = bPerformSubmit;
	passes.push_back(ptr);

	return ptr;
}

RenderPass* FrameGraph::get_pass(std::string pass_name)
{
	auto it = pass_definitions.find(pass_name);
	if (it != pass_definitions.end()) {
		return &it->second;
	}
	else {
		return nullptr;
	}	
}

VkDescriptorImageInfo FrameGraph::get_image_descriptor(std::string name)
{
	return get_attachment(name)->descriptor;
}

FrameGraph::GraphAttachment* FrameGraph::get_attachment(std::string name)
{
	return &graph_attachments[name];
}



VkCommandBuffer FrameGraph::create_graphics_buffer(int threadID)
{
	int frameid = owner->globalFrameNumber;

	auto pool = commandPools.get(frameid)[threadID];
	auto &usable_pool = usableCommands.get(frameid)[threadID];

	VkCommandBuffer buff;
	if (usable_pool.size() > 0) {
		buff = usable_pool.back();
		usable_pool.pop_back();
	}
	else {
	
		VkCommandBufferAllocateInfo allocInfo = vkinit::command_buffer_allocate_info(pool,1);
		
		buff = owner->device.allocateCommandBuffers(allocInfo)[0];
	}

	if (buff == VK_NULL_HANDLE)
	{
		std::cout << "YHo WTF";
	}

	pendingCommands.get(frameid)[threadID].push_back(buff);
	return  buff;
}

void FrameGraph::execute(VkCommandBuffer _cmd)
{
	int frameid = owner->globalFrameNumber;

	auto pool = commandPools.get(frameid)[0];
	{

		ZoneScopedNC("reset command pool", tracy::Color::Red);
		vkResetCommandPool(owner->device, pool, 0);
		//owner->device.resetCommandPool(pool, VkCommandPoolResetFlagBits{0});

		auto& pending = pendingCommands.get(frameid)[0];
		auto& usable = usableCommands.get(frameid)[0];

		for (auto cmd : pending) {
			usable.push_back(cmd);
		}
		pending.clear();
	}

	VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	

	VkCommandBuffer cmd = create_graphics_buffer(0);


	vkBeginCommandBuffer(cmd, &beginInfo);

	int current_submission_id = 0;
	for (auto pass : passes) {

		if (pass->draw_callback)
		{
			if (pass->type == PassType::Graphics) {

			
				VkExtent2D renderArea;
				renderArea.width = pass->render_width;
				renderArea.height = pass->render_height;

				VkRenderPassBeginInfo renderPassInfo =  vkinit::renderpass_begin_info(pass->built_pass, renderArea,pass->framebuffer);					

				renderPassInfo.clearValueCount = pass->clearValues.size();
				renderPassInfo.pClearValues = pass->clearValues.data();

				//cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
				vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
				pass->draw_callback(cmd, pass);

				vkCmdEndRenderPass(cmd);
				//cmd.endRenderPass();

				if (pass->perform_submit) {
				
					vkEndCommandBuffer(cmd);
					//cmd.end();
				
					submit_commands(cmd, current_submission_id, current_submission_id + 1);
					current_submission_id++;
				
					cmd = create_graphics_buffer(0);

					vkBeginCommandBuffer(cmd, &beginInfo);
					//cmd.begin(beginInfo);
				}
			}
			else if (pass->type == PassType::Compute) {

				static std::vector<VkImageMemoryBarrier> image_barriers;

				image_barriers.clear();
				transform_images_to_write(pass, image_barriers, cmd);

				pass->draw_callback(cmd, pass);

				image_barriers.clear();
				transform_images_to_read(pass, image_barriers, cmd);

				if (true){//pass->perform_submit) {

					vkEndCommandBuffer(cmd);
					//cmd.end();

					submit_commands(cmd, current_submission_id, current_submission_id + 1);
					current_submission_id++;

					cmd = create_graphics_buffer(0);

					vkBeginCommandBuffer(cmd, &beginInfo);
					//cmd.begin(beginInfo);
				}
			}
			else {
				pass->draw_callback(cmd, pass);
			}			
		}
	}
	vkEndCommandBuffer(cmd);
	//cmd.end();
	submit_commands(cmd, current_submission_id, 98);
}

void FrameGraph::transform_images_to_read(RenderPass* pass, std::vector<VkImageMemoryBarrier>& image_barriers, VkCommandBuffer& cmd)
{

	vkCmdPipelineBarrier(cmd,
		+vkf::PipelineStageFlagBits::eAllGraphics,
		+vkf::PipelineStageFlagBits::eAllGraphics,
		VkDependencyFlags{},
		0, nullptr, 0, nullptr, pass->endBarriers.size(), pass->endBarriers.data());
}

void FrameGraph::transform_images_to_write(RenderPass* pass, std::vector<VkImageMemoryBarrier>& image_barriers, VkCommandBuffer& cmd)
{
	vkCmdPipelineBarrier(cmd,
		+vkf::PipelineStageFlagBits::eAllGraphics,
		+vkf::PipelineStageFlagBits::eAllGraphics,		
		
		VkDependencyFlags{}, 0, nullptr, 0, nullptr, pass->startBarriers.size(), pass->startBarriers.data());
}

void FrameGraph::submit_commands(VkCommandBuffer cmd, int wait_pass_index, int signal_pass_index)
{
	VkSemaphore signalSemaphores[] = { owner->frameTimelineSemaphore };
	{
		uint64_t waitValue3[3];

		waitValue3[0] = { 1 };			
		waitValue3[1] = /*wait_pass_index == 0 ? 0 : */ owner->current_frame_timeline_value(wait_pass_index);
		
		
		const uint64_t signalValues[] = {
			owner->current_frame_timeline_value(signal_pass_index),owner->current_frame_timeline_value(signal_pass_index)
		};

		VkTimelineSemaphoreSubmitInfoKHR timelineInfo3;
		timelineInfo3.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR;
		timelineInfo3.pNext = NULL;
		//timelineInfo3.waitSemaphoreValueCount = 1;
		//if (wait_pass_index != 0) {
			timelineInfo3.waitSemaphoreValueCount = 1;
		//}
		timelineInfo3.pWaitSemaphoreValues = waitValue3;
		timelineInfo3.signalSemaphoreValueCount = 1;
		timelineInfo3.pSignalSemaphoreValues = signalValues;
	
		VkSubmitInfo submitInfo = vkinit::submit_info(&cmd);

		VkSemaphore waitSemaphores[] = { /*owner->imageAvailableSemaphores[owner->currentFrameIndex],*/owner->frameTimelineSemaphore };
		//if (wait_pass_index != 0) {
		//	waitSemaphores[0] = owner->frameTimelineSemaphore;
		//}
		VkPipelineStageFlags waitStages[] = { +vkf::PipelineStageFlagBits::eColorAttachmentOutput,+vkf::PipelineStageFlagBits::eColorAttachmentOutput };

		submitInfo.waitSemaphoreCount = 1;
		//if (wait_pass_index != 0) {
		//	submitInfo.waitSemaphoreCount = 2;
		//}
		
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmd;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		submitInfo.pNext = &timelineInfo3;
		
		{
			ZoneScopedNC("Submit", tracy::Color::Red);
			vkQueueSubmit(owner->graphicsQueue, 1, &submitInfo, VkFence{});
			//fix owner->graphicsQueue.submit(1, &submitInfo, vk::Fence{},/*owner->inFlightFences[owner->currentFrameIndex],*/ owner->extensionDispatcher); //

		}
	}
}

void FrameGraph::build_command_pools()
{
	//--- QUEUE FAMILY
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = owner->physicalDevice.getQueueFamilyProperties();
	int graphicsFamilyIndex = 0;

	int i = 0;
	for (const auto& queueFamily : queueFamilyProperties) {
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
			graphicsFamilyIndex = i;
			break;
		}
		i++;
	}	

	VkCommandPoolCreateInfo poolInfo = vkinit::command_pool_create_info(graphicsFamilyIndex,VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
	
	for (int i = 0; i < commandPools.num; i++) {
		for(auto& pool : commandPools.items[i]){
			pool = owner->device.createCommandPool(poolInfo);
		}
	}	
}

int FrameGraph::get_reads(const GraphAttachment* attachment)
{
	int reads = 0;
	for (auto use : attachment->uses.usages) {
		if (use == RenderGraphImageAccess::Read) {
			reads++;
		}
	}
	return reads;
}
int FrameGraph::get_writes(const GraphAttachment* attachment) {
	int writes = 0;
	for (auto use : attachment->uses.usages) {
		if (use != RenderGraphImageAccess::Read) {
			writes++;
		}		
	}
	return writes;
}
bool FrameGraph::is_used_as_depth(const GraphAttachment* attachment) {

	for (auto use : attachment->uses.usages) {
		if (use == RenderGraphImageAccess::RenderTargetDepth) {
			return true;
		}
	}
	return false;
}
void RenderAttachmentInfo::set_clear_color(std::array<float, 4> col)
{
	for (int i = 0; i < 4; i++) {
		clearValue.color.float32[i] = col[i];
	}
	
	bClear = true;
}

void RenderAttachmentInfo::set_clear_depth(float depth, uint32_t stencil)
{
	clearValue.depthStencil = { depth, stencil };
	bClear = true;
}
