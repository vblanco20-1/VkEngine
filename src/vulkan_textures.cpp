#include "vulkan_render.h"
#include "sdl_render.h"
#include "rawbuffer.h"

#include "termcolor.hpp"
#include <gli/gli.hpp>
#include <vk_format.h>
//#include <tiny_obj_loader.h>

#include "tiny_gltf.h"
#include "stb_image.h"

#include "vulkan_textures.h"
#include <scene_processor.h>

#include <future>
#include <thread>

using namespace sp;
std::pair<TextureResource,TextureResourceMetadata> VulkanEngine::load_texture_resource(const char* image_path, bool bIsCubemap /*= false*/)
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
		std::cout << "GLI loading texture: " << image_path << to_string(image_format) << std::endl;
		//throw std::runtime_error("failed to load texture image!");
	}

	AllocatedBuffer stagingBuffer;
	createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_UNKNOWN, stagingBuffer);


	void* data;
	vmaMapMemory(allocator, stagingBuffer.allocation, &data);//device.mapMemory(stagingBufferMemory, 0, bufferSize);

	memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));

 	vmaUnmapMemory(allocator, stagingBuffer.allocation);

	stbi_image_free(pixels);


	vk::ImageUsageFlags usageFlags = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;

	createImage(texWidth, texHeight, image_format,
		vk::ImageTiling::eOptimal, usageFlags,
		vk::MemoryPropertyFlagBits::eDeviceLocal, texture.image, bIsCubemap);

	//transitionImageLayout(texture.image.image, image_format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

	auto cmd1 = beginSingleTimeCommands();

	cmd_transitionImageLayout(cmd1, texture.image.image, image_format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, bIsCubemap);

	endSingleTimeCommands(cmd1);

	copyBufferToImage(stagingBuffer.buffer, texture.image.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), bIsCubemap);

	//transitionImageLayout(texture.image.image, image_format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
	auto cmd2 = beginSingleTimeCommands();

	cmd_transitionImageLayout(cmd2, texture.image.image, image_format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, bIsCubemap);

	endSingleTimeCommands(cmd2);

	vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);

	texture.imageView = createImageView(texture.image.image, image_format, vk::ImageAspectFlagBits::eColor, bIsCubemap);

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

	texture.bFullyLoaded = true;

	return { texture,metadata };
}

