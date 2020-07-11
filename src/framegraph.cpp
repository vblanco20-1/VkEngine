#include "framegraph.h"
#include <iostream>
#include <pcheader.h>
#include "vulkan_render.h"
#include <vk_flags.h>
#include <vk_initializers.h>
#include "command_encoder.h"



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



VkAttachmentDescription make_color_attachment(const RenderAttachmentInfo* info, bool bFirstWrite, bool bReadAfter) {

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
		attachment.initialLayout = +vkf::ImageLayout::eUndefined;
	}
	else {
		attachment.initialLayout = +vkf::ImageLayout::eShaderReadOnlyOptimal;
	}

	attachment.finalLayout = bReadAfter ? +vkf::ImageLayout::eShaderReadOnlyOptimal : +vkf::ImageLayout::eColorAttachmentOptimal;

	return attachment;
}


VkAttachmentDescription make_depth_attachment(const RenderAttachmentInfo* info, bool bFirstWrite, bool bReadAfter, bool bWrittenNext) {

	VkAttachmentDescription attachment = {};
	
	attachment.format = info->format;
	attachment.samples = +vkf::SampleCountFlagBits::e1;

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


std::unique_ptr<RenderPassCreateInfo> bundle_renderpass_info(VkRenderPassCreateInfo* info)
{
	std::unique_ptr<RenderPassCreateInfo> newinfo = std::make_unique<RenderPassCreateInfo>();

	newinfo->topInfo = *info;
	newinfo->subpass = info->pSubpasses[0];

	//copy attachment references from subpass 0
	int i = 0;
	for (i; i < info->pSubpasses[0].colorAttachmentCount;i++) {
		newinfo->attachment_refs[i] = info->pSubpasses[0].pColorAttachments[i];
	}
	newinfo->depthAttachments = 0;
	if (info->pSubpasses[0].pDepthStencilAttachment)
	{
		newinfo->attachment_refs[i] = info->pSubpasses[0].pDepthStencilAttachment[0];
		newinfo->depthAttachments = 1;
	}
	
	
	//copy attachments
	i = 0;
	for (i; i < info->attachmentCount; i++) {
		newinfo->attachments[i] = info->pAttachments[i];
	}
	newinfo->numAttachments = info->pSubpasses[0].colorAttachmentCount;

	newinfo->pass_dependencies[0] = info->pDependencies[0];
	newinfo->pass_dependencies[1] = info->pDependencies[1];
	//int i = 0;
	//while(i < info->)
	//
	//
	//newinfo->subpasses[0] = info->pSubpasses[0];
	return newinfo;
}

VkRenderPass FrameGraphCache::requestRenderPass(VulkanEngine* eng, VkRenderPassCreateInfo* info)
{
	auto newinfo = bundle_renderpass_info(info);

	for (auto &nf : cachedRenderPasses)
	{
		if (*nf.createinfo == *newinfo)
		{
			return nf.pass;
		}
	}
	CachedRenderPass newCached;
	newCached.pass = eng->device.createRenderPass(newinfo->makeInfo());
	newCached.createinfo = std::move(newinfo);
	
	cachedRenderPasses.push_back(std::move(newCached));
	

	return cachedRenderPasses.back().pass;
}

VkRenderPassCreateInfo RenderPassCreateInfo::makeInfo()
{
	subpass.pColorAttachments = &attachment_refs[0];

	subpass.pDepthStencilAttachment = (depthAttachments == 0) ? nullptr :  &attachment_refs[this->numAttachments];

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = numAttachments+depthAttachments;
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
		
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = pass_dependencies.data();

	return renderPassInfo;
}


void FrameGraph::create_render_pass(int color_attachments, VkAttachmentReference* references, int depth_attachments, VkAttachmentDescription* attachments, RenderPass* pass, VulkanEngine* eng)
{
	VkSubpassDescription subpass = {};

	subpass.pipelineBindPoint = (VkPipelineBindPoint)vkf::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = color_attachments;

	subpass.pColorAttachments = (color_attachments) ? references : nullptr;

	subpass.pDepthStencilAttachment = (depth_attachments) ? (references + color_attachments) : nullptr;;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = color_attachments + depth_attachments;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	std::array < VkSubpassDependency, 2> pass_dependencies = build_basic_subpass_dependencies();
	renderPassInfo.dependencyCount = static_cast<uint32_t>(pass_dependencies.size());
	renderPassInfo.pDependencies = pass_dependencies.data();

	//auto info = bundle_renderpass_info(&renderPassInfo);

	pass->built_pass = cache.requestRenderPass(eng, &renderPassInfo);

	//pass->built_pass = eng->device.createRenderPass(info->makeInfo());
}

void FrameGraph::build_render_pass(RenderPass* pass, VulkanEngine* eng)
{
	if (pass->built_pass != VK_NULL_HANDLE) return;

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

	//std::cout << "Pass Building : " << pass->name << "-----------------" << std::endl;

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

	create_render_pass(color_attachments, references.data(), depth_attachments, attachments.data(), pass, eng);
	
	//framebuffers
	std::vector<VkImageView> FBAttachments;

	uint32_t width = 0;
	uint32_t height = 0;

	std::vector<VkFramebufferAttachmentImageInfo> FBImageinfos;

	for (auto attachment : pass->physical_attachments) {

		FBAttachments.push_back(graph_attachments[attachment.name].descriptor.imageView);

		width = graph_attachments[attachment.name].real_width;
		height = graph_attachments[attachment.name].real_height;

		VkFramebufferAttachmentImageInfo ainfo{};
		ainfo.pNext = nullptr;
		ainfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO_KHR;
		ainfo.height = height;
		ainfo.width = width;
		ainfo.layerCount = 1;
		ainfo.usage = graph_attachments[attachment.name].usageFlags;
		ainfo.pViewFormats = &graph_attachments[attachment.name].info.format;
		ainfo.viewFormatCount = 1;
		
		FBImageinfos.push_back(ainfo);


	}

	pass->render_height = height;
	pass->render_width = width;




	VkFramebufferAttachmentsCreateInfoKHR fbufAttachmentInfo = {};
	fbufAttachmentInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO;
	fbufAttachmentInfo.pNext = nullptr;

	fbufAttachmentInfo.pAttachmentImageInfos = FBImageinfos.data();
	fbufAttachmentInfo.attachmentImageInfoCount = FBImageinfos.size();


	VkFramebufferCreateInfo fbufCreateInfo = vkinit::framebuffer_create_info(pass->built_pass, {width,height});	

	fbufCreateInfo.pNext = &fbufAttachmentInfo;
	fbufCreateInfo.attachmentCount =FBImageinfos.size();//FBAttachments.size();
	fbufCreateInfo.pAttachments = nullptr;//FBAttachments.data();
	fbufCreateInfo.width = width;
	fbufCreateInfo.height = height;
	fbufCreateInfo.layers = 1;
	fbufCreateInfo.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;

	//std::cout << "building framebuffer for" << pass->name << std::endl;
	pass->framebuffer = eng->device.createFramebuffer(fbufCreateInfo);
}

void FrameGraph::build_compute_barriers(RenderPass* pass, VulkanEngine* eng)
{
	pass->startBarriers.clear();
	pass->endBarriers.clear();
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

			startbarrier.image = graphAth->get_image(this);
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

			endbarrier.image = graphAth->get_image(this);
			endbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			endbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			endbarrier.subresourceRange = range;

			pass->endBarriers.push_back(endbarrier);
		}
	}
}

