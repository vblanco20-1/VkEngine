#include "vulkan_render.h"
#include "sdl_render.h"
#include "rawbuffer.h"

#include "termcolor.hpp"
#include <gli/gli.hpp>
#include <vk_format.h>
//#include <tiny_obj_loader.h>

#include "tiny_gltf.h"
#include "stb_image.h"

EntityID VulkanEngine::load_texture(const char* image_path, std::string textureName)
{
	TextureResource texture;
	int texWidth, texHeight, texChannels;

	stbi_uc* pixels = stbi_load(image_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	void* pixel_ptr = pixels;
	vk::DeviceSize imageSize = texWidth * texHeight * 4;

	vk::Format image_format = vk::Format::eR8G8B8A8Unorm;
	gli::texture textu;
	if (!pixels) {
		textu = gli::load(image_path);
		gli::gl GL(gli::gl::PROFILE_GL33);
		gli::gl::format const Format = GL.translate(textu.format(), textu.swizzles());

		VkFormat format = vkGetFormatFromOpenGLInternalFormat(Format.Internal);

		texWidth = textu.extent().x;
		texHeight = textu.extent().y;

		pixel_ptr = textu.data();
		imageSize = textu.size();


		image_format = vk::Format(format);
		std::cout << "failed to load texture on path: " << image_path << to_string(image_format) << std::endl;
		//throw std::runtime_error("failed to load texture image!");
	}

	AllocatedBuffer stagingBuffer;
	createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer);
	
	
	void* data;
	vmaMapMemory(allocator, stagingBuffer.allocation, &data);//device.mapMemory(stagingBufferMemory, 0, bufferSize);
	
	memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));
	
	vmaUnmapMemory(allocator, stagingBuffer.allocation);
	
	stbi_image_free(pixels);	
	
	createImage(texWidth, texHeight, image_format,
		vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal, texture.image);
	
	transitionImageLayout(texture.image.image, image_format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
	
	copyBufferToImage(stagingBuffer.buffer, texture.image.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
	
	transitionImageLayout(texture.image.image, image_format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
	
	
	vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);
	
	texture.imageView = createImageView(texture.image.image, image_format, vk::ImageAspectFlagBits::eColor);
	
	vk::SamplerCreateInfo samplerInfo;
	samplerInfo.magFilter = vk::Filter::eLinear;
	samplerInfo.minFilter = vk::Filter::eLinear;
	samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
	samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
	samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
	
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;
	
	samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = vk::CompareOp::eAlways;
	
	samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;
	
	texture.textureSampler = device.createSampler(samplerInfo);
	
	TextureResourceMetadata metadata;

	metadata.image_format = image_format;
	metadata.texture_size.x = texWidth;
	metadata.texture_size.y = texHeight;

	auto res = createResource(textureName.c_str(), texture);

	render_registry.assign<TextureResourceMetadata>(res,metadata);

	return res;
}


void VulkanEngine::load_textures_bulk(TextureLoadRequest* requests, size_t count)
{
	ZoneScopedN("Loading Bulk Textures");

	struct ImageData {
		bool bLoaded = false;
		int texWidth, texHeight, texChannels;
		vk::Format image_format = vk::Format::eR8G8B8A8Unorm;
		void* pixel_ptr;
		stbi_uc* pixels;
		vk::DeviceSize imageSize;
		TextureResourceMetadata metadata;
		TextureResource texture;
		AllocatedBuffer stagingBuffer;
	};
	std::vector<ImageData> AllData;
	AllData.resize(count);
//TODO: Shits broken
	constexpr int max_tex_upload = 300;
	int batches = (count / max_tex_upload) + 1;

	for (int batch = 0; batch < batches; batch++) {
		//std::vector<AllocatedBuffer> stagingBuffers;
		for (int i = 0; i < max_tex_upload; i++) {

			int index = batch * batches + i;
			if (index >= count) break;
			
			const char* image_path = requests[index].image_path.c_str();
		
		//int texWidth, texHeight, texChannels;
		//std::cout << "Trying to load texture " << image_path << std::endl;
		AllData[index].pixels = stbi_load(image_path, &AllData[index].texWidth, &AllData[index].texHeight, &AllData[index].texChannels, STBI_rgb_alpha);
		AllData[index].pixel_ptr = AllData[index].pixels;
		AllData[index].imageSize = AllData[index].texWidth * AllData[index].texHeight * 4;

		

		AllData[index].image_format = vk::Format::eR8G8B8A8Unorm;
		AllData[index].metadata.image_format = vk::Format::eR8G8B8A8Unorm;
		AllData[index].metadata.texture_size.x = AllData[index].texWidth;
		AllData[index].metadata.texture_size.y = AllData[index].texHeight;

		gli::texture textu;
		if (!AllData[index].pixels) {
			textu = gli::load(image_path);
			if (textu.empty()) {

				AllData[index].bLoaded = false;
				
				std::cout << "failed to load texture on path: " << image_path << " : " << to_string(AllData[index].image_format) << std::endl;
			}
			else {

				//std::cout << "Loading texture " << image_path << " : " <<to_string(AllData[i].image_format) << std::endl;

				gli::gl GL(gli::gl::PROFILE_GL33);
				gli::gl::format const Format = GL.translate(textu.format(), textu.swizzles());

				VkFormat format = vkGetFormatFromOpenGLInternalFormat(Format.Internal);

				AllData[index].texWidth = textu.extent().x;
				AllData[index].texHeight = textu.extent().y;

				AllData[index].pixel_ptr = textu.data();
				AllData[index].imageSize = textu.size();

				

				AllData[index].image_format = vk::Format(format);

				AllData[index].metadata.image_format = AllData[index].image_format;
				AllData[index].metadata.texture_size.x = AllData[index].texWidth;
				AllData[index].metadata.texture_size.y = AllData[index].texHeight;
				AllData[index].bLoaded = true;
			}			
		}
		else {
			//std::cout << "Loading texture 2" << image_path << " : " << to_string(AllData[i].image_format) << std::endl;
			AllData[index].bLoaded = true;
		}
		
	
		if (!AllData[index].bLoaded) {
			requests[index].bLoaded = false;
			continue;
		}


			createBuffer(AllData[index].imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, AllData[index].stagingBuffer);

			void* data;
			vmaMapMemory(allocator, AllData[index].stagingBuffer.allocation, &data);

			memcpy(data, AllData[index].pixel_ptr, static_cast<size_t>(AllData[index].imageSize));

			vmaUnmapMemory(allocator, AllData[index].stagingBuffer.allocation);
			if (AllData[index].pixels) {
				stbi_image_free(AllData[index].pixels);
			}
			
						


			createImage(AllData[index].texWidth, AllData[index].texHeight, AllData[index].image_format,
				vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
				vk::MemoryPropertyFlagBits::eDeviceLocal, AllData[index].texture.image);
			

		}

		auto cmd = beginSingleTimeCommands();

		{
			tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
			TracyVkZone(profilercontext, VkCommandBuffer(cmd), "Upload Image");

			ZoneScopedN("Copying images to GPU");
			for (int i = 0; i < max_tex_upload; i++) {

				int index = batch * batches + i;
				if (index >= count) break;
				cmd_transitionImageLayout(cmd, AllData[index].texture.image.image, AllData[index].image_format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

				cmd_copyBufferToImage(cmd, AllData[index].stagingBuffer.buffer, AllData[index].texture.image.image, static_cast<uint32_t>(AllData[index].texWidth), static_cast<uint32_t>(AllData[index].texHeight));

				cmd_transitionImageLayout(cmd, AllData[index].texture.image.image, AllData[index].image_format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
			}
		}
		endSingleTimeCommands(cmd);

		for (int i = 0; i < max_tex_upload; i++) {

			int index = batch * batches + i;
			if (index >= count) break;

			vmaDestroyBuffer(allocator, AllData[index].stagingBuffer.buffer, AllData[index].stagingBuffer.allocation);

			AllData[index].texture.imageView = createImageView(AllData[index].texture.image.image, AllData[index].image_format, vk::ImageAspectFlagBits::eColor);

			vk::SamplerCreateInfo samplerInfo;
			samplerInfo.magFilter = vk::Filter::eLinear;
			samplerInfo.minFilter = vk::Filter::eLinear;
			samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
			samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
			samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;

			samplerInfo.anisotropyEnable = VK_TRUE;
			samplerInfo.maxAnisotropy = 16;

			samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;

			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = vk::CompareOp::eAlways;

			samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
			samplerInfo.mipLodBias = 0.0f;
			samplerInfo.minLod = 0.0f;
			samplerInfo.maxLod = 0.0f;

			AllData[index].texture.textureSampler = device.createSampler(samplerInfo);

			requests[index].loadedTexture = createResource(requests[index].textureName.c_str(), AllData[index].texture);
			render_registry.assign<TextureResourceMetadata>(requests[index].loadedTexture, AllData[index].metadata);
			requests[index].bLoaded = true;
		}
	}
	
}


void VulkanEngine::create_texture_image_view()
{
	tset_textureImageView = createImageView(test_textureImage.image, vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor);//device.createImageView(viewInfo);

}

void VulkanEngine::createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
	vk::MemoryPropertyFlags properties, AllocatedImage& image)
{
	vk::ImageCreateInfo imageInfo;
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.extent.width = static_cast<uint32_t>(width);
	imageInfo.extent.height = static_cast<uint32_t>(height);
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;

	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;

	imageInfo.usage = usage;

	imageInfo.sharingMode = vk::SharingMode::eExclusive;

	imageInfo.samples = vk::SampleCountFlagBits::e1;



	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vmaallocInfo.requiredFlags = VkMemoryPropertyFlags(properties);
	VkImage img;

	VkImageCreateInfo imnfo = imageInfo;

	vmaCreateImage(allocator, &imnfo, &vmaallocInfo, &img, &image.allocation, nullptr);
	image.image = img;

}

vk::ImageView VulkanEngine::createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags)
{
	vk::ImageViewCreateInfo viewInfo;
	viewInfo.image = image;
	viewInfo.viewType = vk::ImageViewType::e2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	return device.createImageView(viewInfo);
}

void VulkanEngine::copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height)
{
	auto cmd = beginSingleTimeCommands();
	{
		tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
		TracyVkZone(profilercontext, VkCommandBuffer(cmd), "Upload Image");

		cmd_copyBufferToImage(cmd, buffer, image, width, height);
	}

	endSingleTimeCommands(cmd);
}

