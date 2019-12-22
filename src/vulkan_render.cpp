#include <pcheader.h>
#include "vulkan_render.h"
#include "sdl_render.h"
#include "rawbuffer.h"

#include "termcolor.hpp"
//#include <gli/gli.hpp>
#include <vk_format.h>
//#include <tiny_obj_loader.h>
#include <chrono>

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"

#include "shader_processor.h"

#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#ifndef ASSET_PATH
	//#define ASSET_PATH errorpath
    #define ASSET_PATH "K:/Programming/vkEngine/assets/"
#endif

#define MAKE_ASSET_PATH(path) ASSET_PATH ## path

#undef max()
#undef min()

static glm::mat4 mat_identity = glm::mat4( 1.0f,0.0f,0.0f,0.0f,
0.0f,1.0f,0.0f,0.0f,
0.0f,0.0f,1.0f,0.0f,
0.0f,0.0f,0.0f,1.0f );
static std::vector<char> readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("failed to open file!");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}



void VulkanEngine::init_vulkan()
 {
	vk::ApplicationInfo appInfo{ "CrapEngine",VK_MAKE_VERSION(0,0,0),"CrapEngine",VK_MAKE_VERSION(0,0,0),VK_API_VERSION_1_1 };

	vk::InstanceCreateInfo createInfo;

	createInfo.pApplicationInfo = &appInfo;
	
	config_parameters.CamUp = glm::vec3(0, 0, 1);
	config_parameters.fov = 90.f;
	eng_stats.frametime = 0.1;

	//-- EXTENSIONS

	
	std::vector<const char*> extensionNames = get_extensions();
	createInfo.enabledExtensionCount = (uint32_t) extensionNames.size();
	createInfo.ppEnabledExtensionNames = extensionNames.data();

	//--- LAYERS

	if (enableValidationLayers && !check_layer_support()) {
		throw std::runtime_error("validation layers requested, but not available!");
	}

	if (enableValidationLayers) {
		createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}
	

	instance = vk::createInstance(createInfo);
	if (enableValidationLayers)
	{
		init_vulkan_debug();
	}

	VkSurfaceKHR vksurface;
	if (!SDL_Vulkan_CreateSurface(sdl_get_window(), instance, &vksurface)) {
		throw std::runtime_error("Failed to create surface");
		// failed to create a surface!
	}
	surface = vksurface;

	load_model(MAKE_ASSET_PATH("models/gd-robot.obj"), test_vertices, test_indices);

	pick_physical_device();

	create_device();
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = physicalDevice;
	allocatorInfo.device = device;
	
	vmaCreateAllocator(&allocatorInfo, &allocator);

	createSwapChain();

	createImageViews();

	create_render_pass();

	create_descriptor_set_layout();

	create_descriptor_pool();

	create_command_pool();

	create_gfx_pipeline();

	create_depth_resources();

	create_framebuffers();

	create_semaphores();

	

	create_depth_resources();

	create_texture_image();
	create_texture_image_view();
	create_texture_sampler();


	//create_vertex_buffer();
	//
	//create_index_buffer();
	//
	create_uniform_buffers();
	//
	//
	//
	//
	create_descriptor_sets();
	blankTexture = load_texture(MAKE_ASSET_PATH("sprites/blank.png"), "blank");
	create_command_buffers();

	
	//load_scene("E:/Gamedev/tps-demo/level/geometry/demolevel.blend");
	load_scene(MAKE_ASSET_PATH("models/Bistro_v4/Bistro_Interior.fbx"));
	//load_scene(MAKE_ASSET_PATH("models/Bistro_v4/Bistro_Interior.fbx"));
	//load_scene(MAKE_ASSET_PATH("models/Bistro_v4/Bistro_Interior.fbx"));
	//load_scene(MAKE_ASSET_PATH("models/Bistro_v4/Bistro_Interior.fbx"));
	//load_scene(MAKE_ASSET_PATH("models/Bistro_v4/Bistro_Interior.fbx"));
	//load_scene(MAKE_ASSET_PATH("models/Bistro_v4/Bistro_Exterior.fbx"));
	//load_scene(MAKE_ASSET_PATH("models/SunTemple.fbx"));

	ImGui::CreateContext();

	ImGui_ImplSDL2_InitForVulkan(sdl_get_window());
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = instance;
	init_info.PhysicalDevice = physicalDevice;
	init_info.Device = device;
	//init_info.QueueFamily = ;
	init_info.Queue = this->graphicsQueue;
	//init_info.PipelineCache = ;
	init_info.DescriptorPool = imgui_descriptorPool;
	//init_info.Allocator = g_Allocator;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	//init_info.CheckVkResultFn = check_vk_result;
	ImGui_ImplVulkan_Init(&init_info, renderPass);


	VkCommandBuffer command_buffer = beginSingleTimeCommands();

	ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
	

	endSingleTimeCommands(command_buffer);


	ImGui_ImplVulkan_DestroyFontUploadObjects();

	

#if 0
	PipelineResource pipeline;
	pipeline.pipeline = graphicsPipeline;

	auto pipeline_id = createResource("pipeline_basiclit", pipeline);

	auto mesh_id = load_mesh(MAKE_ASSET_PATH("models/monkey.obj"), "mesh_chalet");
	auto texture_id = load_texture(MAKE_ASSET_PATH("models/chalet.jpg"), "tex_chalet_diffuse");

	auto descriptor_id = create_basic_descriptor_sets(pipeline_id, texture_id);//createResource("descriptor_chalet", descriptor);

	auto id = render_registry.create();
	
	RenderMeshComponent renderable;
	renderable.descriptor_entity = descriptor_id;
	renderable.mesh_resource_entity = mesh_id;
	renderable.pipeline_entity = pipeline_id;

	render_registry.assign<RenderMeshComponent>(id, renderable);

	auto model_mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	TransformComponent transform;
	transform.model = model_mat;
	transform.scale = glm::vec3(0.5f);
	transform.location = glm::vec3(0.0f);

	render_registry.assign<TransformComponent>(id, transform);

	for (int x = -30; x <= 30; x++) {
		for (int y = -30; y <= 30; y++) {

			auto new_mesh = render_registry.create();

			RenderMeshComponent renderable;
			renderable.descriptor_entity = descriptor_id;
			renderable.mesh_resource_entity = mesh_id;
			renderable.pipeline_entity = pipeline_id;
			
			TransformComponent transform;
			//transform.model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.0f));
			transform.scale = glm::vec3(0.2f);
			transform.location = glm::vec3(float(x), float(y), 0.0f);

			render_registry.assign<TransformComponent>(new_mesh, transform);
			render_registry.assign<RenderMeshComponent>(new_mesh, renderable);
		}
	}
#endif
}




void VulkanEngine::createImageViews()
{
	swapChainImageViews.resize(swapChainImages.size());

	for (size_t i = 0; i < swapChainImages.size(); i++) {		

		swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, vk::ImageAspectFlagBits::eColor);
	}

}

