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
	createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer);


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

	return { texture,metadata };
}

EntityID VulkanEngine::load_texture(const char* image_path, std::string textureName, bool bIsCubemap)
{
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


void VulkanEngine::create_texture_image_view()
{
	tset_textureImageView = createImageView(test_textureImage.image, vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor);//device.createImageView(viewInfo);

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

				region.imageOffset = { 0, 0, 0 };
				region.imageExtent = {
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

class RealTextureLoader : public TextureLoader {
public:
	void add_request_from_assimp(const aiScene* scene, aiMaterial* material, aiTextureType textype,
		const std::string& scenepath) override final;

	void flush_requests() override final;

	VulkanEngine* owner;


	std::unordered_map<std::string, EntityID> path_references;
	//std::vector<TextureLoadRequest2> requests;

	entt::registry load_registry;

	//200 megabytes
	static constexpr size_t max_image_buffer_size = 1024L * 1024L * 200;
	
	AllocatedBuffer staging_buffer;

	void finish_image_batch();
};

struct StbInlineLoad {

	stbi_uc* pixel_data;
	int x, y, channels;
};

struct BaseTextureLoad {
	std::string path;
	std::string name;
};

struct LoadStagingBuffer {
	AllocatedBuffer buffer;
};
struct LoadImageAlloc {
	AllocatedImage image;
};

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

	vk::CommandBuffer cmd;
	bool cmd_needs_end = false;

	auto stb_view = load_registry.view<StbInlineLoad>();
	auto upload_view = load_registry.view<StbInlineLoad, LoadStagingBuffer, LoadImageAlloc>();
	while (true) {
		ZoneScopedNC("Texture request batch", tracy::Color::Yellow);
	//
		int current_batch = 0;
		for (auto e : stb_view) {			

			ZoneScopedNC("Texture upload", tracy::Color::Orange);

			//if (!load_registry.has<LoadImageAlloc>(e)) {
				const StbInlineLoad& load = stb_view.get(e);
				LoadStagingBuffer& staging = load_registry.assign<LoadStagingBuffer>(e);
				LoadImageAlloc& texresource = load_registry.assign<LoadImageAlloc>(e);
				const BaseTextureLoad& texload = load_registry.get<BaseTextureLoad>(e);
				size_t image_size = load.x * load.y * load.channels;

				owner->createBuffer(image_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
					staging.buffer);

				void* data;
				vmaMapMemory(owner->allocator, staging.buffer.allocation, &data);

				memcpy(data, load.pixel_data, static_cast<size_t>(image_size));

				vmaUnmapMemory(owner->allocator, staging.buffer.allocation);

				stbi_image_free(load.pixel_data);

				owner->createImage(load.x, load.y, get_image_format_from_stbi(load.channels),
					vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
					vk::MemoryPropertyFlagBits::eDeviceLocal, texresource.image);

				current_batch++;
				if (current_batch >= max_images_per_batch) { break; };

				

				//const StbInlineLoad& load = upload_view.get<StbInlineLoad>(e);
				//const LoadStagingBuffer& staging = upload_view.get<LoadStagingBuffer>(e);
				//const LoadImageAlloc& texresource = upload_view.get<LoadImageAlloc>(e);
				//auto format = get_image_format_from_stbi(load.channels);
				//
				//vk::Image target_image = texresource.image.image;
				//owner->cmd_transitionImageLayout(cmd, target_image, format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
				//
				//owner->cmd_copyBufferToImage(cmd, staging.buffer.buffer, target_image, static_cast<uint32_t>(load.x), static_cast<uint32_t>(load.y));
				//
				//owner->cmd_transitionImageLayout(cmd, target_image, format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
				//owner->endSingleTimeCommands(cmd);
				////finish_image_batch();
				//
				//TextureResource newResource;
				//newResource.image = texresource.image;
				//
				////auto format = get_image_format_from_stbi(load.channels);
				//
				//vmaDestroyBuffer(owner->allocator, staging.buffer.buffer, staging.buffer.allocation);
				//
				//newResource.imageView = owner->createImageView(texresource.image.image, format, vk::ImageAspectFlagBits::eColor);
				//
				//vk::SamplerCreateInfo samplerInfo;
				//samplerInfo.magFilter = vk::Filter::eLinear;
				//samplerInfo.minFilter = vk::Filter::eLinear;
				//samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
				//samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
				//samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
				//
				//samplerInfo.anisotropyEnable = VK_TRUE;
				//samplerInfo.maxAnisotropy = 16;
				//
				//samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
				//samplerInfo.unnormalizedCoordinates = VK_FALSE;
				//
				//samplerInfo.compareEnable = VK_FALSE;
				//samplerInfo.compareOp = vk::CompareOp::eAlways;
				//
				//samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
				//samplerInfo.mipLodBias = 0.0f;
				//samplerInfo.minLod = 0.0f;
				//samplerInfo.maxLod = 0.0f;
				//
				//newResource.textureSampler = owner->device.createSampler(samplerInfo);
				//
				//auto new_entity = owner->createResource(texload.name.c_str(), newResource);
				//
				//TextureResourceMetadata metadata;
				//metadata.image_format = format;
				//metadata.texture_size.x = load.x;
				//metadata.texture_size.y = load.y;
				//
				//owner->render_registry.assign<TextureResourceMetadata>(new_entity, metadata);
				//
				//load_registry.destroy(e);

				//current_batch++;
				//if (current_batch >= max_images_per_batch) { break; };
			//}			
		}
		if (current_batch <= 0) { break; };



		//if(cmd_needs_end)
		//{
		//	ZoneScopedNC("Texture batch wait load queue", tracy::Color::Red);
		//	owner->endSingleTimeCommands(cmd);
		//	finish_image_batch();
		//}
		//
		cmd = owner->beginSingleTimeCommands();
		//cmd_needs_end = true;
		{
			tracy::VkCtx* profilercontext = (tracy::VkCtx*)owner->get_profiler_context(cmd);
			//TracyVkZone(profilercontext, VkCommandBuffer(cmd), "Upload Image");
		
			ZoneScopedN("Copying images to GPU");
		
			for (auto e : upload_view) {
		
				const StbInlineLoad& load = upload_view.get<StbInlineLoad>(e);
				const LoadStagingBuffer& staging = upload_view.get<LoadStagingBuffer>(e);
				const LoadImageAlloc& texresource = upload_view.get<LoadImageAlloc>(e);
				auto format = get_image_format_from_stbi(load.channels);
		
				vk::Image target_image = texresource.image.image;
				owner->cmd_transitionImageLayout(cmd, target_image, format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
		
				owner->cmd_copyBufferToImage(cmd, staging.buffer.buffer, target_image, static_cast<uint32_t>(load.x), static_cast<uint32_t>(load.y));
		
				owner->cmd_transitionImageLayout(cmd, target_image, format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		
				//load_registry.remove<StbInlineLoad>(e);
				//load_registry.remove<LoadStagingBuffer>(e);
			}
			{
				ZoneScopedNC("Texture batch wait load queue", tracy::Color::Red);
				owner->endSingleTimeCommands(cmd);
				finish_image_batch();
			}
		}
	}
	//if (cmd_needs_end)
	//{
	//	ZoneScopedNC("Texture batch wait load queue", tracy::Color::Red);
	//	owner->endSingleTimeCommands(cmd);
	//	finish_image_batch();
	//}
	
}

void RealTextureLoader::finish_image_batch()
{
	auto stb_view = load_registry.view<StbInlineLoad>();
	auto upload_view = load_registry.view<StbInlineLoad, LoadStagingBuffer, LoadImageAlloc,BaseTextureLoad>();

	ZoneScopedNC("Texture batch image view creation", tracy::Color::Blue);
	
	for (auto e : upload_view) {
		const StbInlineLoad& load = upload_view.get<StbInlineLoad>(e);
		const LoadStagingBuffer& staging = upload_view.get<LoadStagingBuffer>(e);
		const LoadImageAlloc& texresource = upload_view.get<LoadImageAlloc>(e);
		const BaseTextureLoad& texload = upload_view.get<BaseTextureLoad>(e);
		TextureResource newResource;
		newResource.image = texresource.image;

		auto format = get_image_format_from_stbi(load.channels);

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

		auto new_entity = owner->createResource(texload.name.c_str(), newResource);

		TextureResourceMetadata metadata;
		metadata.image_format = format;
		metadata.texture_size.x = load.x;
		metadata.texture_size.y = load.y;

		owner->render_registry.assign<TextureResourceMetadata>(new_entity, metadata);

		load_registry.destroy(e);
	}
}

TextureLoader* TextureLoader::create_new_loader(VulkanEngine* ownerEngine)
{
	auto loader = new RealTextureLoader;
	loader->owner = ownerEngine;

	vk::DeviceSize buffersize = RealTextureLoader::max_image_buffer_size;
	


	//allocate a big buffer to use for uploads
	//ownerEngine->createBuffer(buffersize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
	//	loader->staging_buffer);


	return loader;
}
