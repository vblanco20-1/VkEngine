#pragma  once

#include <pcheader.h>
#include "rawbuffer.h"
struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;
	glm::vec3 normal;

	static vk::VertexInputBindingDescription getBindingDescription() {
		vk::VertexInputBindingDescription bindingDescription;
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = vk::VertexInputRate::eVertex;

		return bindingDescription;
	}

	static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions() {
		std::array<vk::VertexInputAttributeDescription, 4> attributeDescriptions;
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = vk::Format::eR32G32B32Sfloat;
		attributeDescriptions[3].offset = offsetof(Vertex, normal);

		return attributeDescriptions;
	}

	static vk::PipelineVertexInputStateCreateInfo getPipelineCreateInfo() {
		static auto bindingDescription = Vertex::getBindingDescription();
		static auto attributeDescriptions = Vertex::getAttributeDescriptions();

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
		return vertexInputInfo;
	}
};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec4 eye;
};
struct GPUSceneParams {
	glm::vec4 fog_a; //xyz color, w power
	glm::vec4 fog_b; //x min, y far, zw unused
	glm::vec4 ambient;//xyz color, w power
};
struct GPUPointLight {
	glm::vec4 pos_r; //xyz pos, w radius
	glm::vec4 col_power; //xyz col, w power
};

struct AllocatedBuffer {
	vk::Buffer buffer;
	VmaAllocation allocation;
};
struct AllocatedImage {
	vk::Image image;
	VmaAllocation allocation;
};

using EntityID = entt::entity;

struct ResourceComponent {
	uint32_t last_usage;
	std::string name;
};

struct TextureResource : public ResourceComponent {
	AllocatedImage image;
	vk::ImageView imageView;
	vk::Sampler textureSampler;
};

//cold data for texture
struct TextureResourceMetadata {
	glm::uvec2 texture_size;
	vk::Format image_format;
};

struct ObjectBounds {
	//radius of sphere in W
	glm::vec4 center_rad;
	glm::vec4 extent;
};

struct MeshResource : public ResourceComponent {
	AllocatedBuffer vertexBuffer;
	AllocatedBuffer indexBuffer;

	ObjectBounds bounds;
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

struct ShaderEffectHandle :public ResourceComponent {
	struct ShaderEffect* handle;
};

struct PipelineResource : public ResourceComponent {
	vk::Pipeline pipeline;
	struct ShaderEffect* effect;
	struct GraphicsPipelineBuilder* pipelineBuilder;
};

enum class MeshPasIndex : uint8_t {
	ShadowPass = 0,
	GBufferPass = 1,
	MainPass = 2,
	TransparencyPass = 3,
	Num = 4
};

struct DescriptorResource : public ResourceComponent {
	//std::vector<vk::DescriptorSet> descriptorSets;
	vk::DescriptorSet materialSet;
};



struct RenderMeshComponent {
	//holds the data needed to render a mesh

	std::array<EntityID, (size_t)MeshPasIndex::Num> pass_pipelines;
	std::array<EntityID, (size_t)MeshPasIndex::Num> pass_descriptors;

	//EntityID pipeline_entity;
	EntityID mesh_resource_entity;
	//EntityID descriptor_entity;

	uint32_t object_idx;
};

struct DrawUnit {
	vk::Pipeline pipeline;
	vk::DescriptorSet material_set;
	vk::Buffer vertexBuffer;
	vk::Buffer indexBuffer;
	ShaderEffect* effect;
	uint32_t index_count;
	uint32_t object_idx;
};

struct TransformComponent {
	glm::mat4 model;

	glm::vec3 location;
	glm::vec3 scale;
};

struct TextureLoadRequest {
	bool bLoaded{ false };
	EntityID loadedTexture;
	//const char* image_path;
	std::string image_path;

	std::string textureName;

};

struct ConfigParams {
	glm::vec3 CamUp;
	glm::vec3 sun_location;
	float shadow_near;
	float shadow_far;
	float shadow_sides;
	float fov;
	bool ShadowView;
	bool PlayerCam;
};

struct EngineStats {
	float frametime;
	int drawcalls;
};