void VulkanEngine::create_depth_resources()
{
	auto depthFormat = findDepthFormat();

	createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, vk::ImageTiling::eOptimal, 
		vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage);


	depthImageView = createImageView(depthImage.image, depthFormat, vk::ImageAspectFlagBits::eDepth);

	transitionImageLayout(depthImage.image, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

void VulkanEngine::create_descriptor_set_layout()
{
	std::vector<vk::DescriptorSetLayoutBinding> bindings;

	vk::DescriptorSetLayoutBinding uboLayoutBinding;
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;
	uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

	bindings.push_back(uboLayoutBinding);

	vk::DescriptorSetLayoutBinding globalUboLayoutBinding;
	globalUboLayoutBinding.binding = 1;
	globalUboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
	globalUboLayoutBinding.descriptorCount = 1;
	globalUboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;
	globalUboLayoutBinding.pImmutableSamplers = nullptr; // Optional

	bindings.push_back(globalUboLayoutBinding);

	vk::DescriptorSetLayoutBinding globalObjectsLayoutBinding;
	globalObjectsLayoutBinding.binding = 2;
	globalObjectsLayoutBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
	globalObjectsLayoutBinding.descriptorCount = 1;
	globalObjectsLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;
	globalObjectsLayoutBinding.pImmutableSamplers = nullptr; // Optional
	bindings.push_back(globalObjectsLayoutBinding);

	for (int i = 0; i < 8; i++) {
		vk::DescriptorSetLayoutBinding samplerLayoutBinding;
		samplerLayoutBinding.binding = 6 + i;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		bindings.push_back(samplerLayoutBinding);
	}
	
	vk::DescriptorSetLayoutCreateInfo layoutInfo;
	layoutInfo.bindingCount = bindings.size();
	layoutInfo.pBindings = bindings.data();

	descriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
}

void VulkanEngine::create_descriptor_pool()
{
	std::array<vk::DescriptorPoolSize, 3> poolSizes = {};
	poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
	poolSizes[0].descriptorCount = 10000;//static_cast<uint32_t>(swapChainImages.size());
	poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;
	poolSizes[1].descriptorCount = 10000;//static_cast<uint32_t>(swapChainImages.size());
	poolSizes[2].type = vk::DescriptorType::eUniformBuffer;
	poolSizes[2].descriptorCount = static_cast<uint32_t>(swapChainImages.size());
	poolSizes[2].type = vk::DescriptorType::eStorageBuffer;
	poolSizes[2].descriptorCount = static_cast<uint32_t>(swapChainImages.size());

	vk::DescriptorPoolCreateInfo poolInfo;	
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 10000; //static_cast<uint32_t>(swapChainImages.size());

	descriptorPool = device.createDescriptorPool(poolInfo);

	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};
	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
	pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;
	vkCreateDescriptorPool(device, &pool_info, nullptr, &imgui_descriptorPool);
}

void VulkanEngine::create_descriptor_sets()
{
	std::vector<vk::DescriptorSetLayout> layouts(swapChainImages.size(), descriptorSetLayout);
	vk::DescriptorSetAllocateInfo allocInfo ;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
	allocInfo.pSetLayouts = layouts.data();

	test_descriptorSets = device.allocateDescriptorSets(allocInfo);
	

	for (size_t i = 0; i < swapChainImages.size(); i++) {
		vk::DescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = test_uniformBuffers[i].buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		vk::DescriptorBufferInfo bigbufferInfo;
		bigbufferInfo.buffer = object_buffers[i].buffer;
		bigbufferInfo.offset = 0;
		bigbufferInfo.range = sizeof(glm::mat4)*10000;

		vk::DescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		imageInfo.imageView = tset_textureImageView;
		imageInfo.sampler = test_textureSampler;

		std::array<vk::WriteDescriptorSet, 4> descriptorWrites = {};
		descriptorWrites[0].dstSet = test_descriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;//Dynamic;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;
		descriptorWrites[0].pImageInfo = nullptr; // Optional
		descriptorWrites[0].pTexelBufferView = nullptr; // Optional		

		descriptorWrites[2].dstSet = test_descriptorSets[i];
		descriptorWrites[2].dstBinding = 1;
		descriptorWrites[2].dstArrayElement = 0;
		descriptorWrites[2].descriptorType = vk::DescriptorType::eUniformBuffer;
		descriptorWrites[2].descriptorCount = 1;
		descriptorWrites[2].pBufferInfo = &bufferInfo;
		descriptorWrites[2].pImageInfo = nullptr; // Optional
		descriptorWrites[2].pTexelBufferView = nullptr; // Optional		

		descriptorWrites[1].dstSet = test_descriptorSets[i];
		descriptorWrites[1].dstBinding = 6;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pBufferInfo = nullptr;
		descriptorWrites[1].pImageInfo = &imageInfo; // Optional
		descriptorWrites[1].pTexelBufferView = nullptr; // Optional		

		descriptorWrites[3].dstSet = test_descriptorSets[i];
		descriptorWrites[3].dstBinding = 2;
		descriptorWrites[3].dstArrayElement = 0;
		descriptorWrites[3].descriptorType = vk::DescriptorType::eStorageBuffer;
		descriptorWrites[3].descriptorCount = 1;
		descriptorWrites[3].pBufferInfo = &bigbufferInfo;
		descriptorWrites[3].pImageInfo = nullptr; // Optional
		descriptorWrites[3].pTexelBufferView = nullptr; // Optional		

		device.updateDescriptorSets(descriptorWrites, 0);
	}
}


