// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

//#include <vk_mem_alloc.h>
//#include <glm/glm.hpp>
//#include <vector>
//
//
////we want to immediately abort when there is an error. In normal engines this would give an error message to the user, or perform a dump of state.
//using namespace std;
//#define VK_CHECK(x)                                                 \
//	do                                                              \
//	{                                                               \
//		VkResult err = x;                                           \
//		if (err)                                                    \
//		{                                                           \
//			std::cout <<"Detected Vulkan error: " << err << std::endl; \
//			abort();                                                \
//		}                                                           \
//	} while (0)
//
//struct AllocatedBuffer {
//	VkBuffer _buffer;
//	VmaAllocation _allocation;
//};
//
//struct AllocatedImage {
//	VkImage _image;
//	VmaAllocation _allocation;
//};
//
//struct Texture {
//	AllocatedImage _image;
//	VkImageView _imageView;
//	VkSampler _sampler;
//};
//
//struct VertexInputDescription {
//	std::vector<VkVertexInputBindingDescription> bindings;
//	std::vector<VkVertexInputAttributeDescription> attributes;
//
//	VkPipelineVertexInputStateCreateFlags flags = 0;
//};
//
//struct Vertex {
//
//	glm::vec3 position;
//	glm::vec3 normal;
//	glm::vec2 uv;
//
//	static VertexInputDescription getVertexInputState() {
//		VertexInputDescription description;
//
//		VkVertexInputBindingDescription mainBinding = {};
//		mainBinding.binding = 0;
//		mainBinding.stride = sizeof(Vertex);
//		mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
//
//		description.bindings.push_back(mainBinding);
//
//		VkVertexInputAttributeDescription positionAttribute = {};
//		positionAttribute.binding = 0;
//		positionAttribute.location = 0;
//		positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
//		positionAttribute.offset = offsetof(Vertex, position);
//
//		VkVertexInputAttributeDescription normalAttribute = {};
//		normalAttribute.binding = 0;
//		normalAttribute.location = 1;
//		normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
//		normalAttribute.offset = offsetof(Vertex, normal);
//
//		VkVertexInputAttributeDescription uvAttribute = {};
//		uvAttribute.binding = 0;
//		uvAttribute.location = 2;
//		uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
//		uvAttribute.offset = offsetof(Vertex, uv);
//
//		description.attributes.push_back(positionAttribute);
//		description.attributes.push_back(normalAttribute);
//		description.attributes.push_back(uvAttribute);
//
//		return description;
//	}
//};
