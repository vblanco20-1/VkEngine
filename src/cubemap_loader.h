#pragma once
#include <pcheader.h>
#include "vulkan_types.h"
class VulkanEngine;
class ShaderEffect;

enum class CubemapFilterMode {
	IRRADIANCE,
	REFLECTION
};

struct CubemapLoader {

	void initialize(VulkanEngine* engine);
	TextureResource load_cubemap(const char* path, int32_t dim, CubemapFilterMode mode);


	void create_offscreen_framebuffer(int32_t dim);

	VulkanEngine* eng;
	struct CubemapOffscreenFramebuffer* offscreen;
	
	ShaderEffect* irradianceRenderEffect;
	ShaderEffect* reflectionRenderEffect;

	vk::RenderPass cubemapRenderPass;
	vk::Pipeline irradianceRenderPipeline;
	vk::Pipeline reflectionRenderPipeline;
};