EntityID VulkanEngine::create_basic_descriptor_sets(EntityID pipelineID, std::array<EntityID,8> textureID)
{
	
	const PipelineResource &pipeline = render_registry.get<PipelineResource>(pipelineID);


	DescriptorResource descriptors;
	
	//create buffers to hold the matrices
	vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

	std::vector<vk::DescriptorSetLayout> layouts(swapChainImages.size(), descriptorSetLayout);
	vk::DescriptorSetAllocateInfo allocInfo;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
	allocInfo.pSetLayouts = layouts.data();

	descriptors.descriptorSets = device.allocateDescriptorSets(allocInfo);

	static std::vector<vk::WriteDescriptorSet> descriptorWrites;
	
	static std::vector<vk::DescriptorBufferInfo> bufferInfos;

	static std::vector<vk::DescriptorImageInfo> imageInfos;

	descriptorWrites.clear();
	bufferInfos.clear();
	imageInfos.clear();

	imageInfos.reserve(8);

	int siz;
	std::string name = render_registry.get<TextureResource>(textureID[0]).name;
	for (size_t i = 0; i < swapChainImages.size(); i++) {

		descriptorWrites.clear();
		bufferInfos.clear();
		imageInfos.clear();

		vk::DescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = transformUniformBuffers[i].buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		vk::DescriptorBufferInfo glbbufferInfo;
		glbbufferInfo.buffer = test_uniformBuffers[i].buffer;
		glbbufferInfo.offset = 0;
		glbbufferInfo.range = sizeof(UniformBufferObject);

		vk::DescriptorBufferInfo bigbufferInfo;
		bigbufferInfo.buffer = object_buffers[i].buffer;
		bigbufferInfo.offset = 0;
		bigbufferInfo.range = sizeof(glm::mat4) * 10000;		

		//std::array<vk::WriteDescriptorSet, 4> descriptorWrites = {};
		descriptorWrites.push_back(vk::WriteDescriptorSet{});
		descriptorWrites[0].dstSet = descriptors.descriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;//Dynamic;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;
		descriptorWrites[0].pImageInfo = nullptr; // Optional
		descriptorWrites[0].pTexelBufferView = nullptr; // Optional		


		for (int j = 0; j < 8; j++) {

			const TextureResource& texture = render_registry.get<TextureResource>(textureID[j]);

			vk::DescriptorImageInfo imageInfo = {};
			imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			imageInfo.imageView = texture.imageView;
			imageInfo.sampler = texture.textureSampler;

			imageInfos.push_back(imageInfo);
		}
		for (int j = 0; j < 8; j++) {
			siz = descriptorWrites.size();
			descriptorWrites.push_back(vk::WriteDescriptorSet{});
			descriptorWrites[siz].dstSet = descriptors.descriptorSets[i];
			descriptorWrites[siz].dstBinding = 6 + j;
			descriptorWrites[siz].dstArrayElement = 0;
			descriptorWrites[siz].descriptorType = vk::DescriptorType::eCombinedImageSampler;
			descriptorWrites[siz].descriptorCount = 1;
			descriptorWrites[siz].pBufferInfo = nullptr;
			descriptorWrites[siz].pImageInfo = &imageInfos[j]; // Optional
			descriptorWrites[siz].pTexelBufferView = nullptr; // Optional	
		}



		siz = descriptorWrites.size();
		descriptorWrites.push_back(vk::WriteDescriptorSet{});
		descriptorWrites[siz].dstSet = descriptors.descriptorSets[i];
		descriptorWrites[siz].dstBinding = 1;
		descriptorWrites[siz].dstArrayElement = 0;
		descriptorWrites[siz].descriptorType = vk::DescriptorType::eUniformBuffer;
		descriptorWrites[siz].descriptorCount = 1;
		descriptorWrites[siz].pBufferInfo = &glbbufferInfo;
		descriptorWrites[siz].pImageInfo = nullptr; // Optional
		descriptorWrites[siz].pTexelBufferView = nullptr; // Optional		

		siz = descriptorWrites.size();
		descriptorWrites.push_back(vk::WriteDescriptorSet{});
		descriptorWrites[siz].dstSet = descriptors.descriptorSets[i];
		descriptorWrites[siz].dstBinding = 2;
		descriptorWrites[siz].dstArrayElement = 0;
		descriptorWrites[siz].descriptorType = vk::DescriptorType::eStorageBuffer;
		descriptorWrites[siz].descriptorCount = 1;
		descriptorWrites[siz].pBufferInfo = &bigbufferInfo;
		descriptorWrites[siz].pImageInfo = nullptr; // Optional
		descriptorWrites[siz].pTexelBufferView = nullptr; // Optional		

		device.updateDescriptorSets(descriptorWrites, 0);
	}

	std::string descriptorName = pipeline.name + name;
	return createResource(descriptorName.c_str(), descriptors);
}

vk::ShaderModule VulkanEngine::createShaderModule(const std::vector<char>& code) {
	vk::ShaderModuleCreateInfo createInfo = {};	
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	return device.createShaderModule(createInfo);
}

vk::ShaderModule VulkanEngine::createShaderModule(const std::vector<unsigned int>& code) {
	vk::ShaderModuleCreateInfo createInfo = {};
	createInfo.codeSize = code.size()*4;
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	return device.createShaderModule(createInfo);
}

struct ShaderEffectHandle:public ResourceComponent {
	ShaderEffect* handle;
};

void VulkanEngine::create_gfx_pipeline()
{

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	ShaderEffect* pipelineEffect;
	if (!doesResourceExist<ShaderEffectHandle>("basiclit") ){

		ShaderEffectHandle newShader;
		newShader.handle = new ShaderEffect();

		newShader.handle->add_shader_from_file(MAKE_ASSET_PATH("shaders/basiclit.vert"));
		newShader.handle->add_shader_from_file(MAKE_ASSET_PATH("shaders/basiclit.frag"));

		newShader.handle->build_effect(device);

		pipelineEffect = newShader.handle;

		createResource<ShaderEffectHandle>("basiclit", newShader);
	}
	else
	{
		pipelineEffect = getResource<ShaderEffectHandle>("basiclit").handle;
		
	}

	shaderStages = pipelineEffect->get_stage_infos();
	
	VkPipelineLayout vklayout = pipelineEffect->build_pipeline_layout(device);

	vk::PipelineLayout newLayout = vk::PipelineLayout(vklayout);


	vk::PipelineVertexInputStateCreateInfo vertexInputInfo = Vertex::getPipelineCreateInfo();

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly;	
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	vk::Viewport viewport;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainExtent.width;
	viewport.height = (float)swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	vk::Rect2D scissor;
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;

	vk::PipelineViewportStateCreateInfo viewportState;	
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;


	vk::PipelineDepthStencilStateCreateInfo depthStencil;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = vk::CompareOp::eLess;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {}; // Optional
	depthStencil.back = {}; // Optional

	vk::PipelineRasterizationStateCreateInfo rasterizer;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = vk::PolygonMode::eFill;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = vk::CullModeFlagBits::eBack;
	rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	vk::PipelineMultisampleStateCreateInfo multisampling;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional


	vk::PipelineColorBlendAttachmentState colorBlendAttachment;
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eOne; // Optional
	colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eZero; // Optional
	colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd; // Optional
	colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;// Optional
	colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero; // Optional
	colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd; // Optional

	vk::PipelineColorBlendStateCreateInfo colorBlending ;

	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = vk::LogicOp::eCopy; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	vk::DynamicState dynamicStates[] = {
	vk::DynamicState::eViewport,
	vk::DynamicState::eLineWidth
	};

	vk::PipelineDynamicStateCreateInfo dynamicState = {};	
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	vk::GraphicsPipelineCreateInfo pipelineInfo;
	pipelineInfo.stageCount = shaderStages.size();
	pipelineInfo.pStages = reinterpret_cast<vk::PipelineShaderStageCreateInfo*>( shaderStages.data());
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr; // Optional
	pipelineInfo.layout = newLayout;//pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;

	pipelineLayout = newLayout;

	graphicsPipeline = device.createGraphicsPipelines(nullptr,pipelineInfo)[0];
}

