#include "framegraph.h"
#include <iostream>
#include <pcheader.h>
#include "vulkan_render.h"


struct FrameGraph::FrameGraphPriv {
	std::unordered_map<std::string, std::vector<vk::AttachmentDescription>> pass_attachments;
};

vk::AttachmentDescription make_attachment_description(const AttachmentInfo& info, RenderGraphResourceAccess access, vk::ImageLayout initialLayout, vk::ImageLayout finalLayout) {

	vk::AttachmentDescription attachment;
	attachment.format = (vk::Format)info.format;
	attachment.samples = vk::SampleCountFlagBits::e1;

	//todo: add dontcare?
	attachment.loadOp = (access == RenderGraphResourceAccess::Write) ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad;

	//attachment.storeOp = store ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare;
	
	//if (display) {
	//	attachment.initialLayout = vk::ImageLayout::eUndefined;
	//	attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;
	//}
	//else if (store) {
	//	attachment.initialLayout = vk::ImageLayout::eUndefined;
	//	attachment.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	//}
	//else {
	//
	//}
	return attachment;
	
}

void RenderPass::add_depth_attachment(std::string name, const AttachmentInfo& info, RenderGraphResourceAccess access)
{
	PassAttachment addinfo;
	addinfo.name = name;
	addinfo.info = info;
	addinfo.bIsDepth = true;
	addinfo.bWrite = (access == RenderGraphResourceAccess::Write);

	depth_attachments.push_back(addinfo);
}

void RenderPass::add_color_attachment(std::string name,const AttachmentInfo& info, RenderGraphResourceAccess access)
{
	PassAttachment addinfo;
	addinfo.name = name;
	addinfo.info = info;
	addinfo.bIsDepth = false;
	addinfo.bWrite = access == RenderGraphResourceAccess::Write;

	color_attachments.push_back(addinfo);
}


FrameGraph::FrameGraph()
{
	priv = new FrameGraphPriv();
}

//void FrameGraph::register_resource(std::string name, const AttachmentInfo& attachment)
//{
//	//GraphAttachment attach;
//
//}
vk::AttachmentDescription make_color_attachment(AttachmentInfo* info, bool bFirstWrite ,bool bReadAfter, bool toDisplay) {

	vk::AttachmentDescription attachment;
	attachment.format = (vk::Format)info->format;
	attachment.samples = vk::SampleCountFlagBits::e1;

	//todo: add dontcare?
	attachment.loadOp = bFirstWrite ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad;

	attachment.storeOp = bReadAfter ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare;

	
	if (bFirstWrite) {
		attachment.initialLayout = vk::ImageLayout::eUndefined;
	}
	else {
		attachment.initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	}
	
	if (toDisplay) {
		attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;
	}
	else {
		attachment.finalLayout = bReadAfter ? vk::ImageLayout::eShaderReadOnlyOptimal : vk::ImageLayout::eUndefined;
	}

	return attachment;
}


vk::AttachmentDescription make_depth_attachment(AttachmentInfo* info, bool bFirstWrite, bool bReadAfter,bool bWrittenNext) {

	vk::AttachmentDescription attachment;
	attachment.format = (vk::Format)info->format;
	attachment.samples = vk::SampleCountFlagBits::e1;

	//todo: add dontcare?
	attachment.loadOp = bFirstWrite ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad;

	attachment.storeOp = (bReadAfter || bWrittenNext) ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare;


	if (bFirstWrite) {
		attachment.initialLayout = vk::ImageLayout::eUndefined;
	}
	else {
		attachment.initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	}

	if (bWrittenNext) {
		attachment.finalLayout =vk::ImageLayout::eDepthStencilAttachmentOptimal;
	}
	else {
		attachment.finalLayout = bReadAfter ? vk::ImageLayout::eDepthStencilReadOnlyOptimal : vk::ImageLayout::eUndefined;
	}

	return attachment;
}


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


