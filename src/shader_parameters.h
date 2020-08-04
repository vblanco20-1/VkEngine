#pragma once
#include "vulkan_types.h"

#include "vulkan_render.h"

struct ResourceHandle {
	uint64_t handle;
};

struct BindSlot {
	uint8_t set;
	uint8_t binding;
};
template<typename HandleType>
struct BindingUnit
{
	BindSlot slot;
	HandleType handle;
};
struct TextureHandle :public ResourceHandle {};
struct BufferHandle :public ResourceHandle {};
struct AccelStructureHandle :public ResourceHandle {};

using TextureBinding = BindingUnit<TextureHandle>;
using BufferBinding = BindingUnit<BufferHandle>;
using AccelStructureBinding = BindingUnit<AccelStructureHandle>;

struct ShaderParameters {	
	std::vector<TextureBinding> textures;
	std::vector<BufferBinding> buffers;

	AccelStructureBinding rayStructure;

	void bind_image(int set, int binding, TextureBinding handle);
	
	void bind_buffer(int set, int binding, BufferBinding handle);
	
	void bind_raystructure(int set, int binding, AccelStructureHandle handle);
};