void VulkanEngine::create_render_pass()
{
	vk::AttachmentDescription colorAttachment;
	colorAttachment.format = swapChainImageFormat;
	colorAttachment.samples = vk::SampleCountFlagBits::e1;
	colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;

	colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
	colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;


	vk::AttachmentDescription depthAttachment;
	depthAttachment.format = findDepthFormat();
	depthAttachment.samples = vk::SampleCountFlagBits::e1;
	depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
	depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
	depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	vk::AttachmentReference colorAttachmentRef;
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::AttachmentReference depthAttachmentRef ;
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	vk::SubpassDescription subpass;
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	vk::SubpassDependency dependency;
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;

	dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.srcAccessMask = vk::AccessFlags{};

	dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

	std::array<vk::AttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
	vk::RenderPassCreateInfo renderPassInfo;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());;
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	renderPass = device.createRenderPass(renderPassInfo);
}

void VulkanEngine::create_framebuffers()
{
	swapChainFramebuffers.resize(swapChainImageViews.size());

	for (size_t i = 0; i < swapChainImageViews.size(); i++) {
		std::array<vk::ImageView, 2> attachments = {
			swapChainImageViews[i],
			depthImageView
		};

		vk::FramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.layers = 1;

		swapChainFramebuffers[i] = device.createFramebuffer(framebufferInfo);
		
	}
}


void* VulkanEngine::mapBuffer(AllocatedBuffer buffer) {
	void* data;
	vmaMapMemory(allocator, buffer.allocation, &data);
	return data;
}
;
void VulkanEngine::unmapBuffer(AllocatedBuffer buffer) {
	vmaUnmapMemory(allocator, buffer.allocation);
};

void VulkanEngine::create_texture_image()
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(MAKE_ASSET_PATH("models/playerrobot_diffusion.png"), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	vk::DeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	//vk::Buffer stagingBuffer;
	//vk::DeviceMemory stagingBufferMemory;
	AllocatedBuffer stagingBuffer;
	createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer);

		
	void* data;
	vmaMapMemory(allocator, stagingBuffer.allocation, &data);//device.mapMemory(stagingBufferMemory, 0, bufferSize);

	memcpy(data, pixels, static_cast<size_t>(imageSize));

	vmaUnmapMemory(allocator, stagingBuffer.allocation);

	stbi_image_free(pixels);

	
	//AllocatedImage textureImage;
	createImage(texWidth, texHeight, vk::Format::eR8G8B8A8Unorm,
		vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, 
		vk::MemoryPropertyFlagBits::eDeviceLocal, test_textureImage);

	transitionImageLayout(test_textureImage.image, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
	
	copyBufferToImage(stagingBuffer.buffer, test_textureImage.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

	transitionImageLayout(test_textureImage.image, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

	vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);

	
}



uint32_t VulkanEngine::findMemoryType(uint32_t typeFilter,const vk::MemoryPropertyFlags& properties) {

	vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");

}

void VulkanEngine::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, AllocatedBuffer& allocatedbuffer)
{
	vk::BufferCreateInfo bufferInfo;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = vk::SharingMode::eExclusive;

	VkBufferCreateInfo vkbinfo = bufferInfo;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = VMA_MEMORY_USAGE_UNKNOWN;
	vmaallocInfo.requiredFlags = VkMemoryPropertyFlags(properties);
	VkBuffer vkbuffer;
	VmaAllocation allocation;
	vmaCreateBuffer(allocator, &vkbinfo, &vmaallocInfo, &vkbuffer, &allocation, nullptr);

	
	allocatedbuffer.buffer = vkbuffer;
	allocatedbuffer.allocation = allocation;
}

void VulkanEngine::copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size)
{
	vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

	vk::BufferCopy copyRegion;
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = size;
	commandBuffer.copyBuffer(srcBuffer, dstBuffer, 1, &copyRegion);

	endSingleTimeCommands(commandBuffer);

}
vk::CommandBuffer VulkanEngine::beginSingleTimeCommands() {
	vk::CommandBufferAllocateInfo allocInfo;
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	vk::CommandBuffer commandBuffer = device.allocateCommandBuffers(allocInfo)[0];

	commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
	return commandBuffer;
}
void VulkanEngine::endSingleTimeCommands(vk::CommandBuffer commandBuffer) {
	commandBuffer.end();

	vk::SubmitInfo submitInfo;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	graphicsQueue.submit(submitInfo, nullptr);
	graphicsQueue.waitIdle();
	device.freeCommandBuffers(commandPool, { commandBuffer });
}


void  VulkanEngine::create_vertex_buffer()
{
	vk::DeviceSize bufferSize = sizeof(test_vertices[0]) * test_vertices.size();

	//vk::DeviceMemory stagingBufferMemory;
	AllocatedBuffer staging_allocation;
	createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, staging_allocation);


	void* data;
	vmaMapMemory(allocator, staging_allocation.allocation, &data);//device.mapMemory(stagingBufferMemory, 0, bufferSize);

	memcpy(data, test_vertices.data(), (size_t)bufferSize);

	vmaUnmapMemory(allocator, staging_allocation.allocation);
	
	//AllocatedBuffer vertex_buffer;
	createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer);//vertexBuffer, vertexBufferMemory);
	//vertexBuffer = vertex_buffer.buffer;
	copyBuffer(staging_allocation.buffer, vertexBuffer.buffer, bufferSize);

	vmaDestroyBuffer(allocator, staging_allocation.buffer,staging_allocation.allocation);
}

void VulkanEngine::create_index_buffer()
{
	vk::DeviceSize bufferSize = sizeof(test_indices[0]) * test_indices.size();

	AllocatedBuffer staging_allocation;
	createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, staging_allocation);


	void* data;
	vmaMapMemory(allocator, staging_allocation.allocation, &data);//device.mapMemory(stagingBufferMemory, 0, bufferSize);

	memcpy(data, test_indices.data(), (size_t)bufferSize);

	vmaUnmapMemory(allocator, staging_allocation.allocation);

	//AllocatedBuffer index_buffer;
	createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer);

	copyBuffer(staging_allocation.buffer, indexBuffer.buffer, bufferSize);


	vmaDestroyBuffer(allocator, staging_allocation.buffer, staging_allocation.allocation);
}

void VulkanEngine::create_uniform_buffers()
{
	vk::DeviceSize bufferSize = align_dynamic_descriptor(sizeof(UniformBufferObject)) * 4;

	test_uniformBuffers.resize(swapChainImages.size());
	object_buffers.resize(swapChainImages.size());
	//uniformBuffersMemory.resize(swapChainImages.size());

	for (size_t i = 0; i < swapChainImages.size(); i++) {
		createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, test_uniformBuffers[i]);
	
		createBuffer(sizeof(glm::mat4) * 10000, vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, object_buffers[i]);
	}



	// Calculate required alignment based on minimum device offset alignment
	//size_t minUboAlignment = vulkanDevice->properties.limits.minUniformBufferOffsetAlignment;
	//dynamicAlignment = sizeof(glm::mat4);
	//if (minUboAlignment > 0) {
	//	dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
	//}


	transformUniformBuffers.resize(swapChainImages.size());

	bufferSize = StagingCPUUBOArray.get_offset(MAX_UNIFORM_BUFFER);// sizeof(UniformBufferObject) * MAX_UNIFORM_BUFFER;
	for (size_t i = 0; i < swapChainImages.size(); i++) {
		createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, transformUniformBuffers[i]);
	}

}