void FrameGraph::return_image(uint64_t image_ID)
{
	cache.reusableImages.push_back(image_ID);
}

uint64_t FrameGraph::request_image(VkImageCreateInfo* createInfo) {

	//search for a reusable image with the same image-create-info
	for (int i = 0; i <cache.reusableImages.size(); i++) {
		auto id = cache.reusableImages[i];
		auto cached_image = cache.cachedImages[id];

		bool bIsSame = memcmp(&cached_image.createInfo, createInfo, sizeof(VkImageCreateInfo)) == 0;
		if(bIsSame)
		{
			//remove the index from the reusable list
			cache.reusableImages[i] = cache.reusableImages.back();
			cache.reusableImages.pop_back();

			//reuse
			return id;
		}
	}

	//allocate a new image

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vmaallocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//V//kImageCreateInfo imnfo1 = imageCreateInfo;

	VkImage image;
	VmaAllocation imageAlloc;
	vmaCreateImage(owner->allocator, createInfo, &vmaallocInfo, &image, &imageAlloc, nullptr);

	CachedImage newCached;
	newCached.createInfo = *createInfo;
	newCached.image.image = image;
	newCached.image.allocation = imageAlloc;

	uint64_t new_id = cache.last_index++;
	cache.cachedImages[new_id] = newCached;

	return new_id;
}

VkImageView FrameGraph::request_image_view(uint64_t image_ID, VkImageViewCreateInfo* createInfo)
{
	//set the target image of the imageview to be the cached image
	CachedImage& image = cache.cachedImages[image_ID];

	createInfo->image = image.image.image;

	//search to check if the image already had that image view created
	
	for (auto v : image.imageViews) {

		bool bIsSame = memcmp(&v.createInfo, createInfo, sizeof(VkImageViewCreateInfo)) == 0;
		if (bIsSame) {
			return v.imageView;
		}
	}

	VkImageView view;

	vkCreateImageView(owner->device, createInfo, nullptr, &view);
	
	CachedImageView cachedView;
	cachedView.imageView = view;
	cachedView.createInfo = *createInfo;
	image.imageViews.push_back(cachedView);

	return view;
}

