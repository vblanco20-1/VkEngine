
#pragma once

#include "vulkan_types.h"
#include "vulkan_descriptors.h"
#include "frustum_cull.h"
#include "player_camera.h"
#include "framegraph.h"

constexpr int MAX_FRAMES_IN_FLIGHT =2;
constexpr int MAX_UNIFORM_BUFFER = 5000;
constexpr int SHADOWMAP_DIM = 2048;

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char*> validationLayers = {
//	"VK_LAYER_KHRONOS_validation"//,
	"VK_LAYER_LUNARG_core_validation",
	"VK_LAYER_LUNARG_object_tracker",
	"VK_LAYER_GOOGLE_threading"//,
	
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME ,
	//VK_NV_GLSL_SHADER_EXTENSION_NAME
};

struct Camera {
	glm::vec3 eyeLoc;
	glm::vec3 eyeDir;
	Frustum camfrustum;
};

struct VulkanEngine {

	PlayerCamera playerCam;
	Camera mainCam; 
	vk::Instance instance;
	vk::DebugUtilsMessengerEXT debugMessenger;
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	vk::Queue graphicsQueue;
	vk::Queue presentQueue;
	vk::SurfaceKHR surface;

	vk::SwapchainKHR swapChain;
	std::vector<vk::Image> swapChainImages;
	vk::Format swapChainImageFormat;
	vk::Extent2D swapChainExtent;
	std::vector<VkImageView> swapChainImageViews;
	AllocatedImage depthImage;
	//vk::DeviceMemory depthImageMemory;
	vk::ImageView depthImageView;
	vk::RenderPass renderPass;

	vk::DescriptorSetLayout descriptorSetLayout;
	vk::DescriptorPool descriptorPool;
	VkDescriptorPool imgui_descriptorPool;
	std::vector<vk::DescriptorSet> test_descriptorSets;

	DescriptorMegaPool descriptorMegapool;

	vk::PipelineLayout pipelineLayout;
	vk::Pipeline gbufferPipeline;
	vk::Pipeline graphicsPipeline;
	vk::Pipeline shadowPipeline;
	AllocatedBuffer ssaoSamples;

	std::vector<vk::Framebuffer> swapChainFramebuffers;


	AllocatedBuffer vertexBuffer;
	//vk::DeviceMemory vertexBufferMemory;
	
	AllocatedBuffer indexBuffer;
	//vk::DeviceMemory indexBufferMemory;

	std::vector < AllocatedBuffer> shadowDataBuffers;
	std::vector < AllocatedBuffer> cameraDataBuffers;
	std::vector < AllocatedBuffer> object_buffers;
	std::vector < AllocatedBuffer> sceneParamBuffers;
	//std::vector<vk::DeviceMemory> uniformBuffersMemory;

	GPUSceneParams sceneParameters;

	VmaAllocator allocator;

	vk::CommandPool commandPool;
	std::vector<vk::CommandBuffer> commandBuffers;

	std::unordered_map< uint64_t, void*> ProfilerContexts;
	void* MainProfilerContext{nullptr};
	AllocatedImage test_textureImage;
	//vk::DeviceMemory textureImageMemory;
	vk::ImageView tset_textureImageView;
	vk::Sampler test_textureSampler;

	vk::PhysicalDeviceProperties deviceProperties;
	std::vector<vk::Semaphore> imageAvailableSemaphores;
	std::vector<vk::Semaphore> renderFinishedSemaphores;
	std::vector<vk::Fence> inFlightFences;
	size_t currentFrameIndex = 0;
	size_t globalFrameNumber = 0;


	std::vector<Vertex> test_vertices;
	std::vector<uint32_t> test_indices;

	ConfigParams config_parameters;
	EngineStats eng_stats;

	FrameGraph render_graph;
	struct FrameBufferAttachment {
		vk::Image image;
		VkDeviceMemory mem;
		vk::ImageView view;
	};
	struct ShadowPass {
		int32_t width, height;
		VkFramebuffer frameBuffer;
		FrameBufferAttachment depth;
		VkRenderPass renderPass;
		VkSampler depthSampler;
		VkDescriptorImageInfo descriptor;
		VmaAllocation depthImageAlloc;
	} shadowPass;

	struct GBufferPass {
		int32_t width, height;
		VkFramebuffer frameBuffer;
		FrameBufferAttachment posdepth;
		FrameBufferAttachment normal;
		VkRenderPass renderPass;
		VkSampler posdepthSampler;
		VkSampler normalSampler;
		VkDescriptorImageInfo posdepthDescriptor;
		VkDescriptorImageInfo normalDescriptor;
		VmaAllocation posdepthImageAlloc;
		VmaAllocation normalImageAlloc;

		static constexpr vk::Format posdepth_format = vk::Format::eR32G32B32A32Sfloat;
		static constexpr vk::Format normal_format = vk::Format::eR16G16B16A16Sfloat;
	} gbuffPass;


	std::vector<const char*> get_extensions();
	void cleanup_swap_chain();

	bool check_layer_support();
	void create_engine_graph();
	void init_vulkan();
	void init_vulkan_debug();
	void create_device();
	void createSwapChain();
	void createImageViews();
	void create_depth_resources();

	void create_descriptor_pool();
	void create_descriptor_sets();

	
	void* get_profiler_context(vk::CommandBuffer cmd);
	void create_gfx_pipeline();
	void create_shadow_pipeline();
	void create_ssao_pipelines();