void VulkanEngine::create_command_buffers()
{
	vk::CommandBufferAllocateInfo allocInfo;	
	allocInfo.commandPool = commandPool;
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandBufferCount = swapChainFramebuffers.size();


	commandBuffers = device.allocateCommandBuffers(allocInfo);
}

void VulkanEngine::start_frame_command_buffer(vk::CommandBuffer buffer, vk::Framebuffer framebuffer)
{
	vk::CommandBufferBeginInfo beginInfo;
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;
	beginInfo.pInheritanceInfo = nullptr; // Optional

	vk::CommandBuffer& cmd = buffer;//commandBuffers[i];

	cmd.reset(vk::CommandBufferResetFlags{});
	cmd.begin(beginInfo);

	vk::RenderPassBeginInfo renderPassInfo;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = framebuffer;//swapChainFramebuffers[i];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapChainExtent;



	std::array<vk::ClearValue, 2> clearValues = {};
	clearValues[0].color = vk::ClearColorValue{ std::array<float,4> { 0.0f, 0.0f, 0.0f, 1.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = clearValues.size();
	renderPassInfo.pClearValues = clearValues.data();

	cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
}

void VulkanEngine::end_frame_command_buffer(vk::CommandBuffer cmd)
{

	cmd.endRenderPass();

	cmd.end();
}

void VulkanEngine::draw_frame()
{
	

	device.waitForFences( 1, &inFlightFences[currentFrameIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());

	
	vk::ResultValue<uint32_t> imageResult = device.acquireNextImageKHR(swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphores[currentFrameIndex], nullptr);

	if (imageResult.result == vk::Result::eErrorOutOfDateKHR) {
		recreate_swapchain();
		return;
	}

	uint32_t imageIndex = imageResult.value;

	vk::SubmitInfo submitInfo;
	

	vk::Semaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrameIndex] };
	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	

	vk::Semaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrameIndex] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;


	device.resetFences(1, &inFlightFences[currentFrameIndex]);

	update_uniform_buffer(imageIndex);

	vk::CommandBufferAllocateInfo allocInfo;
	allocInfo.commandPool = commandPool;
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandBufferCount = 1;

	//auto bffs = device.allocateCommandBuffers(allocInfo);
//

	eng_stats.drawcalls = 0;

	vk::CommandBuffer& cmd = commandBuffers[currentFrameIndex];//bffs[0];

	start_frame_command_buffer(cmd, swapChainFramebuffers[currentFrameIndex]);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

	//vk::Buffer vertexBuffers[] = { vertexBuffer.buffer };
	//vk::DeviceSize offsets[] = { 0 };
	//cmd.bindVertexBuffers(0, 1, vertexBuffers, offsets);
	//
	//cmd.bindIndexBuffer(indexBuffer.buffer, 0, vk::IndexType::eUint32);
	//uint32_t dynamicOffset = 0;//StagingCPUUBOArray.get_offset(3); //3 * sizeof(UniformBufferObject);
	//
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &test_descriptorSets[(currentFrameIndex +1)%3], 0, nullptr);//1, &dynamicOffset);//0, nullptr);
	////vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
	//eng_stats.drawcalls++;
	//cmd.drawIndexed(static_cast<uint32_t>(test_indices.size()), 1, 0, 0, 0);
	vk::Pipeline last_pipeline = graphicsPipeline;
	bool first_render = true;

	render_registry.view<RenderMeshComponent>().each([&](RenderMeshComponent& renderable) {
		
		const MeshResource& mesh = render_registry.get<MeshResource>(renderable.mesh_resource_entity);
		const PipelineResource& pipeline = render_registry.get<PipelineResource>(renderable.pipeline_entity);
		const DescriptorResource& descriptor = render_registry.get<DescriptorResource>(renderable.descriptor_entity);
	
		bool bShouldBindPipeline = first_render ? true : pipeline.pipeline == last_pipeline;
		
		if (bShouldBindPipeline) {
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.pipeline);
			last_pipeline = graphicsPipeline;//pipeline.pipeline;
		}
		
	
		vk::Buffer meshbuffers[] = { mesh.vertexBuffer.buffer };
		vk::DeviceSize offsets[] = { 0 };
		cmd.bindVertexBuffers(0, 1, meshbuffers, offsets);
	
		cmd.bindIndexBuffer(mesh.indexBuffer.buffer, 0, vk::IndexType::eUint32);
		uint32_t dynamicOffset = renderable.ubo_descriptor_offset;// * sizeof(UniformBufferObject);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &descriptor.descriptorSets[currentFrameIndex], 0, nullptr);// 1, &dynamicOffset);
	
		cmd.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(int), &renderable.object_idx);
		cmd.drawIndexed(static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);

		eng_stats.drawcalls++;
	});
	
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	end_frame_command_buffer(cmd);
	


	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;// &commandBuffers[imageIndex];
	graphicsQueue.submit(1, &submitInfo, inFlightFences[currentFrameIndex]);


	vk::PresentInfoKHR presentInfo = {};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	vk::SwapchainKHR swapChains[] = { swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;

	presentInfo.pResults = nullptr; // Optional

	presentQueue.presentKHR(presentInfo);

	currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
	globalFrameNumber++;


}
//std::vector<UniformBufferObject> StagingCPUUBOArray;
void VulkanEngine::update_uniform_buffer(uint32_t currentImage)
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	static auto lastTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
	float delta = std::chrono::duration<float, std::chrono::milliseconds::period>(currentTime - lastTime).count();
	lastTime = currentTime;
		if ((globalFrameNumber % 10) == 0) {
			//std::cout << "[frametime]" << delta << std::endl;
		}

		eng_stats.frametime *= 0.9f;//delta;
		eng_stats.frametime += delta / 10.f;
		float camUp = sin(time / 10.0f) * 1000.f;

		auto eye = glm::vec3(camUp, 600, 200);

	UniformBufferObject ubo = {};
	ubo.model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.0f)) * glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(eye, glm::vec3(0.0f, 400.0f, 0.0f), config_parameters.CamUp);
	ubo.proj = glm::perspective(glm::radians(config_parameters.fov), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10000.0f);
	ubo.eye = glm::vec4(eye,0.0f);
	//invert projection matrix couse glm is inverted y compared to vulkan
	ubo.proj[1][1] *= -1;
		
	//void* data = mapBuffer(test_uniformBuffers[currentImage]);
	//auto alignUbo = align_dynamic_descriptor(sizeof(ubo));
	//char* dt = (char*)data;// + alignUbo; ;
	//
	//// +sizeof(ubo) * 3;
	//memcpy(dt, &ubo, alignUbo );
	//
	//unmapBuffer(test_uniformBuffers[currentImage]);

	//render_registry.view<TransformComponent>().each([&](TransformComponent& transform) {
	//	auto trmat =  glm::translate(mat_identity, transform.location);
	//	auto rotmat = glm::rotate(mat_identity, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	//	auto mat1 =glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
	//		0.0f, 1.0f, 0.0f, 0.0f,
	//		0.0f, 0.0f, 1.0f, 0.0f,
	//		0.0f, 0.0f, 0.0f, 1.0f);
	//	transform.model = mat1 *trmat * rotmat;// * glm::scale(glm::mat4(1.0f), transform.scale);
	//
	//});

	//std::cout << sizeof(ubo);
	//StagingCPUUBOArray.clear();
	//static int renderid = 0;
	//renderid++;
	//if (renderid < 5) {
		int copyidx = 0;
		std::vector<glm::mat4> object_matrices;

		object_matrices.resize(render_registry.capacity<RenderMeshComponent>());
		
		render_registry.view<RenderMeshComponent, TransformComponent>().each([&](EntityID id, RenderMeshComponent& renderable, const TransformComponent& transform) {

			UniformBufferObject ubo2 = ubo;
			ubo2.model = transform.model;

			//StagingCPUUBOArray.push_back(ubo2);
			StagingCPUUBOArray[copyidx] = ubo2;

			renderable.ubo_descriptor_offset = StagingCPUUBOArray.get_offset(copyidx);

			object_matrices[copyidx] = ubo2.model;
			//object_matrices.push_back(transform.model);
			renderable.object_idx = copyidx;

			copyidx++;
			


			});

		if (copyidx > 0) {
			void* data2 = mapBuffer(transformUniformBuffers[currentImage]);
			memcpy(data2, StagingCPUUBOArray.get_raw(), StagingCPUUBOArray.get_offset(copyidx + 1));

			unmapBuffer(transformUniformBuffers[currentImage]);

			void* matdata = mapBuffer(object_buffers[currentImage]);
			memcpy(matdata, object_matrices.data(), object_matrices.size() * sizeof(glm::mat4));

			unmapBuffer(object_buffers[currentImage]);
		}
	//}


}