bool FrameGraph::build( VulkanEngine* engine)
{
	ZoneScopedNC("Rendergraph build", tracy::Color::Blue);

	owner = engine;


	return_images();


	{
		ZoneScopedNC("Rendergraph grab attachments", tracy::Color::Blue1);
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
	}
	
	{
		ZoneScopedNC("Rendergraph gather usages", tracy::Color::Blue1);
		//grab all render targets
		for (auto& [name, attachment] : graph_attachments) {

			//std::cout << "----------------------------" << std::endl;
			for (int i = 0; i < passes.size(); i++) {
				RenderPass* pass = passes[i];
				RenderGraphImageAccess access;
				if (pass->find_image_access(name, access))
				{
					attachment.uses.usages.push_back(access);
					attachment.uses.users.push_back(i);

					//std::cout << "attachment: " << name << " used by " << pass->name << " as ";
					//switch (access) {
					//case RenderGraphImageAccess::Write:
					//	std::cout << "write";
					//	break;
					//case RenderGraphImageAccess::Read:
					//	std::cout << "read";
					//	break;
					//case RenderGraphImageAccess::RenderTargetColor:
					//	std::cout << "render target";
					//	break;
					//case RenderGraphImageAccess::RenderTargetDepth:
					//	std::cout << "depth target";
					//	break;
					//}
					//std::cout << std::endl;
				}
			}
			//std::cout << std::endl;
		}
		//for (auto [name, attachment] : graph_attachments) {
		//	std::cout << (is_used_as_depth(&attachment) ? " Depth Attachment: " : " Color Attachment: ") << name << " -- Reads " << get_reads(&attachment) << " Writes " << get_writes(&attachment) << std::endl;
		//}
	}
	{
		ZoneScopedNC("Rendergraph build image flags", tracy::Color::Blue1);
		//calculate usage flags
		for (auto& [name, attachment] : graph_attachments) {
			VkImageUsageFlags flags{ 0 };


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

	}
	if (mainSampler == VK_NULL_HANDLE) {
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
		// = engine->device.createSampler(sampler);
		vkCreateSampler(engine->device, &samplerInfo, nullptr, &mainSampler);
	}
	{
		ZoneScopedNC("Rendergraph build images", tracy::Color::Blue1);
		//build images
		for (auto [n, atch] : graph_attachments)
		{
			GraphAttachment& v = graph_attachments[n];
			VkImageCreateInfo imageCreateInfo = vkinit::image_create_info(v.info.format, v.usageFlags, VkExtent3D{ 0,0,0 });
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
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.format = v.info.format;
			imageCreateInfo.usage = v.usageFlags;

			v.image_id = request_image(&imageCreateInfo);

			VkImageViewCreateInfo imageViewInfo = vkinit::image_view_create_info(v.info.format, VK_NULL_HANDLE);
			imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewInfo.format = v.info.format;
			imageViewInfo.subresourceRange = VkImageSubresourceRange{};
			imageViewInfo.subresourceRange.aspectMask = is_used_as_depth(&v) ? +vkf::ImageAspectFlagBits::eDepth : +vkf::ImageAspectFlagBits::eColor;
			imageViewInfo.subresourceRange.baseMipLevel = 0;
			imageViewInfo.subresourceRange.levelCount = 1;
			imageViewInfo.subresourceRange.baseArrayLayer = 0;
			imageViewInfo.subresourceRange.layerCount = 1;

			v.descriptor.imageView = request_image_view(v.image_id, &imageViewInfo);
			v.descriptor.imageLayout = is_used_as_depth(&v) ? +vkf::ImageLayout::eDepthStencilReadOnlyOptimal : +vkf::ImageLayout::eShaderReadOnlyOptimal;
			v.descriptor.sampler = mainSampler;
		}
	}
	{
		ZoneScopedNC("Rendergraph build passes", tracy::Color::Blue2);
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
	}
	

	//std::cout << "framegraph built";

	//attachmentNames.clear();
	//for (auto [n, atch] : graph_attachments)
	//{
	//	//leaks
	//	std::string* alloc = new std::string(n);
	//	attachmentNames.push_back(alloc->c_str());
	//}

	return true;
}

void FrameGraph::return_images()
{
	{
		ZoneScopedNC("Rendergraph return images", tracy::Color::Blue1);
		//clear all render targets and return to caches
		for (auto& [name, attachment] : graph_attachments) {
			if (attachment.image_id != 0) {
				return_image(attachment.image_id);
			}
		}
	}

	graph_attachments.clear();
}

void FrameGraph::reset(VulkanEngine* engine)
{
	return_images();

	pass_definitions.clear();
	passes.clear();
	
	attachmentNames.clear();
}

RenderPass* FrameGraph::add_pass(std::string pass_name, std::function<void(RenderPassCommands*)> execution, PassType type, bool bPerformSubmit /*= false*/)
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
	CommandEncoder encoder;

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

	tracy::VkCtx* profilercontext = (tracy::VkCtx*)owner->get_profiler_context(cmd);
	//TracyVkZone(profilercontext, cmd, "Rendergraph");

	TracyVkNamedManualZone(profilercontext, frameZone, cmd, "Framegraph", true);
	frameZone.Start(cmd);
	int current_submission_id = 0; 
	for (auto pass : passes) {

		if (pass->draw_callback)
		{
			if (pass->type == PassType::Graphics) {

			
				VkExtent2D renderArea;
				renderArea.width = pass->render_width;
				renderArea.height = pass->render_height;

				VkRenderPassAttachmentBeginInfo attachmentInfo = {};
				attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO;
				attachmentInfo.pNext = nullptr;

				//framebuffers
				std::vector<VkImageView> FBAttachments;

				uint32_t width = 0;
				uint32_t height = 0;

				for (auto attachment : pass->physical_attachments) {

					FBAttachments.push_back(graph_attachments[attachment.name].descriptor.imageView);
				}

				attachmentInfo.attachmentCount = FBAttachments.size();
				attachmentInfo.pAttachments = FBAttachments.data();
				
				

				VkRenderPassBeginInfo renderPassInfo =  vkinit::renderpass_begin_info(pass->built_pass, renderArea,pass->framebuffer);					

				renderPassInfo.pNext = &attachmentInfo;
				renderPassInfo.clearValueCount = pass->clearValues.size();
				renderPassInfo.pClearValues = pass->clearValues.data();

				//cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
				vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				
				RenderPassCommands commands;
				commands.commandBuffer = cmd;
				commands.renderPass = pass;
				commands.commandEncoder = &encoder;
				commands.profilerContext = profilercontext;
				pass->draw_callback(&commands);

				owner->DecodeCommands(cmd, &encoder);

				vkCmdEndRenderPass(cmd);
				//cmd.endRenderPass();

				if (pass->perform_submit) {
					frameZone.End();
					TracyVkCollect(profilercontext, cmd);
					vkEndCommandBuffer(cmd);
					//cmd.end();
				
					
					submit_commands(cmd, current_submission_id, current_submission_id + 1);
					current_submission_id++;
				
					cmd = create_graphics_buffer(0);
					
					vkBeginCommandBuffer(cmd, &beginInfo);
					frameZone.Start(cmd);
					//cmd.begin(beginInfo);
				}
			}
			else if (pass->type == PassType::Compute) {

				static std::vector<VkImageMemoryBarrier> image_barriers;

				image_barriers.clear();
				transform_images_to_write(pass, image_barriers, cmd);

				
				RenderPassCommands commands;
				commands.commandBuffer = cmd;
				commands.renderPass = pass;
				commands.commandEncoder = &encoder;
				commands.profilerContext = profilercontext;
				pass->draw_callback(&commands);
				owner->DecodeCommands(cmd, &encoder);

				image_barriers.clear();
				transform_images_to_read(pass, image_barriers, cmd);

				if (pass->perform_submit) {
					TracyVkCollect(profilercontext, cmd);
					frameZone.End();
					vkEndCommandBuffer(cmd);
					//cmd.end();
					
					submit_commands(cmd, current_submission_id, current_submission_id + 1);
					current_submission_id++;

					cmd = create_graphics_buffer(0);
					vkBeginCommandBuffer(cmd, &beginInfo);

					frameZone.Start(cmd);
					//cmd.begin(beginInfo);
				}
			}
			else {
				RenderPassCommands commands;
				commands.commandBuffer = cmd;
				commands.renderPass = pass;
				commands.commandEncoder = &encoder;
				commands.profilerContext = profilercontext;
				pass->draw_callback(&commands);
				owner->DecodeCommands(cmd, &encoder);
			}			
		}
	}
	frameZone.End();
	TracyVkCollect(profilercontext, cmd);
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

VkImage FrameGraph::GraphAttachment::get_image(FrameGraph* ownerGraph)
{
	return ownerGraph->cache.cachedImages[image_id].image.image;
}




void build_rendergraph(VulkanEngine* engine, Rendergraph* graph) {
}