	void create_gbuffer_pipeline();
	void create_render_pass();

	void create_framebuffers();
	void create_command_pool();

	void create_texture_image();
	void create_texture_image_view();
	void create_texture_sampler();

	void create_vertex_buffer();
	void create_index_buffer();
	void create_uniform_buffers();
	void create_command_buffers();
	void create_semaphores();
	
	void create_shadow_framebuffer();
	void create_gbuffer_framebuffer(int width, int height);

	void rebuild_pipeline_resource(PipelineResource* resource);

	bool load_model(const char* model_path, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices);

	EntityID load_mesh(const char* model_path, std::string modelName);
	EntityID load_assimp_mesh(aiMesh* mesh);
	EntityID load_texture(const char* image_path, std::string textureName,bool bIsCubemap = false);

	void load_textures_bulk(TextureLoadRequest* requests, size_t count);


	bool load_scene(const char* scene_path, glm::mat4 rootMatrix);
	EntityID create_basic_descriptor_sets(EntityID pipelineID, std::string name, std::array<EntityID,8> textureID);

	void begin_frame_command_buffer(vk::CommandBuffer buffer);

	void start_shadow_renderpass(vk::CommandBuffer buffer);

	void start_gbuffer_renderpass(vk::CommandBuffer buffer);
	void start_frame_renderpass(vk::CommandBuffer buffer, vk::Framebuffer framebuffer);
	void end_frame_command_buffer(vk::CommandBuffer buffer);

	void recreate_swapchain();

	size_t align_dynamic_descriptor(size_t initial_alignement);

	void draw_frame();
	void render_shadow_pass(const vk::CommandBuffer& cmd,int height, int width);
	void render_ssao_pass(const vk::CommandBuffer& cmd, int height, int width);
	void render_ssao_blurx(const vk::CommandBuffer& cmd, int height, int width);
	void render_ssao_blury(const vk::CommandBuffer& cmd, int height, int width);
	void RenderMainPass(const vk::CommandBuffer& cmd);
	void RenderGBufferPass(const vk::CommandBuffer& cmd);
	void update_uniform_buffer(uint32_t currentImage);

	void clear_vulkan();
	void pick_physical_device();
	uint32_t findMemoryType(uint32_t typeFilter, const vk::MemoryPropertyFlags& properties);

	void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, AllocatedBuffer& allocatedbuffer);

	void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

	void createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, 
				vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,AllocatedImage& image, bool bIsCubemap = false);
	vk::ImageView createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, bool bIsCubemap = false);

	void copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height, bool bIsCubemap = false);
	void cmd_copyBufferToImage(vk::CommandBuffer& cmd,vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height);

	void destroyImage(AllocatedImage img);
	void destroyBuffer(AllocatedBuffer buffer);

	void* mapBuffer(AllocatedBuffer buffer);
	void unmapBuffer(AllocatedBuffer buffer);

	vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);
	vk::Format findDepthFormat();

	void transitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

	void cmd_transitionImageLayout(vk::CommandBuffer& commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, bool bIsCubemap = false);

	vk::CommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(vk::CommandBuffer commandBuffer);
	vk::ShaderModule createShaderModule(const std::vector<char>& code);
	vk::ShaderModule createShaderModule(const std::vector<unsigned int>& code);
	bool isDeviceSuitable(vk::PhysicalDevice device);


	void debug_resources();

	template<typename C>
	EntityID createResource(const char* resource_name,C& resource);

	template<typename C>
	C& getResource(EntityID Id);

	template<typename C>
	C& getResource(const char* resource_name);

	template<typename C>
	bool doesResourceExist(const char* resource_name);
	entt::registry render_registry;

	std::unordered_map<std::string, EntityID> resourceMap;	

	AlignedBuffer<UniformBufferObject> StagingCPUUBOArray{0};
	EntityID blankTexture;
	EntityID blackTexture;
	EntityID testCubemap;
	EntityID bluenoiseTexture;

	std::string DisplayImage;
	//vk::Pipeline getBlitPipeline(vk::RenderPass pass);
	PipelineResource* GetBlitPipeline();

	struct GraphicsPipelineBuilder* GetOutputBlitPipeline();

	struct GraphicsPipelineBuilder* gfxPipelineBuilder;
	struct GraphicsPipelineBuilder* shadowPipelineBuilder;
	struct GraphicsPipelineBuilder* gbufferPipelineBuilder;

	struct GraphicsPipelineBuilder* ssaoPipelineBuilder;
	struct GraphicsPipelineBuilder* blurPipelineBuilder;
};



template<typename C>
inline EntityID VulkanEngine::createResource(const char* resource_name, C& resource)
{
	resource.name = std::string(resource_name);
	EntityID resource_id = render_registry.create();

	render_registry.assign<C>(resource_id, resource);

	resourceMap[resource_name] = resource_id;

	return resource_id;
	
}
template<typename C>
inline C& VulkanEngine::getResource(EntityID Id)
{
	return render_registry.get<C>(Id);
}
template<typename C>
inline C& VulkanEngine::getResource(const char* resource_name)
{
	return render_registry.get<C>(resourceMap[resource_name]);
}
template<typename C>
inline bool VulkanEngine::doesResourceExist(const char* resource_name)
{
	auto find = resourceMap.find(resource_name);
	if (find != resourceMap.end()) {
		return render_registry.valid( (*find).second ) && render_registry.has<C>((*find).second);
	}
	return false;
}
static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanValidationErrorCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);