void build_render_pass(RenderPass* pass, VkDevice device)
{
	std::cout << "Pass Building : " << pass->name << "-----------------" << std::endl;

	std::vector<vk::AttachmentReference> references;
	std::vector<vk::AttachmentDescription> attachments;
	int color_attachments = 0;
	int depth_attachments = 0;
	attachments.resize(pass->real_attachments.size());
	bool bScreenOutput = 0;
	for (auto [n, attachment] : pass->real_attachments) {

		vk::AttachmentReference athref;
		athref.attachment = attachment.index;
		athref.layout = attachment.bIsDepth ? vk::ImageLayout::eDepthStencilAttachmentOptimal : vk::ImageLayout::eColorAttachmentOptimal;

		if (attachment.bIsDepth) {
			depth_attachments++;
		}
		else {
			color_attachments++;
		}

		if (attachment.desc.finalLayout == vk::ImageLayout::ePresentSrcKHR)
		{
			bScreenOutput = true;
		}
		attachments[attachment.index] = attachment.desc;

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
	if (!bScreenOutput) {
		std::array < vk::SubpassDependency, 2> pass_dependencies = build_basic_subpass_dependencies();
		renderPassInfo.dependencyCount = static_cast<uint32_t>(pass_dependencies.size());
		renderPassInfo.pDependencies = pass_dependencies.data();

	}
	else {
		vk::SubpassDependency dependency;
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;

		dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dependency.srcAccessMask = vk::AccessFlags{};

		dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;
	}

	pass->built_pass = ((vk::Device)device).createRenderPass(renderPassInfo);
}

void FrameGraph::build(VkDevice device,VmaAllocator allocator)
{
	//grab all attachments
	for (auto pass : passes) {
		for (const auto & coloratch : pass->color_attachments) {

			if (attachments.find(coloratch.name) != attachments.end())
			{				
				if (coloratch.bWrite) {
					//attachments[coloratch.name].usageFlags |= vk::ImageUsageFlagBits::eColorAttachment;
					attachments[coloratch.name].writes++;
				}	
				else {
					//attachments[coloratch.name].usageFlags |= vk::ImageUsageFlagBits::eColorAttachment;
					attachments[coloratch.name].reads++;
				}
			}
			else {
				
					GraphAttachment attachment;
					attachment.info = coloratch.info;
					attachment.name = coloratch.name;
					//attachment.bIsDepth = false;					
					attachment.writes = coloratch.bWrite? 1: 0;					
					attachment.reads = coloratch.bWrite ? 0 : 1;
					attachment.usageFlags = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
					attachments[coloratch.name] = attachment;					
			}			
		}
		for (const auto& coloratch : pass->depth_attachments) {

			if (attachments.find(coloratch.name) != attachments.end())
			{
				if (coloratch.bWrite) {
					attachments[coloratch.name].usageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;;
					attachments[coloratch.name].writes++;
					attachments[coloratch.name].bIsDepth = true;
				}
				else {
					attachments[coloratch.name].reads++;
				}
			}
			else {

				GraphAttachment attachment;
				attachment.info = coloratch.info;
				attachment.name = coloratch.name;
				attachment.bIsDepth = true;
				attachment.writes = coloratch.bWrite ? 1 : 0;
				attachment.reads = coloratch.bWrite ? 0 : 1;
				attachment.usageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
				attachments[coloratch.name] = attachment;
			}
		}
	}

	for (auto [n, v] : attachments) {
		std::cout << (v.bIsDepth ?  " Depth Attachment: ":" Color Attachment: ")<< n << " -- Reads " << v.reads << " Writes " << v.writes<<std::endl;
	}

	//setup attachments per pass
	for (int i = 0; i < passes.size(); i++) {
		RenderPass* pass = passes[i];
		std::cout << "Pass: " << pass->name << "-----------------" << std::endl;

		bool bIsFirst = i == 0;
		bool bIsLast = (i == (passes.size() - 1));

		int attachmentIndex = 0;

		//check depth attachment
		RenderPass::PassAttachment* writtenDepth = nullptr;
		std::vector<RenderPass::PassAttachment*> readDepthImages;
		bool bReadsDepthImage = false;
		for (RenderPass::PassAttachment& attach : pass->depth_attachments) {
			if (attach.bWrite) {
				writtenDepth = &attach;
				readDepthImages.push_back(&attach);
			}
			else {
				readDepthImages.push_back(&attach);
			}
		}
		{

			for (auto ath : pass->color_attachments) {
				bool bAlsoWrittenBefore = false;
				bool bAccessedAsReadAfter = false;


				//check preconditions of those resources
				if (!bIsFirst) {
					//iterate passes before this one to find it
					for (int j = i - 1; j >= 0; j--) {

						RenderPass* otherpass = passes[j];

						for (RenderPass::PassAttachment& attach : otherpass->depth_attachments) {
							if (attach.name == ath.name) {
								if (attach.bWrite) {
									bAlsoWrittenBefore = true;
									break;
								}
							}
						}
						for (RenderPass::PassAttachment& attach : otherpass->color_attachments) {
							if (attach.name == ath.name) {
								if (attach.bWrite) {
									bAlsoWrittenBefore = true;
									break;
								}
							}
						}
					}
				}
				//postconditions
				if (!bIsLast) {
					//iterate passes after this one to find it
					for (int j = i + 1; j < passes.size(); j++) {

						RenderPass* otherpass = passes[j];

						for (RenderPass::PassAttachment& attach : otherpass->depth_attachments) {
							if (attach.name == ath.name) {
								if (!attach.bWrite) {

									bAccessedAsReadAfter = true;
									break;
								}
							}
						}
						for (RenderPass::PassAttachment& attach : otherpass->color_attachments) {
							if (attach.name == ath.name) {
								if (!attach.bWrite) {

									bAccessedAsReadAfter = true;
									break;
								}
							}
						}
					}
				}


				auto wasread = (bAlsoWrittenBefore ? " was written before " : " not written before ");
				auto asread = (bAccessedAsReadAfter ? " is accessed as read after " : " not accessed as read after");
				bool first_use = ((!bAlsoWrittenBefore || bIsFirst) && ath.bWrite);
				if (first_use) wasread = " First Create ";

				bool output = false;
				if (ath.name == "_output_") {
					wasread = "";
					asread = " outputs to the screen ";
					output = true;
				}
				if (ath.bWrite) {
					RenderPass::PhysicalAttachment physAttachment;
					physAttachment.bIsDepth = false;
					physAttachment.desc = make_color_attachment(&ath.info, first_use, bAccessedAsReadAfter, output);
					physAttachment.index = attachmentIndex;

					pass->real_attachments[ath.name] = physAttachment;

					attachmentIndex++;


					std::cout << "  W  Color attachment " << ath.name << wasread << asread << std::endl;
				}
				else {
					std::cout << "  R  Color image " << ath.name << wasread << asread << std::endl;
				}

			}
		}
		{			

			if (pass->name == "SSAO-pre") {
				std::cout << "ssao";
			}

			for (auto ath : readDepthImages) {
				bool bAlsoWrittenBefore = false;
				bool bAccessedAsReadAfter = false;
				//check preconditions of those resources
				if (!bIsFirst) {
					//iterate passes before this one to find it
					for (int j = i - 1; j >= 0; j--) {

						RenderPass* otherpass = passes[j];

						for (RenderPass::PassAttachment& attach : otherpass->depth_attachments) {
							if (attach.name == ath->name) {
								if (attach.bWrite) {
									bAlsoWrittenBefore = true;
									break;
								}
							}
						}
						for (RenderPass::PassAttachment& attach : otherpass->color_attachments) {
							if (attach.name == ath->name) {
								if (attach.bWrite) {
									bAlsoWrittenBefore = true;
									break;
								}
							}
						}
					}
				}
				bool bNextWrite = false;
				
				//postconditions
				if (!bIsLast) {
					//iterate passes before this one to find it
					for (int j = i + 1; j < passes.size(); j++) {

						RenderPass* otherpass = passes[j];

						for (RenderPass::PassAttachment& attach : otherpass->depth_attachments) {
							if (attach.name == ath->name) {
								if (!attach.bWrite) {
									bReadsDepthImage = true;
									bAccessedAsReadAfter = true;
									break;
								}
								else {
									bNextWrite = true;
									break;
								}
							}
						}
						for (RenderPass::PassAttachment& attach : otherpass->color_attachments) {
							if (attach.name == ath->name) {
								if (!attach.bWrite) {
									bReadsDepthImage = true;
									bAccessedAsReadAfter = true;
									break;
								}
							}
						}
					}
				}


				auto wasread = (bAlsoWrittenBefore ? " was written before " : " not written before ");
				auto asread = (bAccessedAsReadAfter ? " is accessed as read after " : " not accessed as read");

				auto first_use = ((!bAlsoWrittenBefore || bIsFirst) && ath->bWrite);
				if (first_use) wasread = " First Create ";

				bool output = false;
				if (ath->name == "_output_") {
					wasread = "";
					asread = " outputs to the screen ";
					output = true;
				}

				if (writtenDepth!= nullptr && writtenDepth->name == ath->name) {
					RenderPass::PhysicalAttachment physAttachment;
					physAttachment.bIsDepth = true;
					physAttachment.desc = make_depth_attachment(&ath->info, first_use, bAccessedAsReadAfter, bNextWrite);
					physAttachment.index = attachmentIndex;

					pass->real_attachments[ath->name] = physAttachment;

					std::cout << "   W Depth attachment " << ath->name << wasread << asread << std::endl;
				}
				else {
					std::cout << "   R Depth Image " << ath->name << wasread << asread << std::endl;
				}
				
			}
		}	
	}


	//one sampler to rule them all
	vk::SamplerCreateInfo sampler;
	sampler.magFilter = vk::Filter::eLinear;
	sampler.minFilter = vk::Filter::eLinear;
	sampler.mipmapMode = vk::SamplerMipmapMode::eLinear;
	sampler.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	sampler.borderColor = vk::BorderColor::eFloatOpaqueWhite;

	vk::Sampler mainSampler = ((vk::Device)device).createSampler(sampler);

	//build images
	for (auto [n, atch] : attachments) 
	{
		GraphAttachment &v = attachments[n];
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
		imageCreateInfo.usage = v.usageFlags;

		VmaAllocationCreateInfo vmaallocInfo = {};
		vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		vmaallocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkImageCreateInfo imnfo1 = imageCreateInfo;

		VkImage image;

		vmaCreateImage(allocator, &imnfo1, &vmaallocInfo, &image, &v.imageAlloc, nullptr);

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

		v.view = ((vk::Device)device).createImageView(imageViewInfo);
		v.sampler = mainSampler;
	}


	//make render passes
	for (int i = 0; i < passes.size(); i++) {
		RenderPass* pass = passes[i];
		build_render_pass(pass, device);

		//framebuffers
		std::vector<vk::ImageView> FBAttachments;
		FBAttachments.resize(pass->real_attachments.size());

		int width;
		int height;

		for (auto [n, attachment] : pass->real_attachments) {
			
			FBAttachments[attachment.index] = this->attachments[n].view;

			if (attachment.desc.loadOp == vk::AttachmentLoadOp::eClear) {
				width = this->attachments[n].real_width;
				height = this->attachments[n].real_height;
			}
		}

		pass->render_height = height;
		pass->render_width = width;
		//vk::ImageView attachments[] = { gbuffPass.posdepth.view,gbuffPass.normal.view,depthImageView };

		vk::FramebufferCreateInfo fbufCreateInfo;
		fbufCreateInfo.renderPass = pass->built_pass;
		fbufCreateInfo.attachmentCount = FBAttachments.size();
		fbufCreateInfo.pAttachments = FBAttachments.data();
		fbufCreateInfo.width = width;
		fbufCreateInfo.height = height;
		fbufCreateInfo.layers = 1;

		std::cout << "building framebuffer for" << pass->name << std::endl;
		pass->framebuffer = ((vk::Device)device).createFramebuffer(fbufCreateInfo);
	}

	std::cout << "framegraph built";
}

RenderPass* FrameGraph::add_pass(std::string pass_name)
{
	pass_definitions[pass_name] = RenderPass();
	
	RenderPass* ptr = &pass_definitions[pass_name];
	ptr->name = pass_name;
	ptr->owner = this;
	passes.push_back(ptr);

	return ptr;
}

void FrameGraph::execute(vk::CommandBuffer cmd)
{
	for (auto pass : passes) {

		if (pass->draw_callback && pass->clear_callback)
		{
			ClearPassSet clear = pass->clear_callback();
			vk::RenderPassBeginInfo renderPassInfo;
			renderPassInfo.renderPass = pass->built_pass;//shadowPass.renderPass;
			renderPassInfo.framebuffer = pass->framebuffer;
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent.width = pass->render_width;//shadowPass.width;
			renderPassInfo.renderArea.extent.height = pass->render_height;//shadowPass.height;

			renderPassInfo.clearValueCount = clear.num;
			renderPassInfo.pClearValues = clear.clearValues.data();

			cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

			pass->draw_callback(cmd,pass);

			cmd.endRenderPass();
		}	
	}
}

void create_engine_graph(VulkanEngine* engine)
{
	FrameGraph& graph = engine->graph;
	VkDevice device = (VkDevice)engine->device;
	VmaAllocator allocator = engine->allocator;
	VkExtent2D swapChainSize = engine->swapChainExtent;

	graph.swapchainSize = swapChainSize;

	//order is very important
	auto shadow_pass = graph.add_pass("ShadowPass");
	auto gbuffer_pass = graph.add_pass("GBuffer");
	auto ssao0_pass = graph.add_pass("SSAO-pre");
	auto ssao1_pass = graph.add_pass("SSAO-blur");
	auto forward_pass = graph.add_pass("MainPass");

	AttachmentInfo shadowbuffer;
	shadowbuffer.format = VK_FORMAT_D16_UNORM;
	shadowbuffer.persistent = true;
	shadowbuffer.size_class = SizeClass::Absolute;
	shadowbuffer.size_x = 2048;
	shadowbuffer.size_y = 2048;	

	AttachmentInfo gbuffer_position;
	gbuffer_position.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	gbuffer_position.persistent = false;
	gbuffer_position.size_class = SizeClass::SwapchainRelative;
	gbuffer_position.size_x = 1.f;
	gbuffer_position.size_y = 1.f;

	AttachmentInfo gbuffer_normal = gbuffer_position;
	gbuffer_normal.format = VK_FORMAT_R16G16B16A16_SFLOAT;

	AttachmentInfo gbuffer_depth = gbuffer_position;
	gbuffer_depth.format = (VkFormat)engine->findDepthFormat();


	AttachmentInfo ssao_pre = gbuffer_position;
	ssao_pre.format = VK_FORMAT_R8_UNORM;

	AttachmentInfo ssao_post = gbuffer_position;

	shadow_pass->add_depth_attachment("shadow_buffer_1", shadowbuffer,RenderGraphResourceAccess::Write);

	gbuffer_pass->add_color_attachment("gbuf_pos", gbuffer_position,RenderGraphResourceAccess::Write);
	gbuffer_pass->add_color_attachment("gbuf_normal", gbuffer_normal,RenderGraphResourceAccess::Write);
	gbuffer_pass->add_depth_attachment("depth_prepass", gbuffer_depth, RenderGraphResourceAccess::Write);

	//ssao0_pass->add_depth_attachment("depth_prepass", gbuffer_depth, RenderGraphResourceAccess::Read);
	ssao0_pass->add_color_attachment("gbuf_pos", gbuffer_position, RenderGraphResourceAccess::Read);
	ssao0_pass->add_color_attachment("gbuf_normal", gbuffer_normal, RenderGraphResourceAccess::Read);
	ssao0_pass->add_color_attachment("ssao_pre", ssao_pre, RenderGraphResourceAccess::Write);
	ssao0_pass->add_depth_attachment("depth_prepass", gbuffer_depth, RenderGraphResourceAccess::Write);


	ssao1_pass->add_color_attachment("ssao_pre", ssao_pre, RenderGraphResourceAccess::Read);
	ssao1_pass->add_color_attachment("ssao_post", ssao_post, RenderGraphResourceAccess::Write);

	forward_pass->add_color_attachment("gbuf_pos", gbuffer_position, RenderGraphResourceAccess::Read);
	forward_pass->add_color_attachment("gbuf_normal", gbuffer_normal, RenderGraphResourceAccess::Read);
	forward_pass->add_color_attachment("shadow_buffer_1", shadowbuffer, RenderGraphResourceAccess::Read);	
	forward_pass->add_depth_attachment("depth_prepass", gbuffer_depth, RenderGraphResourceAccess::Write);

	forward_pass->add_color_attachment("ssao_post", ssao_post, RenderGraphResourceAccess::Read);

	AttachmentInfo render_output = gbuffer_position;
		
	//_output_ is special case
	forward_pass->add_color_attachment("_output_", render_output, RenderGraphResourceAccess::Write);

	graph.build(device, allocator);
}