void VulkanEngine::cleanup_swap_chain()
{
	for (auto imageView : swapChainImageViews) {
		device.destroyImageView(imageView);
	}
	for (auto framebuffer : swapChainFramebuffers) {
		device.destroyFramebuffer(framebuffer);
	}

	for (size_t i = 0; i < test_uniformBuffers.size(); i++) {
		//device.freeMemory(uniformBuffers[i]);
	//	device.freeMemory(uniformBuffersMemory[i]);
		destroyBuffer(test_uniformBuffers[i]);
	}

	
	device.destroyDescriptorPool(descriptorPool);

	device.destroyImageView(depthImageView);

	destroyImage(depthImage);
	//device.destroyImage(depthImage);
	
	//device.freeMemory(depthImageMemory);

	device.freeCommandBuffers(commandPool, commandBuffers);

	device.destroyPipeline(graphicsPipeline);
	device.destroyPipelineLayout(pipelineLayout);
	device.destroyRenderPass(renderPass);
	device.destroySwapchainKHR(swapChain);

}
void VulkanEngine::destroyImage(AllocatedImage img) {
	vmaDestroyImage(allocator, img.image, img.allocation);
}
void VulkanEngine::destroyBuffer(AllocatedBuffer buffer) {
	vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

void VulkanEngine::create_texture_sampler()
{
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

	test_textureSampler = device.createSampler(samplerInfo);
}

void VulkanEngine::clear_vulkan()
{	
	device.waitIdle();

	cleanup_swap_chain();

	destroyBuffer(vertexBuffer);
	destroyBuffer(indexBuffer);
	//device.freeMemory(vertexBufferMemory);
	//device.freeMemory(indexBufferMemory);

	device.destroySampler(test_textureSampler);
	device.destroyImageView(tset_textureImageView);
	//device.freeMemory(textureImageMemory);
	destroyImage(test_textureImage);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		device.destroySemaphore(imageAvailableSemaphores[i]);
		device.destroySemaphore(renderFinishedSemaphores[i]);
		device.destroyFence(inFlightFences[i]);

	}
	
	device.destroyDescriptorSetLayout(descriptorSetLayout);
	
	device.destroy();

	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	func(instance, debugMessenger, nullptr);
	instance.destroy();	
}

void VulkanEngine::create_semaphores()
{
	vk::SemaphoreCreateInfo semaphoreInfo;
	vk::FenceCreateInfo fenceInfo;
	fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
	
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		imageAvailableSemaphores[i] = device.createSemaphore(semaphoreInfo);
		renderFinishedSemaphores[i] = device.createSemaphore(semaphoreInfo);
		inFlightFences[i] = device.createFence(fenceInfo);
	}
}

bool VulkanEngine::load_model(const char* model_path, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices)
{
	//tinyobj::attrib_t attrib;
	//std::vector<tinyobj::shape_t> shapes;
	//std::vector<tinyobj::material_t> materials;
	//std::string warn, err;
	//	
	//vertices.clear();
	//indices.clear();
	//
	//if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path /*MAKE_ASSET_PATH("models/gd-robot.obj")*/)) {
	//	throw std::runtime_error(warn + err);
	//}
	//
	////for (const auto& shape : shapes) {
	//	for (const auto& shape : shapes) {
	//		for (const auto& index : shape.mesh.indices) {
	//			Vertex vertex = {};
	//			vertex.pos = {
	//				attrib.vertices[3 * index.vertex_index + 0],
	//				attrib.vertices[3 * index.vertex_index + 1],
	//				attrib.vertices[3 * index.vertex_index + 2]
	//			};
	//
	//			vertex.texCoord = {
	//				attrib.texcoords[2 * index.texcoord_index + 0],
	//				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
	//			};
	//
	//			if (index.normal_index >= 0){
	//				vertex.normal = {
	//				attrib.normals[3 * index.normal_index + 0],
	//				attrib.normals[3 * index.normal_index + 1],
	//				attrib.normals[3 * index.normal_index + 2],
	//				};
	//			}
	//			else {
	//				vertex.normal = { 0.0,0.0,1.0 };
	//			}
	//			
	//
	//			vertex.color = { 1.0f, 1.0f, 1.0f };
	//
	//			vertices.push_back(vertex);
	//			indices.push_back(indices.size());
	//		}
	//	}
	//}
	return true;
}