EntityID VulkanEngine::load_texture(const char* image_path, std::string textureName, bool bIsCubemap)
{
	ZoneScopedNC("Load Texture", tracy::Color::Red);
	auto texture = load_texture_resource(image_path, bIsCubemap);

	auto res = createResource(textureName.c_str(), texture.first);

	render_registry.assign<TextureResourceMetadata>(res, texture.second);

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

			{
				ZoneScopedNC("Texture request load batch", tracy::Color::Yellow);
				for (int i = 0; i < max_tex_upload; i++) {

					int index = batch * batches + i;
					if (index >= count) break;

					const char* image_path = requests[index].image_path.c_str();

					//int texWidth, texHeight, texChannels;
					//std::cout << "Trying to load texture " << image_path << std::endl;
					{
						ZoneScopedNC("Texture load ", tracy::Color::Red);
						AllData[index].pixels = stbi_load(image_path, &AllData[index].texWidth, &AllData[index].texHeight, &AllData[index].texChannels, STBI_rgb_alpha);
					}
					AllData[index].pixel_ptr = AllData[index].pixels;
					AllData[index].imageSize = AllData[index].texWidth * AllData[index].texHeight * 4;



					AllData[index].image_format = vk::Format::eR8G8B8A8Unorm;
					AllData[index].metadata.image_format = vk::Format::eR8G8B8A8Unorm;
					AllData[index].metadata.texture_size.x = AllData[index].texWidth;
					AllData[index].metadata.texture_size.y = AllData[index].texHeight;

					gli::texture textu;
					if (!AllData[index].pixels) {
						{
							ZoneScopedNC("Texture load ", tracy::Color::Red);
							textu = gli::load(image_path);
						}
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


					createBuffer(AllData[index].imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_UNKNOWN, AllData[index].stagingBuffer);

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
			}
			
			auto cmd = beginSingleTimeCommands();

			{
				tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
				TracyVkZone(profilercontext, VkCommandBuffer(cmd), "Upload Image");

				ZoneScopedN("Copying images to GPU");
				for (int i = 0; i < max_tex_upload; i++) {

					int index = batch * batches + i;
					if (index >= count) break;
					if (AllData[index].bLoaded) {
						cmd_transitionImageLayout(cmd, AllData[index].texture.image.image, AllData[index].image_format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

						cmd_copyBufferToImage(cmd, AllData[index].stagingBuffer.buffer, AllData[index].texture.image.image, static_cast<uint32_t>(AllData[index].texWidth), static_cast<uint32_t>(AllData[index].texHeight));

						cmd_transitionImageLayout(cmd, AllData[index].texture.image.image, AllData[index].image_format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

					}
				}
			}

			{
				ZoneScopedNC("Texture batch wait load queue", tracy::Color::Red);
				endSingleTimeCommands(cmd);
			}
			{
				ZoneScopedNC("Texture batch image view creation", tracy::Color::Blue);
				for (int i = 0; i < max_tex_upload; i++) {

					int index = batch * batches + i;
					if (index >= count) break;

					if (AllData[index].bLoaded) {
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
					else {
						requests[index].bLoaded = false;
					}
				}
			}
		}
	
	
}


void VulkanEngine::createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
	vk::MemoryPropertyFlags properties, AllocatedImage& image, bool bIsCubemap)
{
	vk::ImageCreateInfo imageInfo;
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.extent.width = static_cast<uint32_t>(width);
	imageInfo.extent.height = static_cast<uint32_t>(height);
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	if (bIsCubemap)
	{
		imageInfo.arrayLayers = 6;
	}
	else {
		imageInfo.arrayLayers = 1;
	}
	

	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;

	imageInfo.usage = usage;

	imageInfo.sharingMode = vk::SharingMode::eExclusive;

	imageInfo.samples = vk::SampleCountFlagBits::e1;
	if (bIsCubemap)
	{
		imageInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;
	}
	

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vmaallocInfo.requiredFlags = VkMemoryPropertyFlags(properties);
	VkImage img;

	VkImageCreateInfo imnfo = imageInfo;

	vmaCreateImage(allocator, &imnfo, &vmaallocInfo, &img, &image.allocation, nullptr);
	image.image = img;

}

vk::ImageView VulkanEngine::createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, bool bIsCubemap)
{
	vk::ImageViewCreateInfo viewInfo;
	viewInfo.image = image;
	viewInfo.viewType = bIsCubemap ? vk::ImageViewType::eCube:  vk::ImageViewType::e2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	if (bIsCubemap)
	{
		viewInfo.subresourceRange.layerCount = 6;
	}

	return device.createImageView(viewInfo);
}

void VulkanEngine::copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height, bool bIsCubemap)
{
	auto cmd = beginSingleTimeCommands();
	{
		tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
		TracyVkZone(profilercontext, VkCommandBuffer(cmd), "Upload Image");
		if (!bIsCubemap) {
			cmd_copyBufferToImage(cmd, buffer, image, width, height);
		}
		else {
			std::vector<vk::BufferImageCopy> bufferCopyRegions;
		
			for (uint32_t face = 0; face < 6; face++)
			{
				vk::BufferImageCopy region = {};
				region.bufferOffset = 0;
				region.bufferRowLength = 0;
				region.bufferImageHeight = 0;

				region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
				region.imageSubresource.mipLevel = 0;
				region.imageSubresource.baseArrayLayer = face;
				region.imageSubresource.layerCount = 1;

				region.imageOffset = vk::Offset3D{ 0, 0, 0 };
				region.imageExtent = vk::Extent3D{
					width,
					height,
					1
				};

				bufferCopyRegions.push_back(region);
			}
		
			cmd.copyBufferToImage(
				buffer,
				image,
				vk::ImageLayout::eTransferDstOptimal,
				bufferCopyRegions.size(),
				&bufferCopyRegions[0]);
			}
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

	region.imageOffset = vk::Offset3D{ 0, 0, 0 };
	region.imageExtent = vk::Extent3D{
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
void VulkanEngine::cmd_transitionImageLayout(vk::CommandBuffer& commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::ImageSubresourceRange range)
{
	vk::ImageMemoryBarrier barrier;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = range;
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
	else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
		barrier.srcAccessMask = vk::AccessFlags{};
		barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

		sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
		destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
	}
	else {
		throw std::invalid_argument("unsupported layout transition!");
	}

	commandBuffer.pipelineBarrier(sourceStage, destinationStage, vk::DependencyFlags{}, 0, nullptr, 0, nullptr, 1, &barrier);

}
void VulkanEngine::cmd_transitionImageLayout(vk::CommandBuffer& commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, bool bIsCubemap)
{
	vk::ImageSubresourceRange range;
	range.aspectMask = vk::ImageAspectFlagBits::eColor;
	range.baseMipLevel = 0;
	range.levelCount = 1;
	range.baseArrayLayer = 0;
	range.layerCount = bIsCubemap ? 6 : 1;

	cmd_transitionImageLayout(commandBuffer, image, format, oldLayout, newLayout, range);
}

struct TextureLoadRequest2 {
	
	bool bFailedLoade{ false };
	EntityID loadedTexture;
	std::string image_path;
	std::string textureName;	
};
struct StbInlineLoad {

	stbi_uc* pixel_data{ nullptr };
	int x, y, channels;

	~StbInlineLoad() {
		if (pixel_data) {
			stbi_image_free(pixel_data);
		}
	}
};

struct DirectInlineLoad {

	char* pixel_data{ nullptr };
	uint64_t data_size;
	int x, y, channels;
};

struct BaseTextureLoad {
	std::string path;
	std::string name;
	guid::BinaryGUID guid;
};

struct LoadStagingBuffer {
	AllocatedBuffer buffer;
};
struct LoadImageAlloc {
	AllocatedImage image;
	int x, y, channels;
	vk::Format format;
};

struct LoadImageAlloc;
class RealTextureLoader : public TextureLoader {
public:
	virtual bool should_flush();
	void add_request_from_assimp(const aiScene* scene, aiMaterial* material, aiTextureType textype,
		const std::string& scenepath) override final;

	void flush_requests() override final;

	void upload_pending_images();

	void uploadImage(vk::CommandBuffer cmd, vk::Image target_image, vk::Format format, const LoadStagingBuffer& staging, const LoadImageAlloc& texresource);

	virtual void preload_textures(sp::SceneLoader* loader);

	virtual void request_texture_load(guid::BinaryGUID textureGUID) override final;

	void add_load_from_db_coro(EntityID e, SceneLoader* loader, DbTexture& texture);
	void add_load_from_db(EntityID e, SceneLoader* loader, DbTexture& texture);


	sp::SceneLoader* preloaded_db;

	vk::CommandBuffer get_upload_command_buffer() {


		vk::CommandBufferAllocateInfo allocInfo;
		allocInfo.level = vk::CommandBufferLevel::ePrimary;
		allocInfo.commandPool = owner->transferCommandPool;
		allocInfo.commandBufferCount = 1;

		auto cmd = owner->device.allocateCommandBuffers(allocInfo)[0];
		

		//owner->device.resetCommandPool(owner->commandPool, vk::CommandPoolResetFlagBits::eReleaseResources);

		cmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
		return cmd;
	}

	void submit_command_buffer(vk::CommandBuffer cmd, bool withFence = false) {
		cmd.end();

		vk::SubmitInfo submitInfo;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmd;

		if (withFence) {

			if (uploadFence == nullptr) {
			
				vk::FenceCreateInfo info;
				
				uploadFence = owner->device.createFence(info);
			}


			owner->graphicsQueue.submit(submitInfo, uploadFence);
		}
		else {
			owner->graphicsQueue.submit(submitInfo, nullptr);
		}
	}
	void wait_queue_idle() {
		owner->graphicsQueue.waitIdle();
		owner->device.resetCommandPool(owner->transferCommandPool, vk::CommandPoolResetFlagBits{});
	}

	bool checkUploadFinished() {
		auto result = owner->device.waitForFences(1, (vk::Fence*)&uploadFence, true, 0);
		if (result == vk::Result::eTimeout) {
			return false;
		}
		else {
			return true;
		}
	}

	void finish_upload_command_buffer() {
		submit_command_buffer(upload_buffer);
		wait_queue_idle();
	}

	


	VulkanEngine* owner;

	vk::CommandBuffer upload_buffer{};

	VkFence uploadFence{VK_NULL_HANDLE};

	std::unordered_map<std::string, EntityID> path_references;

	std::unordered_map<std::string, DbTexture> pending_db_textures;

	std::unordered_map<guid::BinaryGUID, EntityID> pending_loads;

	std::vector<guid::BinaryGUID> load_requests;

	std::vector<EntityID> loading_resources;

	bool bLoadingTextures{ false };
	bool bLoadedDB{false};
	//std::vector<TextureLoadRequest2> requests;

	entt::registry load_registry;

	//200 megabytes
	static constexpr size_t max_image_buffer_size = 1024L * 1024L * 200;
	
	AllocatedBuffer staging_buffer;

	void finish_image_batch();
	void add_request_from_assimp_db(SceneLoader* loader, aiMaterial* material, aiTextureType textype, const std::string& scenepath);

	virtual void load_all_textures(SceneLoader* loader, const std::string& scenepath) override;
	void load_all_textures_coroutines(SceneLoader* loader, const std::string& scenepath);
	virtual void update_background_loads() override;
};


bool RealTextureLoader::should_flush()
{
	return load_registry.view<BaseTextureLoad>().size() > 40;
}

void RealTextureLoader::add_request_from_assimp(const aiScene* scene, aiMaterial* material, aiTextureType textype, const std::string& scenepath)
{
	aiString texpath;
	if (material->GetTextureCount(textype))
	{
		material->GetTexture(textype, 0, &texpath);

		const char* txpath = &texpath.data[0];
		char* ch = &texpath.data[1];

		for (int i = 0; i < texpath.length; i++)
		{
			if (texpath.data[i] == '\\')
			{
				texpath.data[i] = '/';
			}
		}
		std::filesystem::path texture_path{ txpath };

		std::string tx_path = scenepath + "/" + texture_path.string();

		auto load_entity = path_references.find(tx_path);
		if (load_entity == path_references.end()) {

			ZoneScopedNC("Texture load from stbi buffer", tracy::Color::Red);

			EntityID load_id = load_registry.create();

			if (auto texture = scene->GetEmbeddedTexture(texpath.C_Str())) {
				size_t tex_size = texture->mHeight * texture->mWidth;
				if (texture->mHeight == 0) {
					tex_size = texture->mWidth;
				}
				//int x, y, c;
				StbInlineLoad load;
				
				load.pixel_data = stbi_load_from_memory((stbi_uc*)texture->pcData, tex_size, &load.x, &load.y, &load.channels, STBI_rgb_alpha);

				if (load.pixel_data) {
					load.channels = STBI_rgb_alpha;
					load_registry.assign_or_replace<StbInlineLoad>(load_id,load);

					BaseTextureLoad bload;
					bload.path = tx_path;
					bload.name = txpath;

					load_registry.assign_or_replace<BaseTextureLoad>(load_id, bload);

					path_references[tx_path] = load_id;
				}
			}
			else {
				BaseTextureLoad load;
				load.path = tx_path;
				load.name = txpath;

				//load_registry.assign_or_replace<BaseTextureLoad>(load_id, load);
				//path_references[tx_path] = load_id;
			}
		}
	}
}

vk::Format get_image_format_from_stbi(int channels) {
	switch (channels) {
	case 1:
		return vk::Format::eR8Unorm;
		break;
	case 2:
		return vk::Format::eR8G8Unorm;
		break;
	case 3:
		return vk::Format::eR8G8B8Unorm;
		break;
	case 4:
		return vk::Format::eR8G8B8A8Unorm;
		break;
	}
	return vk::Format{};
};

void RealTextureLoader::flush_requests()
{
	const int max_images_per_batch = 20;



	auto stb_view = load_registry.view<StbInlineLoad>();
	auto direct_view = load_registry.view<DirectInlineLoad>();
	
	while (true) {
		ZoneScopedNC("Texture request batch", tracy::Color::Yellow);
	//
		int current_batch = 0;
		for (auto e : stb_view) {			

			ZoneScopedNC("Texture upload", tracy::Color::Orange);

			const StbInlineLoad& load = stb_view.get(e);
			LoadStagingBuffer& staging = load_registry.assign<LoadStagingBuffer>(e);
			LoadImageAlloc& texresource = load_registry.assign<LoadImageAlloc>(e);
			const BaseTextureLoad& texload = load_registry.get<BaseTextureLoad>(e);
			size_t image_size = load.x * load.y * load.channels;

			texresource.format = get_image_format_from_stbi(load.channels);
			texresource.x = load.x;
			texresource.y = load.y;
			texresource.channels = load.channels;


			owner->createBuffer(image_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_UNKNOWN,
				staging.buffer);

			void* data;
			vmaMapMemory(owner->allocator, staging.buffer.allocation, &data);

			memcpy(data, load.pixel_data, static_cast<size_t>(image_size));

			vmaUnmapMemory(owner->allocator, staging.buffer.allocation);			

			owner->createImage(load.x, load.y, texresource.format,
				vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
				vk::MemoryPropertyFlagBits::eDeviceLocal, texresource.image);

			current_batch++;
			if (current_batch >= max_images_per_batch) { break; };		
		}
		if (current_batch < max_images_per_batch) {
			for (auto e : direct_view) {

				ZoneScopedNC("Texture upload-direct", tracy::Color::Orange);

				const DirectInlineLoad& load = direct_view.get(e);
				LoadStagingBuffer& staging = load_registry.assign<LoadStagingBuffer>(e);
				LoadImageAlloc& texresource = load_registry.assign<LoadImageAlloc>(e);
				const BaseTextureLoad& texload = load_registry.get<BaseTextureLoad>(e);
				size_t image_size = load.data_size;//load.x * load.y * load.channels;


				texresource.x = load.x;
				texresource.y = load.y;
				texresource.channels = load.channels;


				owner->createBuffer(image_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_UNKNOWN,
					staging.buffer);

				void* data;
				vmaMapMemory(owner->allocator, staging.buffer.allocation, &data);

				memcpy(data, load.pixel_data, static_cast<size_t>(image_size));

				vmaUnmapMemory(owner->allocator, staging.buffer.allocation);

				owner->createImage(load.x, load.y, texresource.format,
					vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
					vk::MemoryPropertyFlagBits::eDeviceLocal, texresource.image);

				current_batch++;
				if (current_batch >= max_images_per_batch) { break; };
			}
		}
		if (current_batch <= 0) { break; };


		

		upload_pending_images();

	}
}

void RealTextureLoader::upload_pending_images()
{
	{
		owner->device.resetCommandPool(owner->transferCommandPool, vk::CommandPoolResetFlagBits{});
	
		vk::CommandBuffer cmd = get_upload_command_buffer();
		auto upload_view = load_registry.view<LoadStagingBuffer, LoadImageAlloc>();
		tracy::VkCtx* profilercontext = (tracy::VkCtx*)owner->get_profiler_context(cmd);
		

		ZoneScopedN("Copying images to GPU");
		auto view_iterator = upload_view.begin();
		while (true) {
			//submit buffers every 10 textures;
			for (int i = 0; i < 10; i++) {
				if (view_iterator == upload_view.end()) {
					goto end;
				}
				EntityID e = *view_iterator;
				view_iterator++;

				TracyVkZone(profilercontext, VkCommandBuffer(cmd), "Upload Image");
				const LoadStagingBuffer& staging = upload_view.get<LoadStagingBuffer>(e);
				const LoadImageAlloc& texresource = upload_view.get<LoadImageAlloc>(e);


				vk::Image target_image = texresource.image.image;
				uploadImage(cmd, target_image, texresource.format, staging, texresource);
			}
			{
				ZoneScopedNC("Texture batch queue upload", tracy::Color::Red);
				submit_command_buffer(cmd);
				cmd = get_upload_command_buffer();
			}
			profilercontext = (tracy::VkCtx*)owner->get_profiler_context(cmd);
		}
		end:

		{
			ZoneScopedNC("Texture batch wait load queue", tracy::Color::Red);

			submit_command_buffer(cmd);
			wait_queue_idle();
			//finish_upload_command_buffer();
			
			finish_image_batch();
		}
	}
}

void RealTextureLoader::uploadImage(vk::CommandBuffer cmd, vk::Image target_image, vk::Format format, const LoadStagingBuffer& staging, const LoadImageAlloc& texresource)
{
	//TracyVkZone(profilercontext, VkCommandBuffer(cmd), "Upload Image");
	owner->cmd_transitionImageLayout(cmd, target_image, format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

	owner->cmd_copyBufferToImage(cmd, staging.buffer.buffer, target_image, static_cast<uint32_t>(texresource.x), static_cast<uint32_t>(texresource.y));

	owner->cmd_transitionImageLayout(cmd, target_image, format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
}
void RealTextureLoader::add_load_from_db_coro(EntityID e, SceneLoader* loader, DbTexture& texture)
{
	ZoneScopedNC("Texture upload-direct", tracy::Color::Orange);


	LoadStagingBuffer& staging = load_registry.assign<LoadStagingBuffer>(e);
	LoadImageAlloc& texresource = load_registry.assign<LoadImageAlloc>(e);
	const BaseTextureLoad& texload = load_registry.get<BaseTextureLoad>(e);
	size_t image_size = texture.byte_size;

	texresource.format = vk::Format{ texture.vk_format };
	texresource.x = texture.size_x;
	texresource.y = texture.size_y;
	texresource.channels = texture.channels;


	owner->createBuffer(image_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_UNKNOWN,
		staging.buffer);

	void* data;
	vmaMapMemory(owner->allocator, staging.buffer.allocation, &data);

	memcpy(data, texture.data_raw, image_size);

	vmaUnmapMemory(owner->allocator, staging.buffer.allocation);

	owner->createImage(texture.size_x, texture.size_y, texresource.format,
		vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal, texresource.image);
}
void RealTextureLoader::add_load_from_db(EntityID e ,SceneLoader* loader,DbTexture& texture)
{
	ZoneScopedNC("Texture upload-direct", tracy::Color::Orange);


	LoadStagingBuffer& staging = load_registry.assign<LoadStagingBuffer>(e);
	LoadImageAlloc& texresource = load_registry.assign<LoadImageAlloc>(e);
	const BaseTextureLoad& texload = load_registry.get<BaseTextureLoad>(e);
	size_t image_size = texture.byte_size;

	texresource.format = vk::Format{ texture.vk_format };
	texresource.x = texture.size_x;
	texresource.y = texture.size_y;
	texresource.channels = texture.channels;
	


	owner->createBuffer(image_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_UNKNOWN,
		staging.buffer);

	void* data;
	vmaMapMemory(owner->allocator, staging.buffer.allocation, &data);

	//load directly from DB to buffer
	//loader->load_db_texture(texture.name, data);
	loader->load_db_texture(texture.guid, data);
	vmaUnmapMemory(owner->allocator, staging.buffer.allocation);

	owner->createImage(texture.size_x, texture.size_y, texresource.format,
		vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal, texresource.image);
}

void RealTextureLoader::finish_image_batch()
{
	auto upload_view = load_registry.view<LoadStagingBuffer, LoadImageAlloc,BaseTextureLoad>();

	ZoneScopedNC("Texture batch image view creation", tracy::Color::Blue);
	
	for (auto e : upload_view) {
		const LoadStagingBuffer& staging = upload_view.get<LoadStagingBuffer>(e);
		const LoadImageAlloc& texresource = upload_view.get<LoadImageAlloc>(e);
		const BaseTextureLoad& texload = upload_view.get<BaseTextureLoad>(e);
		TextureResource newResource;
		newResource.image = texresource.image;

		auto format = texresource.format;//get_image_format_from_stbi(texresource.channels);

		vmaDestroyBuffer(owner->allocator, staging.buffer.buffer, staging.buffer.allocation);

		newResource.imageView = owner->createImageView(texresource.image.image, format, vk::ImageAspectFlagBits::eColor);

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

		newResource.textureSampler = owner->device.createSampler(samplerInfo);
		newResource.bFullyLoaded = true;
		TextureResourceMetadata metadata;
		metadata.image_format = format;
		metadata.texture_size.x = texresource.x;
		metadata.texture_size.y = texresource.y;

		if (owner->loadedTextures.find(texload.guid) != owner->loadedTextures.end())
		{
			 EntityID res = owner->loadedTextures[texload.guid];
			 newResource.bindlessHandle = owner->getResource<TextureResource>(res).bindlessHandle;
			owner->render_registry.replace<TextureResource>(res, newResource);
			owner->render_registry.replace<TextureResourceMetadata>(res, metadata);


			if (owner->texCache->image_Ids.size() > newResource.bindlessHandle && newResource.bindlessHandle > 0) {
			
				if (owner->texCache->image_Ids[newResource.bindlessHandle] == res) {
					owner->texCache->Refresh(newResource, newResource.bindlessHandle);
				}
			}

		}
		else {


			auto new_entity = owner->createResource(texload.name.c_str(), newResource);

			owner->render_registry.assign<TextureResourceMetadata>(new_entity, metadata);

			owner->loadedTextures[texload.guid] = new_entity;
		}

		load_registry.destroy(e);
	}
}

void RealTextureLoader::add_request_from_assimp_db(SceneLoader* loader, aiMaterial* material, aiTextureType textype, const std::string& scenepath)
{
	load_all_textures(loader, scenepath);
	return;
#if 0

	aiString texpath;
	if (material->GetTextureCount(textype))
	{
		material->GetTexture(textype, 0, &texpath);

		const char* txpath = &texpath.data[0];
		char* ch = &texpath.data[1];

		for (int i = 0; i < texpath.length; i++)
		{
			if (texpath.data[i] == '\\')
			{
				texpath.data[i] = '/';
			}
		}
		std::filesystem::path texture_path{ txpath };

		std::string tx_path = scenepath + "/" + texture_path.string();

		auto load_entity = path_references.find(tx_path);
		if (load_entity == path_references.end()) {

			ZoneScopedNC("Texture load from database buffer", tracy::Color::Red);
			EntityID load_id = load_registry.create();

			auto pending_load = pending_db_textures.find(txpath);
			if (pending_load != pending_db_textures.end()) {

				BaseTextureLoad bload;
				bload.path = tx_path;
				bload.name = txpath;
				bload.guid = pending_load->second.guid;
				load_registry.assign_or_replace<BaseTextureLoad>(load_id, bload);

				this->add_load_from_db(load_id, loader, pending_load->second);

				pending_db_textures.erase(txpath);

				if (should_flush()) {
					ZoneScopedNC("Texture request flush load", tracy::Color::Yellow);
					upload_pending_images();
				}
			}
			else {
				ZoneScopedNC("Texture load from database pending", tracy::Color::Orange);
				DbTexture texture;

				if (loader->load_db_texture(txpath, texture) == 0) {

					//int x, y, c;
					DirectInlineLoad load;

					load.pixel_data = (char*)texture.data_raw;
					load.data_size = texture.byte_size;
					

					load.x = texture.size_x;
					load.y = texture.size_y;


					if (load.pixel_data) {
						load.channels = texture.channels;
						load_registry.assign_or_replace<DirectInlineLoad>(load_id, load);

						BaseTextureLoad bload;
						bload.path = tx_path;
						bload.name = txpath;
						bload.guid = texture.guid;

						load_registry.assign_or_replace<BaseTextureLoad>(load_id, bload);

						path_references[tx_path] = load_id;
					}

					if (should_flush()) {
						ZoneScopedNC("Texture request flush load", tracy::Color::Yellow);
						flush_requests();
					}
				}
			}
		}
	}
#endif
}

void RealTextureLoader::load_all_textures(SceneLoader* loader, const std::string& scenepath)
{
	//load_all_textures_coroutines(loader, scenepath);
	if (!bLoadedDB) {
		ZoneScopedNC("Database load all textures", tracy::Color::Red);
		//std::vector<DbTexture> textures;
		{
			ZoneScopedNC("Database fetch textures metadata", tracy::Color::Yellow1);
			//loader->load_textures_from_db("", textures);
		}
	
		owner->device.resetCommandPool(owner->transferCommandPool, vk::CommandPoolResetFlagBits{});
	
		vk::CommandBuffer cmd = get_upload_command_buffer();
		//auto upload_view = load_registry.view<LoadStagingBuffer, LoadImageAlloc>();
		tracy::VkCtx* profilercontext = (tracy::VkCtx*)owner->get_profiler_context(cmd);
		int ntextures = 0;
		for (auto t : loader->load_all_textures()){//textures) {
			
			EntityID load_id = load_registry.create();
	
	
			std::string tx_path = scenepath + "/" + t.name;
	
			BaseTextureLoad bload;
			bload.path = tx_path;
			bload.name = t.name;
			bload.guid = t.guid;
			load_registry.assign_or_replace<BaseTextureLoad>(load_id, bload);
	
			add_load_from_db(load_id, loader, t);
	
			const LoadStagingBuffer& staging = load_registry.get<LoadStagingBuffer>(load_id);
			const LoadImageAlloc& texresource = load_registry.get<LoadImageAlloc>(load_id);
	
			vk::Image target_image = texresource.image.image; 
			{
			TracyVkZone(profilercontext, VkCommandBuffer(cmd), "Upload Image");
			uploadImage(cmd, target_image, texresource.format, staging, texresource);
			}
	
			ntextures++;
			if (ntextures > 20) {
				ZoneScopedNC("Texture batch submit", tracy::Color::Red);
				submit_command_buffer(cmd);
				cmd = get_upload_command_buffer();
				profilercontext = (tracy::VkCtx*)owner->get_profiler_context(cmd);
				ntextures = 0;
			}
		}
	
	
		{
			ZoneScopedNC("Texture batch wait load queue", tracy::Color::Red);
	
			submit_command_buffer(cmd);
			wait_queue_idle();
			//finish_upload_command_buffer();0
	
			finish_image_batch();
		}
	}


	
	
	{
			ZoneScopedNC("Texture request flush load", tracy::Color::Yellow);
			upload_pending_images();
			bLoadedDB = true;
	}


	return;
}

struct LoadBatch {
	std::vector<BaseTextureLoad> loads;
	std::vector< LoadStagingBuffer> stagings;
	std::vector< LoadImageAlloc> allocs;
};

void RealTextureLoader::load_all_textures_coroutines(SceneLoader* loader, const std::string& scenepath)
{
	if (!bLoadedDB) {
		ZoneScopedNC("Database load all textures", tracy::Color::Red);
		
		owner->device.resetCommandPool(owner->transferCommandPool, vk::CommandPoolResetFlagBits{});

		vk::CommandBuffer cmd = get_upload_command_buffer();
		auto upload_view = load_registry.view<LoadStagingBuffer, LoadImageAlloc>();
		tracy::VkCtx* profilercontext = (tracy::VkCtx*)owner->get_profiler_context(cmd);
		int ntextures = 20;

		auto texture_generator = loader->load_all_textures();

		auto texture_iterator = texture_generator.begin();
		while (texture_iterator != texture_generator.end())
		{
			for (int ntex = 0; ntex < ntextures; ntex++) {
				if (texture_iterator != texture_generator.end()) {
					DbTexture &t = (*texture_iterator);

					EntityID load_id = load_registry.create();
					std::string tx_path = scenepath + "/" + t.name;

					BaseTextureLoad bload;
					bload.path = tx_path;
					bload.name = t.name;

					load_registry.assign_or_replace<BaseTextureLoad>(load_id, bload);

					{
						ZoneScopedNC("Texture upload-direct", tracy::Color::Orange);


						LoadStagingBuffer& staging = load_registry.assign<LoadStagingBuffer>(load_id);
						LoadImageAlloc& texresource = load_registry.assign<LoadImageAlloc>(load_id);
						const BaseTextureLoad& texload = load_registry.get<BaseTextureLoad>(load_id);
						size_t image_size = t.byte_size;

						texresource.format = vk::Format{ t.vk_format };
						texresource.x = t.size_x;
						texresource.y = t.size_y;
						texresource.channels = t.channels;


						owner->createBuffer(image_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_UNKNOWN,
							staging.buffer);

						void* data;
						vmaMapMemory(owner->allocator, staging.buffer.allocation, &data);

						memcpy(data, t.data_raw, image_size);

						vmaUnmapMemory(owner->allocator, staging.buffer.allocation);

						owner->createImage(t.size_x, t.size_y, texresource.format,
							vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
							vk::MemoryPropertyFlagBits::eDeviceLocal, texresource.image);
					}
					const LoadStagingBuffer& staging = load_registry.get<LoadStagingBuffer>(load_id);
					const LoadImageAlloc& texresource = load_registry.get<LoadImageAlloc>(load_id);

					vk::Image target_image = texresource.image.image;
					{
						TracyVkZone(profilercontext, VkCommandBuffer(cmd), "Upload Image");
						uploadImage(cmd, target_image, texresource.format, staging, texresource);
					}

					texture_iterator++;

					free(t.data_raw);
				}				
			}

			ZoneScopedNC("Texture batch submit", tracy::Color::Red);
			submit_command_buffer(cmd);
			cmd = get_upload_command_buffer();
			profilercontext = (tracy::VkCtx*)owner->get_profiler_context(cmd);
		}

		{
			ZoneScopedNC("Texture batch wait load queue", tracy::Color::Red);
			
			wait_queue_idle();
			finish_image_batch();
		}
	}
}

TextureLoader* TextureLoader::create_new_loader(VulkanEngine* ownerEngine)
{
	auto loader = new RealTextureLoader;
	loader->owner = ownerEngine;

	vk::DeviceSize buffersize = RealTextureLoader::max_image_buffer_size;

	return loader;
}
struct LoadPacket {
	guid::BinaryGUID guid;
	void* mappedData;
	EntityID entity;
};

struct TextureAsyncLoadState {
	std::vector<LoadPacket> packets;
	std::future<void> async_f;
	std::atomic<bool> isDone;
};

enum class ETextureAsyncLoad {
	Idle,
	AsyncLoad,
	WaitVulkan
};
void RealTextureLoader::update_background_loads()
{
	static ETextureAsyncLoad loadState = ETextureAsyncLoad::Idle;
	static TextureAsyncLoadState* asyncstate = nullptr;

	if (loadState == ETextureAsyncLoad::WaitVulkan) {
		if (checkUploadFinished()) {
			ZoneScopedNC("Texture batch wait load queue", tracy::Color::Red);
			finish_image_batch();

			loadState = ETextureAsyncLoad::Idle;
		}
		else {
			return;
		}
	}
	else if (loadState == ETextureAsyncLoad::AsyncLoad)
	{
		if (asyncstate->isDone == true) {
		
			ZoneScopedNC("Texture ASync loads", tracy::Color::Orange3);
			vk::CommandBuffer cmd = get_upload_command_buffer();

			tracy::VkCtx* profilercontext = (tracy::VkCtx*)owner->get_profiler_context(cmd);

			for (auto p : asyncstate->packets) {
				const LoadStagingBuffer& staging = load_registry.get<LoadStagingBuffer>(p.entity);
				DbTexture& texture = load_registry.get<DbTexture>(p.entity);
				LoadImageAlloc& texresource = load_registry.get<LoadImageAlloc>(p.entity);

				vmaUnmapMemory(owner->allocator, staging.buffer.allocation);

				owner->createImage(texture.size_x, texture.size_y, texresource.format,
					vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
					vk::MemoryPropertyFlagBits::eDeviceLocal, texresource.image);

				vk::Image target_image = texresource.image.image;

				{
					TracyVkZone(profilercontext, VkCommandBuffer(cmd), "Upload Image");
					uploadImage(cmd, target_image, texresource.format, staging, texresource);
				}
			}
			delete asyncstate;
			asyncstate = nullptr;
			{
				if (uploadFence != nullptr)
				{
					owner->device.resetFences(1, (vk::Fence*) & uploadFence);
				}

				submit_command_buffer(cmd, true);
			}

			loadState = ETextureAsyncLoad::WaitVulkan;
		}
	}

	if(loadState == ETextureAsyncLoad::Idle &&  load_requests.size() > 0)
	{
		ZoneScopedNC("Database load all textures", tracy::Color::Red);
		
		owner->device.resetCommandPool(owner->transferCommandPool, vk::CommandPoolResetFlagBits{});

		int ntextures = 0;

		std::vector<EntityID> batch_to_load;
		batch_to_load.reserve(10);
		int i = load_requests.size() - 1;
		//check the load requests, so they arent duplicated, and put the entities in the vector above
		while(i >= 0) {
			auto gd = load_requests[i];
			i--;
			auto it = pending_loads.find(gd);
			if (it != pending_loads.end()) {
				EntityID e = pending_loads[gd];
				if (load_registry.valid(e)) {
					if (load_registry.has<BaseTextureLoad>(e))
					{
						load_requests.pop_back();
						batch_to_load.push_back(e);

						pending_loads.erase(it);
						if (batch_to_load.size() >= 10) {
							break;
						}
					}
				}
			}			
		}
		vk::CommandBuffer cmd = get_upload_command_buffer();

		if (asyncstate == nullptr)
		{
			asyncstate = new TextureAsyncLoadState();
		}

		tracy::VkCtx* profilercontext = (tracy::VkCtx*)owner->get_profiler_context(cmd);
		for (auto e : batch_to_load) {
			loadState = ETextureAsyncLoad::AsyncLoad;
			//bLoadingTextures = true;
			EntityID load_id = e;

			auto t = load_registry.get<DbTexture>(load_id);
			
			BaseTextureLoad bload = load_registry.get<BaseTextureLoad>(load_id);

			EntityID resourceid = owner->loadedTextures[bload.guid];

			loading_resources.push_back(resourceid);
			//add_load_from_db(load_id, preloaded_db, t);

			{
				ZoneScopedNC("Texture upload-direct", tracy::Color::Orange);

				DbTexture& texture = t;
				LoadStagingBuffer& staging = load_registry.assign<LoadStagingBuffer>(e);
				LoadImageAlloc& texresource = load_registry.assign<LoadImageAlloc>(e);
				const BaseTextureLoad& texload = load_registry.get<BaseTextureLoad>(e);
				size_t image_size = t.byte_size;

				texresource.format = vk::Format{ texture.vk_format };
				texresource.x = texture.size_x;
				texresource.y = texture.size_y;
				texresource.channels = texture.channels;

				owner->createBuffer(image_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_UNKNOWN,
					staging.buffer);

				void* data;
				vmaMapMemory(owner->allocator, staging.buffer.allocation, &data);

				LoadPacket packet;
				packet.guid = texture.guid;
				packet.mappedData = data;
				packet.entity = e;
				asyncstate->packets.push_back(packet);
			}
		}

		asyncstate->isDone = false;
		asyncstate->async_f = std::async(std::launch::async, [this]() { 
			
			for (auto p : asyncstate->packets) {
				ZoneScopedNC("DB GUID LOAD", tracy::Color::Red);
				LoadImageAlloc& texresource = load_registry.get<LoadImageAlloc>(p.entity);

				preloaded_db->load_db_texture(p.guid, p.mappedData);
			}		
			asyncstate->isDone = true;
		});

	}
}

void RealTextureLoader::preload_textures(sp::SceneLoader* loader)
{
	preloaded_db = loader;
	for (auto tx : loader->preload_all_textures()) {
		EntityID load_id = load_registry.create();

		//std::string tx_path = scenepath + "/" + t.name;

		BaseTextureLoad bload;
		bload.path = "nopath";
		bload.name = tx.name;
		bload.guid = tx.guid;
		load_registry.assign_or_replace<BaseTextureLoad>(load_id, bload);
		load_registry.assign_or_replace<DbTexture>(load_id, tx);

		pending_loads[tx.guid] = load_id;

		load_requests.push_back(tx.guid);
		//create entity for the resource on main engine, reserving it
		TextureResource newResource;
		newResource.bindlessHandle = -1;

		auto new_entity = owner->createResource(tx.name.c_str(), newResource);

		TextureResourceMetadata metadata;
		metadata.image_format = vk::Format{ tx.vk_format };
		metadata.texture_size.x = tx.size_x;
		metadata.texture_size.y = tx.size_y;

		owner->render_registry.assign<TextureResourceMetadata>(new_entity, metadata);

		owner->loadedTextures[tx.guid] = new_entity;
	}
}

void RealTextureLoader::request_texture_load(guid::BinaryGUID textureGUID)
{
	load_requests.push_back(textureGUID);
}

void TextureBindlessCache::AddToCache(TextureResource& resource, EntityID id)
{

	vk::DescriptorImageInfo newImage = {};
	newImage.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	newImage.imageView = resource.imageView;
	newImage.sampler = resource.textureSampler;

	//if image is null, we are adding a dummy, so duplicate the index zero which is black or white default
	if (VkImage(resource.image.image) == VK_NULL_HANDLE || !resource.bFullyLoaded) {
		newImage = all_images[0];//.back();
	}
	

	resource.bindlessHandle = image_Ids.size();

	all_images.push_back(newImage);
	image_Ids.push_back(id);
}

void TextureBindlessCache::Refresh(TextureResource& resource, int index)
{
	vk::DescriptorImageInfo newImage = {};
	newImage.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	newImage.imageView = resource.imageView;
	newImage.sampler = resource.textureSampler;


	all_images[index] = newImage;
}