void VulkanEngine::cmd_copyBufferToImage(vk::CommandBuffer& cmd, vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height)
{
	vk::BufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		width,
		height,
		1
	};

	cmd.copyBufferToImage(
		buffer,
		image,
		vk::ImageLayout::eTransferDstOptimal,
		1,
		&region);

}

vk::Format VulkanEngine::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
{
	for (vk::Format format : candidates) {
		vk::FormatProperties props = physicalDevice.getFormatProperties(format);

		if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}
	throw std::runtime_error("failed to find supported format!");
}

vk::Format  VulkanEngine::findDepthFormat()
{
	return findSupportedFormat({ vk::Format::eD32Sfloat,vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint }, vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

bool hasStencilComponent(vk::Format format) {
	return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}
void VulkanEngine::transitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
	auto cmd = beginSingleTimeCommands();

	cmd_transitionImageLayout(cmd, image, format, oldLayout, newLayout);

	endSingleTimeCommands(cmd);
}

void VulkanEngine::cmd_transitionImageLayout(vk::CommandBuffer& commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
	vk::ImageMemoryBarrier barrier;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	//barrier.srcAccessMask = 0; // TODO
	//barrier.dstAccessMask = 0; // TODO

	vk::PipelineStageFlags sourceStage;
	vk::PipelineStageFlags destinationStage;

	if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
		barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

		if (hasStencilComponent(format)) {
			barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
		}
	}


	if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
		barrier.srcAccessMask = vk::AccessFlags{};
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

		sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
		destinationStage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		sourceStage = vk::PipelineStageFlagBits::eTransfer;
		destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
		barrier.srcAccessMask = vk::AccessFlags{};
		barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

		sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
		destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
	}
	else {
		throw std::invalid_argument("unsupported layout transition!");
	}

	commandBuffer.pipelineBarrier(sourceStage, destinationStage, vk::DependencyFlags{}, 0, nullptr, 0, nullptr, 1, &barrier);
}