EntityID VulkanEngine::load_mesh(const char* model_path, std::string modelName)
{
	MeshResource newMesh;
	
	

	load_model(model_path, newMesh.vertices, newMesh.indices);


	vk::DeviceSize vertexBufferSize = sizeof(newMesh.vertices[0]) * newMesh.vertices.size();
	vk::DeviceSize indexBufferSize = sizeof(newMesh.indices[0]) * newMesh.indices.size();
	
	AllocatedBuffer vertex_staging_allocation;
	createBuffer(vertexBufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, vertex_staging_allocation);

	AllocatedBuffer index_staging_allocation;
	createBuffer(indexBufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, index_staging_allocation);

	//copy vertex data
	void* data;
	vmaMapMemory(allocator, vertex_staging_allocation.allocation, &data);

	memcpy(data, newMesh.vertices.data(), (size_t)vertexBufferSize);

	vmaUnmapMemory(allocator, vertex_staging_allocation.allocation);

	//copy index data
	vmaMapMemory(allocator, index_staging_allocation.allocation, &data);

	memcpy(data, newMesh.indices.data(), (size_t)indexBufferSize);

	vmaUnmapMemory(allocator, index_staging_allocation.allocation);


	
	createBuffer(vertexBufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, newMesh.vertexBuffer);//vertexBuffer, vertexBufferMemory);
	
	copyBuffer(vertex_staging_allocation.buffer, newMesh.vertexBuffer.buffer, vertexBufferSize);

	vmaDestroyBuffer(allocator, vertex_staging_allocation.buffer, vertex_staging_allocation.allocation);
		
	

	//AllocatedBuffer index_buffer;
	createBuffer(indexBufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, newMesh.indexBuffer);

	copyBuffer(index_staging_allocation.buffer, newMesh.indexBuffer.buffer, indexBufferSize);

	vmaDestroyBuffer(allocator, index_staging_allocation.buffer, index_staging_allocation.allocation);

	EntityID id = createResource(modelName.c_str(), newMesh);

	return id;
}

EntityID VulkanEngine::load_assimp_mesh(aiMesh* mesh)
{
	MeshResource newMesh;
	
	newMesh.vertices.clear();
	newMesh.indices.clear();
	
	newMesh.vertices.reserve(mesh->mNumVertices);
	newMesh.indices.reserve(mesh->mNumFaces*3);
	
	for (int vtx = 0; vtx < mesh->mNumVertices; vtx++) {
		Vertex vertex = {};			
	
		vertex.pos = {
			mesh->mVertices[vtx].x,
			mesh->mVertices[vtx].y,
			mesh->mVertices[vtx].z
		};
		if (mesh->mTextureCoords[0])
		{
			vertex.texCoord = {
			mesh->mTextureCoords[0][vtx].x,
			mesh->mTextureCoords[0][vtx].y,
			};
		}
		else {
			vertex.texCoord = {
				0.5f,0.5f		
			};
		}
		
		vertex.normal = {
			mesh->mNormals[vtx].x,
			mesh->mNormals[vtx].y,
			mesh->mNormals[vtx].z
		};
		newMesh.vertices.push_back(vertex);
	}
	
	for (int face = 0; face < mesh->mNumFaces; face++){
		newMesh.indices.push_back(mesh->mFaces[face].mIndices[0]);
		newMesh.indices.push_back(mesh->mFaces[face].mIndices[1]);
		newMesh.indices.push_back(mesh->mFaces[face].mIndices[2]);
	}


	vk::DeviceSize vertexBufferSize = sizeof(newMesh.vertices[0]) * newMesh.vertices.size();
	vk::DeviceSize indexBufferSize = sizeof(newMesh.indices[0]) * newMesh.indices.size();

	AllocatedBuffer vertex_staging_allocation;
	createBuffer(vertexBufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, vertex_staging_allocation);

	AllocatedBuffer index_staging_allocation;
	createBuffer(indexBufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, index_staging_allocation);

	//copy vertex data
	void* data;
	vmaMapMemory(allocator, vertex_staging_allocation.allocation, &data);

	memcpy(data, newMesh.vertices.data(), (size_t)vertexBufferSize);

	vmaUnmapMemory(allocator, vertex_staging_allocation.allocation);

	//copy index data
	vmaMapMemory(allocator, index_staging_allocation.allocation, &data);

	memcpy(data, newMesh.indices.data(), (size_t)indexBufferSize);

	vmaUnmapMemory(allocator, index_staging_allocation.allocation);



	createBuffer(vertexBufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, newMesh.vertexBuffer);//vertexBuffer, vertexBufferMemory);

	copyBuffer(vertex_staging_allocation.buffer, newMesh.vertexBuffer.buffer, vertexBufferSize);

	vmaDestroyBuffer(allocator, vertex_staging_allocation.buffer, vertex_staging_allocation.allocation);



	//AllocatedBuffer index_buffer;
	createBuffer(indexBufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, newMesh.indexBuffer);

	copyBuffer(index_staging_allocation.buffer, newMesh.indexBuffer.buffer, indexBufferSize);

	vmaDestroyBuffer(allocator, index_staging_allocation.buffer, index_staging_allocation.allocation);

	
	EntityID id = createResource(mesh->mName.C_Str(), newMesh);

	return id;	
}



void eraseSubStr(std::string& mainStr, const std::string& toErase)
{
	// Search for the substring in string
	size_t pos = mainStr.find(toErase);

	if (pos != std::string::npos)
	{
		// If found then erase it from string
		mainStr.erase(pos, toErase.length());
	}
}


bool GrabTextureLoadRequest(aiMaterial* material, aiTextureType textype, const std::string &scenepath,TextureLoadRequest& LoadRequest) {

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
		try {


			std::string folderstring = scenepath + "/" + texture_path.string();			
			LoadRequest.bLoaded = false;
			LoadRequest.image_path = folderstring;
			LoadRequest.textureName = txpath;
			return true;			
		}
		catch (std::runtime_error& e) {
			std::cout << "error with path texture:" << texpath.C_Str() << std::endl;
			return false;
		}
	}
	return false;
};

bool GrabTextureID(aiMaterial* material, aiTextureType textype, VulkanEngine* eng, EntityID& textureID) {
	aiString texpath;
	if (material->GetTextureCount(textype))
	{
		material->GetTexture(textype, 0, &texpath);

		const char* txpath = &texpath.data[0];
		for (int i = 0; i < texpath.length; i++)
		{
			if (texpath.data[i] == '\\')
			{
				texpath.data[i] = '/';
			}
		}
		
		textureID = eng->resourceMap[std::string(txpath)];
		return true;
	}
	return false;
}


bool VulkanEngine::load_scene(const char* scene_path)
{
	auto start1 = std::chrono::system_clock::now();

	std::filesystem::path sc_path{ std::string(scene_path) };
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(scene_path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);
	
	auto end = std::chrono::system_clock::now();
	auto elapsed = end - start1;
	std::cout << "Assimp load time" <<elapsed.count() << '\n';

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
		return false;
	}

	std::vector<EntityID> textures;
	std::vector< TextureLoadRequest> textureLoadRequests;



	struct SimpleMaterial {
		std::array<EntityID, 8> textureIDs;
		
	};
	std::vector<SimpleMaterial> materials;
	for (int i = 0; i < scene->mNumMaterials; i++)
	{
		std::cout << std::endl;
		std::cout << "iterating material" << scene->mMaterials[i]->GetName().C_Str() << std::endl;
		TextureLoadRequest request;
		std::string scenepath = sc_path.parent_path().string();
		if (GrabTextureLoadRequest(scene->mMaterials[i], aiTextureType_DIFFUSE,scenepath,request )) {
			textureLoadRequests.push_back(request);
		}
		if (GrabTextureLoadRequest(scene->mMaterials[i], aiTextureType_NORMALS, scenepath, request)) {
			textureLoadRequests.push_back(request);
		}
		if (GrabTextureLoadRequest(scene->mMaterials[i], aiTextureType_SPECULAR, scenepath, request)) {
			textureLoadRequests.push_back(request);
		}
		if (GrabTextureLoadRequest(scene->mMaterials[i], aiTextureType_SHININESS, scenepath, request)) {
			textureLoadRequests.push_back(request);
		}

		for (int j = 0; j < scene->mMaterials[i]->mNumProperties; j++)
		{
			std::cout << "found param:" << scene->mMaterials[i]->mProperties[j]->mKey.C_Str() << std::endl;
		}
	}

	start1 = std::chrono::system_clock::now();
	load_textures_bulk(textureLoadRequests.data(), textureLoadRequests.size());

	 end = std::chrono::system_clock::now();
	 elapsed = end - start1;
	std::cout << "Texture load time" << elapsed.count() << '\n';

	materials.resize(scene->mNumMaterials);

	std::array<EntityID, 8> blank_textures;
	for (int i = 0; i < 8; i++) {
		blank_textures[i] = blankTexture;
	}

	for (int i = 0; i < scene->mNumMaterials; i++)
	{
		SimpleMaterial mat;
		mat.textureIDs = blank_textures;

		EntityID textureId;
		if (GrabTextureID(scene->mMaterials[i], aiTextureType_DIFFUSE, this, textureId)) {
			mat.textureIDs[0] = textureId;
		}
		if (GrabTextureID(scene->mMaterials[i], aiTextureType_NORMALS, this, textureId)) {
			mat.textureIDs[1] = textureId;
		}
		if (GrabTextureID(scene->mMaterials[i], aiTextureType_SPECULAR, this, textureId)) {
			mat.textureIDs[2] = textureId;
		}
		if (GrabTextureID(scene->mMaterials[i], aiTextureType_SHININESS, this, textureId)) {
			mat.textureIDs[3] = textureId;
		}
		materials[i] = mat;
		
		//std::cout << std::endl;
		//std::cout << "iterating material" << scene->mMaterials[i]->GetName().C_Str() << std::endl;
		//aiString texpath;
		//if (scene->mMaterials[i]->GetTextureCount(aiTextureType_DIFFUSE))
		//{
		//	scene->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE, 0, &texpath);
		//	
		//	const char* txpath = &texpath.data[0];
		//	for (int i = 0; i < texpath.length; i++)
		//	{
		//		if (texpath.data[i] == '\\')
		//		{
		//			texpath.data[i] = '/';
		//		}
		//	}
		//	std::cout << "loading material" << txpath << std::endl;
		//	EntityID textureId = resourceMap[std::string(txpath)];
		//	textures.push_back(textureId);
		//}
		//else {
		//	textures.push_back(blankTexture);
		//}		
	}
	

	PipelineResource pipeline;
	pipeline.pipeline = graphicsPipeline;
	auto pipeline_id = createResource("pipeline_basiclit", pipeline);

	
	auto blank_descriptor = create_basic_descriptor_sets(pipeline_id, blank_textures);
	std::vector<EntityID> loaded_meshes;
	std::vector<EntityID> mesh_descriptors;
	for (int i = 0; i < scene->mNumMeshes; i++)
	{
		loaded_meshes.push_back( load_assimp_mesh(scene->mMeshes[i]));

		//std::array<EntityID, 8> textureIDs = blank_textures;
		//
		//textureIDs = ; 
		try {
			
			//blank_textures[0] = textures;
			auto descriptor_id = create_basic_descriptor_sets(pipeline_id, materials[scene->mMeshes[i]->mMaterialIndex].textureIDs);
			mesh_descriptors.push_back(descriptor_id);
		}
		catch (std::runtime_error& e) {
			std::cout << "error creating descriptor:" << e.what() << std::endl;
			mesh_descriptors.push_back(blank_descriptor);
		}		
	}


	entt::registry& registry = render_registry;

	std::function<void(aiNode * node, aiMatrix4x4 & parentmat)> process_node = [&mesh_descriptors,&loaded_meshes, &registry, &pipeline_id, &process_node](aiNode* node, aiMatrix4x4& parentmat) {

		aiMatrix4x4 node_mat = /*node->mTransformation */parentmat;
		//std::cout << "node transforms: ";
		//for (int y = 0; y < 4; y++)
		//{
		//	for (int x = 0; x < 4; x++)
		//	{
		//		std::cout << node_mat[x][y] << " ";
		//
		//	}
		//	std::cout << std::endl;
		//}
		for (int msh = 0; msh < node->mNumMeshes; msh++) {
			RenderMeshComponent renderable;
			renderable.descriptor_entity = mesh_descriptors[node->mMeshes[msh]];

			renderable.mesh_resource_entity = loaded_meshes[node->mMeshes[msh]];// node->//mesh_id;
			renderable.pipeline_entity = pipeline_id;

			EntityID id = registry.create();
			registry.assign<RenderMeshComponent>(id, renderable);

			glm::mat4 modelmat;
			for (int y = 0; y < 4; y++)
			{
				for (int x = 0; x < 4; x++)
				{
					modelmat[y][x] = node_mat[x][y];
				}
			}

			registry.assign<TransformComponent>(id);
			//auto model_mat = glm::mat//glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
		
			registry.get<TransformComponent>(id).model = modelmat;
			registry.get<TransformComponent>(id).scale = glm::vec3{1.f};
			registry.get<TransformComponent>(id).location = glm::vec3(0.0f, 0.0f, 0.0f);

			
		}
		
		for (int ch = 0; ch < node->mNumChildren; ch++)
		{
			process_node(node->mChildren[ch],node_mat);
		}
	};
	aiMatrix4x4 mat{};
	process_node(scene->mRootNode, mat);

	return true;	

}



void VulkanEngine::recreate_swapchain()
{
	device.waitIdle();

	cleanup_swap_chain();

	createSwapChain();
	createImageViews();
	create_render_pass();
	create_gfx_pipeline();
	create_depth_resources();
	create_framebuffers();
	
	create_descriptor_pool();

	create_uniform_buffers();

	create_descriptor_sets();

	create_command_buffers();
}

size_t VulkanEngine::align_dynamic_descriptor(size_t initial_alignement)
{
	// Calculate required alignment based on minimum device offset alignment
	size_t minUboAlignment = deviceProperties.limits.minUniformBufferOffsetAlignment;
	size_t dynamicAlignment = initial_alignement;
	if (minUboAlignment > 0) {
		dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}
	return dynamicAlignment;
}
