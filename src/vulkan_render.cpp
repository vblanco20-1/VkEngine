#include <pcheader.h>
#include "vulkan_render.h"
#include "sdl_render.h"
#include "rawbuffer.h"
#include <glm/gtx/vector_angle.hpp>
#include "termcolor.hpp" 
//#include <gli/gli.hpp>
#include <vk_format.h>
//#include <tiny_obj_loader.h>
#include <chrono> 

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"

#include "shader_processor.h"
#include "vulkan_pipelines.h"
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"
#include <assimp/pbrmaterial.h>

#include <framegraph.h>
#include "cubemap_loader.h"
#include "vulkan_textures.h"
#include "../../scene_processor/public/scene_processor.h"





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

void VulkanEngine::create_engine_graph()
{
	VulkanEngine* engine = this;
	FrameGraph& graph = engine->render_graph;
	VkDevice device = (VkDevice)engine->device;
	VmaAllocator allocator = engine->allocator;
	VkExtent2D swapChainSize = engine->swapChainExtent;

	graph.swapchainSize = swapChainSize;


	auto gbuffer_pass = graph.add_pass("GBuffer", [&](vk::CommandBuffer cmd, RenderPass* pass) {

		RenderGBufferPass(cmd);
		}, PassType::Graphics);

	//order is very important
	auto shadow_pass = graph.add_pass("ShadowPass", [&](vk::CommandBuffer cmd, RenderPass* pass) {
		
		this->render_shadow_pass(cmd, pass->render_height, pass->render_width);
		}, PassType::Graphics);


	

	auto ssao0_pass = graph.add_pass("SSAO-pre", [&](vk::CommandBuffer cmd, RenderPass* pass) {
		
		render_ssao_pass(cmd, pass->render_height, pass->render_width);
		}, PassType::Graphics);

	auto blurx_pass = graph.add_pass("SSAO-blurx", [&](vk::CommandBuffer cmd, RenderPass* pass) {
		
		render_ssao_blurx(cmd, pass->render_height, pass->render_width);
		}, PassType::Graphics);

	auto blury_pass = graph.add_pass("SSAO-blury", [&](vk::CommandBuffer cmd, RenderPass* pass) {
	
		render_ssao_blury(cmd, pass->render_height, pass->render_width);
		}, PassType::Graphics);

	auto forward_pass = graph.add_pass("MainPass", [&](vk::CommandBuffer cmd, RenderPass* pass) {

		RenderMainPass(cmd);
		}, PassType::Graphics);
	
	auto test_pass = graph.add_pass("test", [&](vk::CommandBuffer cmd, RenderPass* pass) {
		if (engine->globalFrameNumber > 3)
		{

		}
		}, PassType::CPU);

	auto display_pass = graph.add_pass("DisplayPass", [](vk::CommandBuffer cmd, RenderPass* pass) {}, PassType::Graphics);

	RenderAttachmentInfo shadowbuffer;
	shadowbuffer.format = VK_FORMAT_D16_UNORM;
	shadowbuffer.size_class = SizeClass::Absolute;
	shadowbuffer.size_x = 2048;
	shadowbuffer.size_y = 2048;
	shadowbuffer.set_clear_depth(1.0f, 0);

	RenderAttachmentInfo gbuffer_position;
	gbuffer_position.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	gbuffer_position.set_clear_color({ 0.0f, 0.0f, 0.0f, 1.0f });

	RenderAttachmentInfo gbuffer_normal;
	gbuffer_normal.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	gbuffer_position.set_clear_color({ 0.0f, 0.0f, 0.0f, 1.0f });

	RenderAttachmentInfo gbuffer_depth;
	gbuffer_depth.format = (VkFormat)engine->findDepthFormat();
	gbuffer_depth.set_clear_depth(0.0f, 0);



	RenderAttachmentInfo ssao_pre;
	ssao_pre.format = VK_FORMAT_R8_UNORM;
#if 0
	ssao_pre.size_x = 1.f; // 0.5f;
	ssao_pre.size_y = 1.f; // 0.5f;
#else
	ssao_pre.size_x = 0.5f;
	ssao_pre.size_y = 0.5f;
#endif
	ssao_pre.set_clear_color({ 0.0f, 0.0f, 0.0f, 1.0f });

	RenderAttachmentInfo ssao_midblur = gbuffer_position;
	ssao_midblur.format = VK_FORMAT_R8_UNORM;
	RenderAttachmentInfo ssao_post = ssao_midblur;

	RenderAttachmentInfo render_output = gbuffer_position;

	shadow_pass->set_depth_attachment("shadow_buffer_1", shadowbuffer);

	gbuffer_pass->add_color_attachment("gbuf_pos", gbuffer_position);
	gbuffer_pass->add_color_attachment("gbuf_normal", gbuffer_normal);
	gbuffer_pass->set_depth_attachment("depth_prepass", gbuffer_depth);

	ssao0_pass->add_image_dependency("gbuf_pos");
	ssao0_pass->add_image_dependency("gbuf_normal");
	ssao0_pass->add_color_attachment("ssao_pre", ssao_pre);

	blurx_pass->add_image_dependency("ssao_pre");
	blurx_pass->add_color_attachment("ssao_mid", ssao_midblur);

	blury_pass->add_image_dependency("ssao_mid");
	blury_pass->add_color_attachment("ssao_post", ssao_post);

	forward_pass->add_image_dependency("gbuf_pos");
	forward_pass->add_image_dependency("gbuf_normal");
	forward_pass->add_image_dependency("shadow_buffer_1");
	forward_pass->add_image_dependency("ssao_post");

	forward_pass->add_color_attachment("main_image", render_output);
	forward_pass->set_depth_attachment("depth_prepass", gbuffer_depth);

	display_pass->add_image_dependency("main_image");
	display_pass->add_color_attachment("_output_", render_output);

	graph.build(engine);
}


void VulkanEngine::init_vulkan()
 {
	
	vk::ApplicationInfo appInfo{ "VkEngine",VK_MAKE_VERSION(0,0,0),"VkEngine:Demo",VK_MAKE_VERSION(0,0,0),VK_API_VERSION_1_1 };

	vk::InstanceCreateInfo createInfo;

	createInfo.pApplicationInfo = &appInfo;	

	playerCam.camera_location = glm::vec3(0, 100, 600);
	playerCam.camera_forward = glm::vec3(0, 1, 0);
	playerCam.camera_up = glm::vec3(0, 0, 1);

	config_parameters.PlayerCam = false;
	config_parameters.CamUp = glm::vec3(0, 0, 1);
	config_parameters.fov = 90.f;
	config_parameters.ShadowView = false;

	config_parameters.sun_location = glm::vec3(100.0f, 1000.0f, 800.0f);
	config_parameters.shadow_near = 1024.f;
	config_parameters.shadow_far = 8096.f;
	config_parameters.shadow_sides = 2048.f;

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

	descriptorMegapool.initialize(MAX_FRAMES_IN_FLIGHT,device);

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = physicalDevice;
	allocatorInfo.device = device;
	
	vmaCreateAllocator(&allocatorInfo, &allocator);

	createSwapChain();

	createImageViews();

	create_render_pass();

	//create_engine_graph(this);
	create_engine_graph();
	
	


	DisplayImage = "main_image";

	create_gfx_pipeline();	

	create_ssao_pipelines();

	create_descriptor_pool();

	create_command_pool();

	

	create_depth_resources();

	create_shadow_framebuffer();

	create_gbuffer_framebuffer(swapChainExtent.width, swapChainExtent.height);

	create_framebuffers();

	create_semaphores();

	

	create_depth_resources();

	create_command_buffers();

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
	
	
	tex_loader = TextureLoader::create_new_loader(this);

	blankTexture = load_texture(MAKE_ASSET_PATH("sprites/blank.png"), "blank");
	blackTexture = load_texture(MAKE_ASSET_PATH("sprites/black.png"), "black");
	//testCubemap = load_texture(MAKE_ASSET_PATH("models/SunTemple_Skybox.hdr"), "cubemap", true);

	bluenoiseTexture = load_texture(MAKE_ASSET_PATH("sprites/bluenoise256.png"), "blunoise");
	//testCubemap = load_texture(MAKE_ASSET_PATH("sprites/cubemap_yokohama_bc3_unorm.ktx"), "cubemap",true);
	testCubemap = load_texture(MAKE_ASSET_PATH("sprites/pisa_cube.ktx"), "cubemap", true);
	testCubemap = load_texture(MAKE_ASSET_PATH("sprites/pisa_cube.ktx"), "cubemap", true);
	load_texture(MAKE_ASSET_PATH("sprites/ibl_brdf_lut.png"), "brdf", false);

	CubemapLoader cubeLoad;
	cubeLoad.initialize(this);
	//"sprites / cubemap_yokohama_bc3_unorm.ktx"
	//TextureResource irradianceMap = cubeLoad.load_cubemap(MAKE_ASSET_PATH("sprites/pisa_cube.ktx"));
	TextureResource irradianceMap = cubeLoad.load_cubemap(MAKE_ASSET_PATH("sprites/pisa_cube.ktx"),64, CubemapFilterMode::IRRADIANCE);

	TextureResource reflectionMap = cubeLoad.load_cubemap(MAKE_ASSET_PATH("sprites/pisa_cube.ktx"), 512, CubemapFilterMode::REFLECTION);
	createResource("irradiance_map", irradianceMap);
	createResource("reflection_map", reflectionMap);
	sceneParameters.fog_a = glm::vec4(1);
	sceneParameters.fog_b.x = 0.f;
	sceneParameters.fog_b.y = 10000.f;
	sceneParameters.ambient = glm::vec4(1.f);
	sceneParameters.kernel_width = 4;
	sceneParameters.ssao_roughness = 600;
	//load_scene("E:/Gamedev/tps-demo/level/geometry/demolevel.blend");
	
#if 1
	//load_scene(MAKE_ASSET_PATH("models/Bistro_v4/Bistro_Interior.fbx"), glm::mat4(1.f));
	//load_scene(MAKE_ASSET_PATH("models/Bistro_v4/Bistro_Exterior.fbx"), glm::mat4(1.f));

	//load_scene(MAKE_ASSET_PATH("models/Elemental/Elemental.obj"),
	//	glm::rotate(glm::mat4(1), glm::radians(90.f), glm::vec3(1, 0, 0)));
	//load_scene(MAKE_ASSET_PATH("models/sponza/sponza_light.glb"),//glm::mat4(100.f));
	//	glm::rotate(glm::scale(glm::vec3(100.f)), glm::radians(90.f), glm::vec3(1, 0, 0)));

	

	sp::SceneLoader* loader = sp::SceneLoader::Create();

	sp::SceneProcessConfig loadConfig;
	loadConfig.bLoadMeshes = true;
	loadConfig.bLoadNodes = true;
	//textures take a huge amount of time
	loadConfig.bLoadTextures = true;
	
	//loadConfig.rootMatrix = &glm::rotate(glm::scale(glm::vec3(100.f)), glm::radians(90.f), glm::vec3(1, 0, 0))[0][0];
	//loader->transform_scene(MAKE_ASSET_PATH("models/sponza/sponza_light.glb"),//glm::mat4(100.f));
	//	loadConfig);

	

	loadConfig.database_name = "bistro_ext.db";
	loadConfig.rootMatrix =& glm::mat4(1.f)[0][0];//&glm::rotate(glm::scale(glm::vec3(100.f)), glm::radians(90.f), glm::vec3(1, 0, 0))[0][0];
	//loader->transform_scene(MAKE_ASSET_PATH("models/Bistro_v4/Bistro_Exterior.fbx"),//glm::mat4(100.f));
	//	loadConfig);

	load_scene("bistro_ext.db",MAKE_ASSET_PATH("models/Bistro_v4/Bistro_Exterior.fbx"), glm::rotate(glm::scale(glm::vec3(100.f)), glm::radians(90.f), glm::vec3(1, 0, 0)));

	//load_scene("sun_temple.db",
	//	MAKE_ASSET_PATH("models/SunTemple.fbx"),
	//	glm::rotate(glm::scale(glm::vec3(100.f)), glm::radians(90.f), glm::vec3(1, 0, 0)));

	//load_scene("scene.db",MAKE_ASSET_PATH("models/sponza/sponza_light.glb"),//glm::mat4(100.f));
	//	glm::rotate(glm::scale(glm::vec3(100.f)), glm::radians(90.f), glm::vec3(1, 0, 0)));

#else

	load_scene(TODO,
		MAKE_ASSET_PATH("models/SunTemple.fbx"),
		glm::rotate(glm::mat4(1), glm::radians(90.f), glm::vec3(1, 0, 0)));
#endif
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
	FrameGraph::GraphAttachment* norm_attachment = render_graph.get_attachment("depth_prepass");
	depthImageView = norm_attachment->descriptor.imageView;
	depthImage.image  =(vk::Image) norm_attachment->image;
}


void VulkanEngine::create_descriptor_pool()
{
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

}


EntityID VulkanEngine::create_basic_descriptor_sets(EntityID pipelineID, std::string name, std::array<EntityID,8> textureID)
{
	
	const PipelineResource &pipeline = render_registry.get<PipelineResource>(pipelineID);

	DescriptorSetBuilder setBuilder{ pipeline.effect,&descriptorMegapool };

	DescriptorResource descriptors;

	//int siz;
	
	for (int j = 0; j < 8; j++) {
		if (render_registry.valid(textureID[j]) && render_registry.has<TextureResource>(textureID[j])) {
			const TextureResource& texture = render_registry.get<TextureResource>(textureID[j]);

			vk::DescriptorImageInfo imageInfo = {};
			imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			imageInfo.imageView = texture.imageView;
			imageInfo.sampler = texture.textureSampler;

			setBuilder.bind_image(2, 6 + j, imageInfo);			
		}	
		else {
			const TextureResource& texture = render_registry.get<TextureResource>(blankTexture);

			vk::DescriptorImageInfo imageInfo = {};
			imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			imageInfo.imageView = texture.imageView;
			imageInfo.sampler = texture.textureSampler;

			setBuilder.bind_image(2, 6 + j, imageInfo);
		}
	}

	descriptors.materialSet = setBuilder.build_descriptor(2, DescriptorLifetime::Static);

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



PipelineResource* VulkanEngine::GetBlitPipeline()
{
	if (!doesResourceExist<PipelineResource>("output_blit")) {

		auto blitPipelineBuilder = GetOutputBlitPipeline();
		//auto &pass = graph.pass_definitions["DisplayPass"];
		auto pipelineEffect = getResource<ShaderEffectHandle>("output").handle;

		vk::Pipeline pipeline = blitPipelineBuilder->build_pipeline(device, renderPass, 0, pipelineEffect);

		PipelineResource pipelineRes;
		pipelineRes.pipeline = pipeline;
		pipelineRes.pipelineBuilder = blitPipelineBuilder;
		pipelineRes.effect = pipelineEffect;
		pipelineRes.renderPassName = "DisplayPass";
		createResource("output_blit", pipelineRes);
	}

	return &getResource<PipelineResource>("output_blit");
}

struct GraphicsPipelineBuilder* VulkanEngine::GetOutputBlitPipeline()
{
	ShaderEffect* pipelineEffect;
	if (!doesResourceExist<ShaderEffectHandle>("output")) {

		ShaderEffectHandle newShader;
		newShader.handle = new ShaderEffect();

		newShader.handle->add_shader_from_file(MAKE_ASSET_PATH("shaders/fullscreen.vert"));
		newShader.handle->add_shader_from_file(MAKE_ASSET_PATH("shaders/blit.frag"));

		newShader.handle->build_effect(device);

		pipelineEffect = newShader.handle;

		createResource<ShaderEffectHandle>("output", newShader);
	}
	else
	{
		pipelineEffect = getResource<ShaderEffectHandle>("output").handle;

	}

	GraphicsPipelineBuilder *blitBuilder = new GraphicsPipelineBuilder();
	blitBuilder->data.vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{};
	blitBuilder->data.inputAssembly = VkPipelineInitializers::build_input_assembly(vk::PrimitiveTopology::eTriangleList);
	//blitBuilder.data.viewport = VkPipelineInitializers::build_viewport(sizex, sizey);
	//blitBuilder.data.scissor = VkPipelineInitializers::build_rect2d(0, 0, sizex, sizey);
	blitBuilder->data.multisampling = VkPipelineInitializers::build_multisampling();
	blitBuilder->data.depthStencil = VkPipelineInitializers::build_depth_stencil(true, false, vk::CompareOp::eAlways);
	blitBuilder->data.rasterizer = VkPipelineInitializers::build_rasterizer();
	blitBuilder->data.rasterizer.cullMode = vk::CullModeFlagBits::eNone;
	//1 color attachments
	blitBuilder->data.colorAttachmentStates.push_back(VkPipelineInitializers::build_color_blend_attachment_state());

	blitBuilder->data.dynamicStates = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};

	return blitBuilder;

	//shadowPipeline = blitBuilder.build_pipeline(device, shadowPass.renderPass, 0, pipelineEffect);
}

void* VulkanEngine::get_profiler_context(vk::CommandBuffer cmd)
{
	if (!MainProfilerContext)
	{
		MainProfilerContext = TracyVkContext(physicalDevice, device, graphicsQueue, cmd);
	}
	return MainProfilerContext;//ProfilerContexts[reinterpret_cast<uint64_t>( VkCommandBuffer(cmd))];
}

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

	gfxPipelineBuilder = new GraphicsPipelineBuilder();


	gfxPipelineBuilder->data.vertexInputInfo = Vertex::getPipelineCreateInfo();
	gfxPipelineBuilder->data.inputAssembly = VkPipelineInitializers::build_input_assembly(vk::PrimitiveTopology::eTriangleList);
	gfxPipelineBuilder->data.viewport = VkPipelineInitializers::build_viewport(swapChainExtent.width, swapChainExtent.height);
	gfxPipelineBuilder->data.scissor = VkPipelineInitializers::build_rect2d(0, 0, swapChainExtent.width, swapChainExtent.height);
	gfxPipelineBuilder->data.depthStencil = VkPipelineInitializers::build_depth_stencil(true, false,vk::CompareOp::eEqual);
	gfxPipelineBuilder->data.rasterizer = VkPipelineInitializers::build_rasterizer();
	gfxPipelineBuilder->data.multisampling = VkPipelineInitializers::build_multisampling();
	gfxPipelineBuilder->data.colorAttachmentStates.push_back(VkPipelineInitializers::build_color_blend_attachment_state());	

	RenderPass* pass = render_graph.get_pass("MainPass");

	graphicsPipeline = gfxPipelineBuilder->build_pipeline(device, pass->built_pass, 0, pipelineEffect);
}


void VulkanEngine::create_shadow_pipeline()
{
	ShaderEffect* pipelineEffect;
	if (!doesResourceExist<ShaderEffectHandle>("shadowpipeline")) {

		ShaderEffectHandle newShader;
		newShader.handle = new ShaderEffect();

		newShader.handle->add_shader_from_file(MAKE_ASSET_PATH("shaders/basicshadow.vert"));
		//newShader.handle->add_shader_from_file(MAKE_ASSET_PATH("shaders/basicshadow.frag"));

		newShader.handle->build_effect(device);

		pipelineEffect = newShader.handle;

		createResource<ShaderEffectHandle>("shadowpipeline", newShader);
	}
	else
	{
		pipelineEffect = getResource<ShaderEffectHandle>("shadowpipeline").handle;

	}

	GraphicsPipelineBuilder shadowPipelineBuilder;
	shadowPipelineBuilder.data.vertexInputInfo = Vertex::getPipelineCreateInfo();
	shadowPipelineBuilder.data.inputAssembly = VkPipelineInitializers::build_input_assembly(vk::PrimitiveTopology::eTriangleList);
	shadowPipelineBuilder.data.viewport = VkPipelineInitializers::build_viewport(2048, 2048);
	shadowPipelineBuilder.data.scissor = VkPipelineInitializers::build_rect2d(0, 0, 2048, 2048);
	shadowPipelineBuilder.data.depthStencil = VkPipelineInitializers::build_depth_stencil(true, true);
	shadowPipelineBuilder.data.rasterizer = VkPipelineInitializers::build_rasterizer();
	//shadowPipelineBuilder.data.rasterizer.cullMode = vk::CullModeFlagBits::eFront;
	shadowPipelineBuilder.data.multisampling = VkPipelineInitializers::build_multisampling();
	
	shadowPipelineBuilder.data.dynamicStates = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
		vk::DynamicState::eDepthBias
	};

	shadowPipeline = shadowPipelineBuilder.build_pipeline(device, shadowPass.renderPass, 0, pipelineEffect);
}
float lerp(float a, float b, float f)
{
	return a + f * (b - a);
}

constexpr int SSAO_KERNEL_SIZE = 16;
constexpr int SSAO_NOISE_DIM = 4;

void VulkanEngine::create_ssao_pipelines()
{
	ShaderEffect* pipelineEffect;
	if (!doesResourceExist<ShaderEffectHandle>("ssao")) {

		ShaderEffectHandle newShader;
		newShader.handle = new ShaderEffect();

		newShader.handle->add_shader_from_file(MAKE_ASSET_PATH("shaders/fullscreen.vert"));
		newShader.handle->add_shader_from_file(MAKE_ASSET_PATH("shaders/ssao.frag"));

		newShader.handle->build_effect(device);

		pipelineEffect = newShader.handle;

		createResource<ShaderEffectHandle>("ssao", newShader);
	}
	else
	{
		pipelineEffect = getResource<ShaderEffectHandle>("ssao").handle;
	}

	FrameGraph::GraphAttachment* ssaoAttachment = render_graph.get_attachment("ssao_pre");// &graph.attachments["ssao_pre"];

	int sizex = ssaoAttachment->real_width;
	int sizey = ssaoAttachment->real_height;

	ssaoPipelineBuilder = new GraphicsPipelineBuilder();
	ssaoPipelineBuilder->data.vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{};// = Vertex::getPipelineCreateInfo();
	ssaoPipelineBuilder->data.inputAssembly = VkPipelineInitializers::build_input_assembly(vk::PrimitiveTopology::eTriangleList);
	ssaoPipelineBuilder->data.viewport = VkPipelineInitializers::build_viewport(sizex, sizey);
	ssaoPipelineBuilder->data.scissor = VkPipelineInitializers::build_rect2d(0, 0, sizex, sizey);
	ssaoPipelineBuilder->data.multisampling = VkPipelineInitializers::build_multisampling();
	ssaoPipelineBuilder->data.depthStencil = VkPipelineInitializers::build_depth_stencil(true, false, vk::CompareOp::eAlways);
	ssaoPipelineBuilder->data.rasterizer = VkPipelineInitializers::build_rasterizer();
	ssaoPipelineBuilder->data.rasterizer.cullMode = vk::CullModeFlagBits::eNone;
	//1 color attachments
	ssaoPipelineBuilder->data.colorAttachmentStates.push_back(VkPipelineInitializers::build_color_blend_attachment_state());

	
	RenderPass* pass = render_graph.get_pass("SSAO-pre");
	vk::Pipeline ssaoPipeline = ssaoPipelineBuilder->build_pipeline(device, pass->built_pass, 0, pipelineEffect);

	
	PipelineResource pipeline;
	pipeline.pipeline = ssaoPipeline;
	pipeline.effect = pipelineEffect;
	pipeline.pipelineBuilder = ssaoPipelineBuilder;
	pipeline.renderPassName = "SSAO-pre";
	auto pipeline_id = createResource("pipeline_ssao", pipeline);

	//ssaoPipeline = ssaoPipelineBuilder->build_pipeline(device, renderPass, 0, pipelineEffect);

	


	//cam buffer
	createBuffer(sizeof(glm::vec4) * SSAO_KERNEL_SIZE, vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, ssaoSamples);


	// SSAO
	std::default_random_engine rndEngine((unsigned)time(nullptr));
	std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

	// Sample kernel
	std::vector<glm::vec4> ssaoKernel(SSAO_KERNEL_SIZE);
	for (uint32_t i = 0; i < SSAO_KERNEL_SIZE; ++i)
	{
		glm::vec3 sample(rndDist(rndEngine) * 2.0 - 1.0, rndDist(rndEngine) * 2.0 - 1.0, rndDist(rndEngine));
		sample = glm::normalize(sample);
		//sample *= rndDist(rndEngine);
		float scale = 1.f;
		if (i < 8) {
			scale = 0.6f;
		}
		//float(i) / float(SSAO_KERNEL_SIZE);
		//scale = lerp(0.1f, 1.0f, scale * scale);
		ssaoKernel[i] = glm::vec4(sample * scale, 0.0f);
	}


	void* data = mapBuffer(ssaoSamples);



	memcpy(data, &ssaoKernel[0], sizeof(glm::vec4) * SSAO_KERNEL_SIZE);


	unmapBuffer(ssaoSamples);

	// Random noise
	std::vector<glm::vec4> ssaoNoise(SSAO_NOISE_DIM * SSAO_NOISE_DIM);
	for (uint32_t i = 0; i < static_cast<uint32_t>(ssaoNoise.size()); i++)
	{
		ssaoNoise[i] = glm::vec4(rndDist(rndEngine) * 2.0f - 1.0f, rndDist(rndEngine) * 2.0f - 1.0f, 0.0f, 0.0f);
	}

	// BLUR--------------

	
	if (!doesResourceExist<ShaderEffectHandle>("ssao-blur")) {

		ShaderEffectHandle newShader;
		newShader.handle = new ShaderEffect();

		newShader.handle->add_shader_from_file(MAKE_ASSET_PATH("shaders/fullscreen.vert"));
		newShader.handle->add_shader_from_file(MAKE_ASSET_PATH("shaders/blur.frag"));

		newShader.handle->build_effect(device);

		pipelineEffect = newShader.handle;

		createResource<ShaderEffectHandle>("ssao-blur", newShader);
	}
	else
	{
		pipelineEffect = getResource<ShaderEffectHandle>("ssao-blur").handle;
	}

	blurPipelineBuilder = new GraphicsPipelineBuilder();
	blurPipelineBuilder->data.vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{};// = Vertex::getPipelineCreateInfo();
	blurPipelineBuilder->data.inputAssembly = VkPipelineInitializers::build_input_assembly(vk::PrimitiveTopology::eTriangleList);
	blurPipelineBuilder->data.viewport = VkPipelineInitializers::build_viewport(swapChainExtent.width, swapChainExtent.height);
	blurPipelineBuilder->data.scissor = VkPipelineInitializers::build_rect2d(0, 0, swapChainExtent.width, swapChainExtent.height);
	blurPipelineBuilder->data.multisampling = VkPipelineInitializers::build_multisampling();
	blurPipelineBuilder->data.depthStencil = VkPipelineInitializers::build_depth_stencil(true, false, vk::CompareOp::eAlways);
	blurPipelineBuilder->data.rasterizer = VkPipelineInitializers::build_rasterizer();
	blurPipelineBuilder->data.rasterizer.cullMode = vk::CullModeFlagBits::eNone;
	//1 color attachments
	blurPipelineBuilder->data.colorAttachmentStates.push_back(VkPipelineInitializers::build_color_blend_attachment_state());


	vk::Pipeline blurx = blurPipelineBuilder->build_pipeline(device, render_graph.get_pass("SSAO-blurx")->built_pass, 0, pipelineEffect);
	vk::Pipeline blury = blurPipelineBuilder->build_pipeline(device, render_graph.get_pass("SSAO-blury")->built_pass, 0, pipelineEffect);

	//PipelineResource pipeline;
	pipeline.pipeline = blurx;
	pipeline.effect = pipelineEffect;
	pipeline.pipelineBuilder = blurPipelineBuilder;
	pipeline.renderPassName = "SSAO-blurx";
	pipeline_id = createResource("pipeline_ssao_blurx", pipeline);

	
	pipeline.pipeline = blury;
	pipeline.effect = pipelineEffect;
	pipeline.pipelineBuilder = blurPipelineBuilder;
	pipeline.renderPassName = "SSAO-blury";
	pipeline_id = createResource("pipeline_ssao_blury", pipeline);
}


void VulkanEngine::create_gbuffer_pipeline()
{
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	ShaderEffect* pipelineEffect;
	if (!doesResourceExist<ShaderEffectHandle>("basicgbuf")) {

		ShaderEffectHandle newShader;
		newShader.handle = new ShaderEffect();

		newShader.handle->add_shader_from_file(MAKE_ASSET_PATH("shaders/gbuffer.vert"));
		newShader.handle->add_shader_from_file(MAKE_ASSET_PATH("shaders/gbuffer.frag"));

		newShader.handle->build_effect(device);

		pipelineEffect = newShader.handle;

		createResource<ShaderEffectHandle>("basicgbuf", newShader);
	}
	else
	{
		pipelineEffect = getResource<ShaderEffectHandle>("basicgbuf").handle;
	}

	gbufferPipelineBuilder = new GraphicsPipelineBuilder();


	gbufferPipelineBuilder->data.vertexInputInfo = Vertex::getPipelineCreateInfo();
	gbufferPipelineBuilder->data.inputAssembly = VkPipelineInitializers::build_input_assembly(vk::PrimitiveTopology::eTriangleList);
	gbufferPipelineBuilder->data.viewport = VkPipelineInitializers::build_viewport(swapChainExtent.width, swapChainExtent.height);
	gbufferPipelineBuilder->data.scissor = VkPipelineInitializers::build_rect2d(0, 0, swapChainExtent.width, swapChainExtent.height);
	gbufferPipelineBuilder->data.depthStencil = VkPipelineInitializers::build_depth_stencil(true, true,vk::CompareOp::eGreaterOrEqual);
	gbufferPipelineBuilder->data.rasterizer = VkPipelineInitializers::build_rasterizer();
	gbufferPipelineBuilder->data.multisampling = VkPipelineInitializers::build_multisampling();
	//2 oclor attachments
	gbufferPipelineBuilder->data.colorAttachmentStates.push_back(VkPipelineInitializers::build_color_blend_attachment_state());
	gbufferPipelineBuilder->data.colorAttachmentStates.push_back(VkPipelineInitializers::build_color_blend_attachment_state());

	gbufferPipeline = gbufferPipelineBuilder->build_pipeline(device, gbuffPass.renderPass, 0, pipelineEffect);
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
	depthAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
	depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
	depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eLoad;
	depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	depthAttachment.initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
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
	vk::Result result = vk::Result(vmaCreateBuffer(allocator, &vkbinfo, &vmaallocInfo, &vkbuffer, &allocation, nullptr));

	assert(result == vk::Result::eSuccess);
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
	vk::DeviceSize UBOSize = (sizeof(UniformBufferObject));

	shadowDataBuffers.resize(swapChainImages.size());
	cameraDataBuffers.resize(swapChainImages.size());
	sceneParamBuffers.resize(swapChainImages.size());
	object_buffers.resize(swapChainImages.size());
	//uniformBuffersMemory.resize(swapChainImages.size());

	for (size_t i = 0; i < swapChainImages.size(); i++) {
		//cam buffer
		createBuffer(sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, cameraDataBuffers[i]);
	
		//shadow buffer
		createBuffer(sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, shadowDataBuffers[i]);


		//scene data buffer
		createBuffer(sizeof(GPUSceneParams), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, sceneParamBuffers[i]);

		//mesh transform buffer
		createBuffer(sizeof(glm::mat4) * 10000, vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, object_buffers[i]);
	

	}

}

void VulkanEngine::create_command_buffers()
{
	vk::CommandBufferAllocateInfo allocInfo;	
	allocInfo.commandPool = commandPool;
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandBufferCount = commandBuffers.num;


	auto buffers = device.allocateCommandBuffers(allocInfo);
	for (int i = 0; i < commandBuffers.num; i++) {
		commandBuffers.items[i] = buffers[i];
	}
	

	get_profiler_context(commandBuffers.get(0));
}
void VulkanEngine::begin_frame_command_buffer(vk::CommandBuffer buffer)
{
	vk::CommandBufferBeginInfo beginInfo;
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	beginInfo.pInheritanceInfo = nullptr; // Optional

	vk::CommandBuffer& cmd = buffer;//commandBuffers[i];

	cmd.reset(vk::CommandBufferResetFlags{});
	cmd.begin(beginInfo);
}
void VulkanEngine::start_frame_renderpass(vk::CommandBuffer buffer, vk::Framebuffer framebuffer)
{
	vk::RenderPassBeginInfo renderPassInfo;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = framebuffer;//swapChainFramebuffers[i];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapChainExtent;

	std::array<vk::ClearValue, 1> clearValues = {};
	clearValues[0].color = vk::ClearColorValue{ std::array<float,4> { 0.0f, 0.0f, 0.2f, 1.0f } };
	//clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = clearValues.size();
	renderPassInfo.pClearValues = clearValues.data();
	tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(buffer);
	//TracyVkCollect(profilercontext, cmd);
	buffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
}
static int counter = 0;
void VulkanEngine::end_frame_command_buffer(vk::CommandBuffer cmd)
{
	tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);

	//cmd.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags{}, 0, nullptr, 0, nullptr, 0, nullptr);

	cmd.endRenderPass();

	//counter++;
	//if (counter > 5) {
		TracyVkCollect(profilercontext, cmd);
		counter = 0;
	//}
	
	
	cmd.end();
}

void VulkanEngine::draw_frame()
{
	ZoneNamedNC(Framemark1, "Draw Frame 0", tracy::Color::Blue1, currentFrameIndex == 0);
	ZoneNamedNC(Framemark2, "Draw Frame 1", tracy::Color::Blue2, currentFrameIndex == 1);
	ZoneNamedNC(Framemark3, "Draw Frame 2 ", tracy::Color::Blue3, currentFrameIndex == 2);
	//("Draw Frame", color);
	{
		ZoneScopedNC("WaitFences", tracy::Color::Red);



		uint64_t waitValue = get_last_frame_timeline_value();


		vk::SemaphoreWaitInfoKHR waitInfo;
		waitInfo.flags = vk::SemaphoreWaitFlagBitsKHR{};
		waitInfo.semaphoreCount = 1;
		waitInfo.pSemaphores = &frameTimelineSemaphore;
		waitInfo.pValues = &waitValue;
		std::cout << "cpu pre pass: " << globalFrameNumber << " - " << device.getSemaphoreCounterValueKHR(frameTimelineSemaphore, extensionDispatcher) << std::endl;

		device.waitSemaphoresKHR(waitInfo, UINT64_MAX, extensionDispatcher);
		std::cout << "cpu frame pass: " << globalFrameNumber << " - " << device.getSemaphoreCounterValueKHR(frameTimelineSemaphore, extensionDispatcher) << std::endl;

		//vkWaitSemaphoresKHR(device, (VkSemaphoreWaitInfoKHR*)&waitInfo, UINT64_MAX);
	}
	{
		ZoneScopedNC("Real Fence", tracy::Color::Yellow);
		
		device.waitForFences(1, &inFlightFences[currentFrameIndex], VK_TRUE, 0);
		device.resetFences(1, &inFlightFences[currentFrameIndex]);
	}
	descriptorMegapool.set_frame(currentFrameIndex);

	vk::ResultValue<uint32_t> imageResult = device.acquireNextImageKHR(swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphores[currentFrameIndex], nullptr);

	if (imageResult.result == vk::Result::eErrorOutOfDateKHR) {
		recreate_swapchain();
		return;
	}


	uint32_t imageIndex = imageResult.value;
	//std::cout << "swapchain image is " << imageIndex << "engine index is " << currentFrameIndex << std::endl;
	vk::SubmitInfo submitInfo;


	vk::Semaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrameIndex] };
	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;



	



	

	update_uniform_buffer(currentFrameIndex/*imageIndex*/);

	vk::CommandBufferAllocateInfo allocInfo;
	allocInfo.commandPool = commandPool;
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandBufferCount = 1;

	eng_stats.drawcalls = 0;

	vk::CommandBuffer cmd = commandBuffers.get(globalFrameNumber);//commandBuffers[currentFrameIndex];

	begin_frame_command_buffer(cmd);
	tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);

	{
		ZoneScopedNC("RenderSort", tracy::Color::Violet);
		//render_registry.sort<RenderMeshComponent>([](const RenderMeshComponent& A, const RenderMeshComponent& B) {
		//	return A.mesh_resource_entity < B.mesh_resource_entity;
		//	}, entt::std_sort{});
	}

	render_graph.execute(cmd);

	

	start_frame_renderpass(cmd, swapChainFramebuffers[imageIndex]);

	{
		
		VkCommandBuffer commandbuffer = VkCommandBuffer(cmd);
		TracyVkZone(profilercontext, commandbuffer, "All Frame");

		PipelineResource* blit = GetBlitPipeline();
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, blit->pipeline);

		vk::Viewport viewport;
		viewport.height = swapChainExtent.height;
		viewport.width = swapChainExtent.width;
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;

		cmd.setViewport(0, viewport);

		vk::Rect2D scissor;
		scissor.extent = swapChainExtent;

		cmd.setScissor(0, scissor);

		DescriptorSetBuilder setBuilder{ blit->effect,&descriptorMegapool };

		setBuilder.bind_image(0,0, render_graph.get_image_descriptor(DisplayImage));

		std::array<vk::DescriptorSet, 2> descriptors;
		descriptors[0] = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, blit->effect->build_pipeline_layout(device), 0, 1, &descriptors[0], 0, nullptr);
		
		cmd.draw(3, 1, 0, 0);
		
		{
			
			ZoneScopedNC("Imgui Update", tracy::Color::Grey);
			TracyVkZone(profilercontext, commandbuffer, "Imgui pass");
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
		}

	}
	end_frame_command_buffer(cmd);

	vk::Semaphore signalSemaphores[] = { frameTimelineSemaphore, renderFinishedSemaphores[currentFrameIndex]  };
	{
		const uint64_t waitValue3 = 0; 
		const uint64_t signalValues[] = {
			globalFrameNumber + 1000,globalFrameNumber + 1000
		};
		//const uint64_t globalFrameNumber = 8;

		VkTimelineSemaphoreSubmitInfoKHR timelineInfo3;
		timelineInfo3.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR;
		timelineInfo3.pNext = NULL;
		timelineInfo3.waitSemaphoreValueCount = 1;
		timelineInfo3.pWaitSemaphoreValues = &waitValue3;
		timelineInfo3.signalSemaphoreValueCount = 2;
		timelineInfo3.pSignalSemaphoreValues = signalValues;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmd;
		submitInfo.signalSemaphoreCount = 2;
		//submitInfo.pSignalSemaphores = signalSemaphores;
		submitInfo.pSignalSemaphores = signalSemaphores;
		submitInfo.pNext = &timelineInfo3;
		{
			ZoneScopedNC("Submit", tracy::Color::Red);
			graphicsQueue.submit(1, &submitInfo, inFlightFences[currentFrameIndex]); //vk::Fence{}); //
			//graphicsQueue.submit(1, &submitInfo, vk::Fence{}); 
		}
		


		vk::PresentInfoKHR presentInfo = {};
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrameIndex];

		vk::SwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;

		presentInfo.pResults = nullptr; // Optional
		{
			ZoneScopedNC("Present", tracy::Color::Red);
			presentQueue.presentKHR(presentInfo);
		}

		currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
		globalFrameNumber++;
	}
}

void set_pipeline_state_depth(int height, int width, const vk::CommandBuffer& cmd)
{
	vk::Viewport viewport;
	viewport.height = height;
	viewport.width = width;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	cmd.setViewport(0, viewport);

	vk::Rect2D scissor;
	scissor.extent.width = height;
	scissor.extent.height = width;

	cmd.setScissor(0, scissor);

	// Set depth bias (aka "Polygon offset")
				// Required to avoid shadow mapping artefacts

	float depthBiasConstant = 1.25f;
	// Slope depth bias factor, applied depending on polygon's slope
	float depthBiasSlope = 1.75f;

	cmd.setDepthBias(depthBiasConstant, 0, depthBiasSlope);
}

vk::DescriptorBufferInfo make_buffer_info(vk::Buffer buffer, size_t size, uint32_t offset = 0) {
	vk::DescriptorBufferInfo info;
	info.buffer = buffer;
	info.offset = offset;
	info.range = size;
	return info;
}
template<typename T>
vk::DescriptorBufferInfo make_buffer_info(const AllocatedBuffer& allocbuffer, uint32_t offset = 0) {
	return make_buffer_info(allocbuffer.buffer, sizeof(T), offset);
}
vk::DescriptorImageInfo VulkanEngine::get_image_resource(const char* name) {
	const TextureResource& texture_env = getResource<TextureResource>(name);//render_registry.get<TextureResource>();
	vk::DescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	imageInfo.imageView = texture_env.imageView;
	imageInfo.sampler = texture_env.textureSampler;
	return imageInfo;
}

void VulkanEngine::render_shadow_pass(const vk::CommandBuffer& cmd, int height, int width)
{
	tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
	TracyVkZoneC(profilercontext, VkCommandBuffer(cmd), "Shadow pass", tracy::Color::Blue);

	


	ZoneScopedNC("Shadow Pass", tracy::Color::BlueViolet);
	EntityID LastMesh = entt::null;

	vk::Pipeline last_pipeline = shadowPipeline;
	vk::PipelineLayout piplayout;
	bool first_render = true;
	ShaderEffect* effect = getResource<ShaderEffectHandle>("shadowpipeline").handle;

	render_registry.view<RenderMeshComponent>().each([&](RenderMeshComponent& renderable) {

		const MeshResource& mesh = render_registry.get<MeshResource>(renderable.mesh_resource_entity);

		bool bShouldBindPipeline = first_render;// ? true : pipeline.pipeline != last_pipeline;

		if (bShouldBindPipeline) {
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, shadowPipeline);

			set_pipeline_state_depth(height, width, cmd);

			piplayout = (vk::PipelineLayout)effect->build_pipeline_layout(device);
			last_pipeline = shadowPipeline;

			DescriptorSetBuilder setBuilder{ effect,&descriptorMegapool };		

			vk::DescriptorBufferInfo shadowBufferInfo = make_buffer_info<UniformBufferObject>(shadowDataBuffers[currentFrameIndex]);

			vk::DescriptorBufferInfo transformBufferInfo = make_buffer_info(object_buffers[currentFrameIndex].buffer, sizeof(glm::mat4) * 10000);

			setBuilder.bind_buffer("ubo", shadowBufferInfo);
			setBuilder.bind_buffer("MainObjectBuffer", transformBufferInfo);

			std::array<vk::DescriptorSet, 2> descriptors;
			descriptors[0] = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);			

			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, piplayout, 0, 1, &descriptors[0], 0, nullptr);
		}

		if (LastMesh != renderable.mesh_resource_entity) {
			vk::Buffer meshbuffers[] = { mesh.vertexBuffer.buffer };
			vk::DeviceSize offsets[] = { 0 };
			cmd.bindVertexBuffers(0, 1, meshbuffers, offsets);

			cmd.bindIndexBuffer(mesh.indexBuffer.buffer, 0, vk::IndexType::eUint32);

			LastMesh = renderable.mesh_resource_entity;
		}

		cmd.pushConstants(piplayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(int), &renderable.object_idx);

		
		cmd.drawIndexed(static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
		eng_stats.drawcalls++;
		
		first_render = false;
	});

}

void VulkanEngine::render_ssao_pass(const vk::CommandBuffer& cmd, int height, int width)
{
	tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
	TracyVkZoneC(profilercontext, VkCommandBuffer(cmd), "SSAO pass", tracy::Color::Red);

	ZoneScopedNC("SSAO Pass", tracy::Color::BlueViolet);

	TextureResource bluenoise = getResource<TextureResource>(bluenoiseTexture);
	PipelineResource ssaopip = getResource<PipelineResource>("pipeline_ssao");

	ShaderEffect* effect = ssaopip.effect;

	VkPipelineLayout layout = effect->build_pipeline_layout((VkDevice)device);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ssaopip.pipeline);

	DescriptorSetBuilder setBuilder{ effect,&descriptorMegapool };

	vk::DescriptorBufferInfo camBufferInfo = make_buffer_info<UniformBufferObject>(cameraDataBuffers[currentFrameIndex]);
	vk::DescriptorBufferInfo ssaoSamplesbuffer = make_buffer_info(ssaoSamples.buffer, sizeof(glm::vec4) * SSAO_KERNEL_SIZE);

	vk::DescriptorImageInfo noiseImage = {};
	noiseImage.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	noiseImage.imageView = bluenoise.imageView;
	noiseImage.sampler = bluenoise.textureSampler;//bluenoise.textureSampler;

	setBuilder.bind_buffer("ubo", camBufferInfo);
	setBuilder.bind_buffer("uboSSAOKernel", ssaoSamplesbuffer);

	setBuilder.bind_image(0, 0, render_graph.get_image_descriptor("gbuf_pos"));
	setBuilder.bind_image(0, 1, render_graph.get_image_descriptor("gbuf_normal"));
	setBuilder.bind_image(0, 2, noiseImage);

	std::array<vk::DescriptorSet, 2> descriptors;
	descriptors[0] = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, 1, &descriptors[0], 0, nullptr);

	cmd.draw(3, 1, 0, 0);	
}

void VulkanEngine::render_ssao_blurx(const vk::CommandBuffer& cmd, int height, int width)
{
	tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
	TracyVkZoneC(profilercontext, VkCommandBuffer(cmd), "SSAO blurX", tracy::Color::Red);

	ZoneScopedNC("SSAO Blur X", tracy::Color::BlueViolet);

	PipelineResource ssaopip = getResource<PipelineResource>("pipeline_ssao_blurx");

	ShaderEffect* effect = ssaopip.effect;

	VkPipelineLayout layout = effect->build_pipeline_layout((VkDevice)device);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ssaopip.pipeline);

	DescriptorSetBuilder setBuilder{ effect,&descriptorMegapool };	
	

	setBuilder.bind_image(0, 0, render_graph.get_image_descriptor("ssao_pre"));
	setBuilder.bind_image(0, 1, render_graph.get_image_descriptor("gbuf_pos"));


	std::array<vk::DescriptorSet, 2> descriptors;
	descriptors[0] = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, 1, &descriptors[0], 0, nullptr);

	glm::vec4 blur_dir;
	blur_dir.x = (1.f /(float) swapChainExtent.width) * 2.f;;
	blur_dir.y = 0;
	blur_dir.z = sceneParameters.ssao_roughness;
	blur_dir.w = sceneParameters.kernel_width;
	cmd.pushConstants(layout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(glm::vec4), &blur_dir);

	cmd.draw(3, 1, 0, 0);
}

constexpr int MULTIDRAW = 1;

void VulkanEngine::render_ssao_blury(const vk::CommandBuffer& cmd, int height, int width)
{
	tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
	TracyVkZoneC(profilercontext, VkCommandBuffer(cmd), "SSAO blurY", tracy::Color::Red);
	ZoneScopedNC("SSAO Blur Y", tracy::Color::BlueViolet);

	PipelineResource ssaopip = getResource<PipelineResource>("pipeline_ssao_blury");

	ShaderEffect* effect = ssaopip.effect;

	VkPipelineLayout layout = effect->build_pipeline_layout((VkDevice)device);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ssaopip.pipeline);

	DescriptorSetBuilder setBuilder{ effect,&descriptorMegapool };



	setBuilder.bind_image(0, 0, render_graph.get_image_descriptor("ssao_mid"));
	setBuilder.bind_image(0, 1, render_graph.get_image_descriptor("gbuf_pos"));

	std::array<vk::DescriptorSet, 2> descriptors;
	descriptors[0] = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, 1, &descriptors[0], 0, nullptr);

	glm::vec4 blur_dir;
	blur_dir.y = (1.f / (float)swapChainExtent.height)*2.f;
	blur_dir.x = 0;
	blur_dir.z = sceneParameters.ssao_roughness;
	blur_dir.w = sceneParameters.kernel_width;
	cmd.pushConstants(layout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(float) * 4, &blur_dir);

	cmd.draw(3, 1, 0, 0);
}


void VulkanEngine::RenderMainPass(const vk::CommandBuffer& cmd)
{
	tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
	TracyVkZoneC(profilercontext, VkCommandBuffer(cmd), "Forward Pass", tracy::Color::Red);
	TextureResource bluenoise = getResource<TextureResource>(bluenoiseTexture);

	ZoneScopedNC("Main Pass", tracy::Color::BlueViolet);
	EntityID LastMesh = entt::null;
	
	vk::Pipeline last_pipeline;
	vk::PipelineLayout piplayout;
	vk::Buffer last_vertex_buffer;
	vk::Buffer last_index_buffer;
	bool first_render = true;		

	static std::vector<DrawUnit> drawables;

	drawables.clear();
	drawables.reserve(render_registry.view<RenderMeshComponent>().size());

	render_registry.group<RenderMeshComponent,TransformComponent,ObjectBounds>().each([&](RenderMeshComponent& renderable,TransformComponent& tf ,ObjectBounds& bounds) {
		const MeshResource& mesh = render_registry.get<MeshResource>(renderable.mesh_resource_entity);
		const PipelineResource& pipeline = render_registry.get<PipelineResource>(renderable.pass_pipelines[(size_t)MeshPasIndex::MainPass]);
		const DescriptorResource& descriptor = render_registry.get<DescriptorResource>(renderable.pass_descriptors[(size_t)MeshPasIndex::MainPass]);

		bool bVisible = false;

		glm::vec3 bounds_center = glm::vec3(bounds.center_rad);
		glm::vec3 dir = normalize(mainCam.eyeLoc - bounds_center);

		float angle = glm::angle(dir, mainCam.eyeDir);

		glm::vec3 bmin = bounds_center - glm::vec3(bounds.extent);
		glm::vec3 bmax = bounds_center + glm::vec3(bounds.extent);

		bVisible = mainCam.camfrustum.IsBoxVisible(bmin,bmax); //glm::degrees(angle) < 90.f;

		if (bVisible) {
			DrawUnit newDrawUnit;
			newDrawUnit.indexBuffer = mesh.indexBuffer.buffer;
			newDrawUnit.vertexBuffer = mesh.vertexBuffer.buffer;
			newDrawUnit.index_count = mesh.indices.size();
			newDrawUnit.material_set = descriptor.materialSet;
			newDrawUnit.object_idx = renderable.object_idx;
			newDrawUnit.pipeline = pipeline.pipeline;
			newDrawUnit.effect = pipeline.effect;

			drawables.push_back(newDrawUnit);
		}		
	});

	std::sort(drawables.begin(), drawables.end(), [](const DrawUnit& a, const DrawUnit& b) {
		return a.vertexBuffer < b.vertexBuffer;
		});	

	for(const DrawUnit& unit : drawables){		
		
		const bool bShouldBindPipeline = unit.pipeline != last_pipeline;

		if (bShouldBindPipeline) {
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, unit.pipeline);
			piplayout = (vk::PipelineLayout)unit.effect->build_pipeline_layout(device);
			last_pipeline = unit.pipeline;

			DescriptorSetBuilder setBuilder{ unit.effect,&descriptorMegapool };

			vk::DescriptorBufferInfo camBufferInfo;
			if (config_parameters.ShadowView)
			{
				camBufferInfo.buffer = shadowDataBuffers[currentFrameIndex].buffer;
			}
			else
			{
				camBufferInfo.buffer = cameraDataBuffers[currentFrameIndex].buffer;
			}

			camBufferInfo.offset = 0;
			camBufferInfo.range = sizeof(UniformBufferObject);


			vk::DescriptorBufferInfo shadowBufferInfo = make_buffer_info<UniformBufferObject>(shadowDataBuffers[currentFrameIndex]);

			vk::DescriptorBufferInfo sceneBufferInfo = make_buffer_info<GPUSceneParams>(sceneParamBuffers[currentFrameIndex]);

		    vk::DescriptorBufferInfo transformBufferInfo = make_buffer_info(object_buffers[currentFrameIndex].buffer, sizeof(glm::mat4) * 10000);

			vk::DescriptorImageInfo shadowInfo = {};
			shadowInfo.imageLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
			shadowInfo.imageView = shadowPass.depth.view;
			shadowInfo.sampler = bluenoise.textureSampler;

			vk::DescriptorImageInfo noiseImage = {};
			noiseImage.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			noiseImage.imageView = bluenoise.imageView;
			noiseImage.sampler = bluenoise.textureSampler;

			setBuilder.bind_image("ssaoMap", render_graph.get_image_descriptor("ssao_post"));
			setBuilder.bind_image("samplerBRDFLUT", get_image_resource("brdf"));
			setBuilder.bind_image("blueNoise", noiseImage);
			setBuilder.bind_image("shadowMap", shadowInfo);
			setBuilder.bind_image("ambientCubemap", get_image_resource("irradiance_map"));
			setBuilder.bind_image("reflectionCubemap", get_image_resource("reflection_map"));
			setBuilder.bind_buffer("ubo", camBufferInfo);
			setBuilder.bind_buffer("shadowUbo", shadowBufferInfo);
			setBuilder.bind_buffer("sceneParams", sceneBufferInfo);
			setBuilder.bind_buffer("MainObjectBuffer", transformBufferInfo);

			std::array<vk::DescriptorSet, 2> descriptors;
			descriptors[0] = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);
			descriptors[1] = setBuilder.build_descriptor(1, DescriptorLifetime::PerFrame);

			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, piplayout, 0, 2, &descriptors[0], 0, nullptr);
		}

		if (last_vertex_buffer != unit.vertexBuffer) {
			vk::Buffer meshbuffers[] = { unit.vertexBuffer };
			vk::DeviceSize offsets[] = { 0 };

			cmd.bindVertexBuffers(0, 1, meshbuffers, offsets);		

			last_vertex_buffer = unit.vertexBuffer;
		}

		if (last_index_buffer != unit.indexBuffer) {
			
			cmd.bindIndexBuffer(unit.indexBuffer, 0, vk::IndexType::eUint32);

			last_index_buffer = unit.indexBuffer;
		}

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, piplayout, 2, 1, &unit.material_set, 0, nullptr);

		cmd.pushConstants(piplayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(int), &unit.object_idx);

		cmd.drawIndexed(static_cast<uint32_t>(unit.index_count), 1, 0, 0, 0);
		eng_stats.drawcalls++;

		first_render = false;
	}

	//render_ssao_pass(cmd, swapChainExtent.height, swapChainExtent.width);
}

void VulkanEngine::RenderGBufferPass(const vk::CommandBuffer& cmd)
{

	tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
	TracyVkZoneC(profilercontext, VkCommandBuffer(cmd), "Gbuffer pass", tracy::Color::Grey);

	ZoneScopedNC("GBuffer Pass", tracy::Color::BlueViolet);
	EntityID LastMesh = entt::null;

	vk::Pipeline last_pipeline;
	vk::PipelineLayout piplayout;
	vk::Buffer last_vertex_buffer;
	vk::Buffer last_index_buffer;
	bool first_render = true;

	static std::vector<DrawUnit> drawables;

	drawables.clear();
	drawables.reserve(render_registry.view<RenderMeshComponent>().size());

	ShaderEffect* effect = getResource<ShaderEffectHandle>("basicgbuf").handle;
	render_registry.group<RenderMeshComponent, TransformComponent, ObjectBounds>().each([&](RenderMeshComponent& renderable, TransformComponent& tf, ObjectBounds& bounds) {
		const MeshResource& mesh = render_registry.get<MeshResource>(renderable.mesh_resource_entity);
		//const PipelineResource& pipeline = render_registry.get<PipelineResource>(renderable.pass_pipelines[(size_t)MeshPasIndex::MainPass]);
		const DescriptorResource& descriptor = render_registry.get<DescriptorResource>(renderable.pass_descriptors[(size_t)MeshPasIndex::MainPass]);

		bool bVisible = false;

		glm::vec3 bounds_center = glm::vec3(bounds.center_rad);
		glm::vec3 dir = normalize(mainCam.eyeLoc - bounds_center);

		float angle = glm::angle(dir, mainCam.eyeDir);

		glm::vec3 bmin = bounds_center - glm::vec3(bounds.extent);
		glm::vec3 bmax = bounds_center + glm::vec3(bounds.extent);

		bVisible = mainCam.camfrustum.IsBoxVisible(bmin, bmax); //glm::degrees(angle) < 90.f;

		if (bVisible) {
			DrawUnit newDrawUnit;
			newDrawUnit.indexBuffer = mesh.indexBuffer.buffer;
			newDrawUnit.vertexBuffer = mesh.vertexBuffer.buffer;
			newDrawUnit.index_count = mesh.indices.size();
			//newDrawUnit.material_set = descriptor.materialSet;
			newDrawUnit.object_idx = renderable.object_idx;
			newDrawUnit.pipeline = gbufferPipeline;//pipeline.pipeline;
			newDrawUnit.effect = effect;

			drawables.push_back(newDrawUnit);
		}
		});

	std::sort(drawables.begin(), drawables.end(), [](const DrawUnit& a, const DrawUnit& b) {
		return a.vertexBuffer < b.vertexBuffer;
		});

	

	for (const DrawUnit& unit : drawables) {

		const bool bShouldBindPipeline = unit.pipeline != last_pipeline;

		if (bShouldBindPipeline) {
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, unit.pipeline);
			piplayout = (vk::PipelineLayout)unit.effect->build_pipeline_layout(device);
			last_pipeline = unit.pipeline;

			DescriptorSetBuilder setBuilder{ unit.effect,&descriptorMegapool };

			vk::DescriptorBufferInfo camBufferInfo;
			if (config_parameters.ShadowView)
			{
				camBufferInfo.buffer = shadowDataBuffers[currentFrameIndex].buffer;
			}
			else
			{
				camBufferInfo.buffer = cameraDataBuffers[currentFrameIndex].buffer;
			}

			camBufferInfo.offset = 0;
			camBufferInfo.range = sizeof(UniformBufferObject);

			vk::DescriptorBufferInfo transformBufferInfo;
			transformBufferInfo.buffer = object_buffers[currentFrameIndex].buffer;
			transformBufferInfo.offset = 0;
			transformBufferInfo.range = sizeof(glm::mat4) * 10000;
						
			setBuilder.bind_buffer("ubo", camBufferInfo);			
			setBuilder.bind_buffer("MainObjectBuffer", transformBufferInfo);

			std::array<vk::DescriptorSet, 2> descriptors;
			descriptors[0] = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);

			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, piplayout, 0, 1, &descriptors[0], 0, nullptr);
		}

		if (last_vertex_buffer != unit.vertexBuffer) {
			vk::Buffer meshbuffers[] = { unit.vertexBuffer };
			vk::DeviceSize offsets[] = { 0 };

			cmd.bindVertexBuffers(0, 1, meshbuffers, offsets);

			last_vertex_buffer = unit.vertexBuffer;
		}

		if (last_index_buffer != unit.indexBuffer) {

			cmd.bindIndexBuffer(unit.indexBuffer, 0, vk::IndexType::eUint32);

			last_index_buffer = unit.indexBuffer;
		}

		//cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, piplayout, 2, 1, &unit.material_set, 0, nullptr);

		cmd.pushConstants(piplayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(int), &unit.object_idx);

		cmd.drawIndexed(static_cast<uint32_t>(unit.index_count), 1, 0, 0, 0);
		eng_stats.drawcalls++;

		first_render = false;
	}
}

//std::vector<UniformBufferObject> StagingCPUUBOArray;
void VulkanEngine::update_uniform_buffer(uint32_t currentImage)
{
	ZoneScopedN("Update Uniforms");

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

	auto eye = glm::vec3(camUp, 100, 600);


	

	UniformBufferObject ubo = {};
	ubo.model = glm::mat4(1.0f);

	ubo.proj = glm::perspective(glm::radians(config_parameters.fov), swapChainExtent.width / (float)swapChainExtent.height,  50000.0f, 0.1f);
	glm::mat4 revproj = glm::perspective(glm::radians(config_parameters.fov), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 50000.0f);
	ubo.proj[1][1] *= -1;
	if (!config_parameters.PlayerCam) {

		ubo.view = glm::lookAt(eye, glm::vec3(0.0f, 400.0f, 0.0f), config_parameters.CamUp);
		
		ubo.eye = glm::vec4(eye, 0.0f);
		//invert projection matrix couse glm is inverted y compared to vulkan
		

		mainCam.eyeLoc = eye;
		mainCam.eyeDir = glm::normalize(eye - glm::vec3(0.0f, 400.0f, 0.0f));
	}
	else {
		ubo.view = glm::lookAt(playerCam.camera_location, playerCam.camera_location+ playerCam.camera_forward, playerCam.camera_up);
		ubo.eye = glm::vec4(playerCam.camera_location, 0.0f);


		mainCam.eyeLoc = ubo.eye;
		mainCam.eyeDir = playerCam.camera_forward;
	}
	mainCam.camfrustum = Frustum(revproj * ubo.view);
	

	UniformBufferObject shadowubo = {};	

	glm::vec3 shadowloc = config_parameters.sun_location;
	glm::vec3 shadowtarget = glm::vec3(0.0f);


	float s = config_parameters.shadow_sides;
	float n = config_parameters.shadow_near;
	float f = config_parameters.shadow_far;

	shadowubo.view = glm::lookAt(shadowloc, glm::vec3(0.0f, 400.0f, 0.0f), config_parameters.CamUp);
	shadowubo.proj = glm::ortho(-s,s,-s,s,-n,f);//glm::perspective(glm::radians(89.f), 1.0f,1.f, 5000.f);
	shadowubo.proj[1][1] *= -1;
	shadowubo.model = shadowubo.view * shadowubo.proj;//shadowubo.view * shadowubo.proj;



	void* data = mapBuffer(cameraDataBuffers[currentImage]);
	//auto alignUbo = align_dynamic_descriptor(sizeof(ubo));
	char* dt = (char*)data;// + alignUbo; ;
	
	
	memcpy(dt, &ubo, sizeof(UniformBufferObject) );
	

	unmapBuffer(cameraDataBuffers[currentImage]);

	data = mapBuffer(sceneParamBuffers[currentImage]);
	
	dt = (char*)data;

	memcpy(dt, &sceneParameters, sizeof(GPUSceneParams));

	unmapBuffer(sceneParamBuffers[currentImage]);


	data = mapBuffer(shadowDataBuffers[currentImage]);

	dt = (char*)data;

	memcpy(dt, &shadowubo, sizeof(UniformBufferObject));

	unmapBuffer(shadowDataBuffers[currentImage]);



		int copyidx = 0;
		std::vector<glm::mat4> object_matrices;

		object_matrices.resize(render_registry.capacity<RenderMeshComponent>());
		
		render_registry.group<RenderMeshComponent, TransformComponent, ObjectBounds>().each([&](EntityID id, RenderMeshComponent& renderable, const TransformComponent& transform, ObjectBounds& bounds) {

			object_matrices[copyidx] = transform.model;			
			renderable.object_idx = copyidx;
			copyidx++;
		});

		if (copyidx > 0) {
			ZoneScopedN("Uniform copy");

			void* matdata = mapBuffer(object_buffers[currentImage]);
			memcpy(matdata, object_matrices.data(), object_matrices.size() * sizeof(glm::mat4));

			unmapBuffer(object_buffers[currentImage]);
		}
}






void VulkanEngine::cleanup_swap_chain()
{
	for (auto imageView : swapChainImageViews) {
		device.destroyImageView(imageView);
	}
	for (auto framebuffer : swapChainFramebuffers) {
		device.destroyFramebuffer(framebuffer);
	}

	for (size_t i = 0; i < cameraDataBuffers.size(); i++) {
		//device.freeMemory(uniformBuffers[i]);
	//	device.freeMemory(uniformBuffersMemory[i]);
		destroyBuffer(cameraDataBuffers[i]);
	}

	
	device.destroyDescriptorPool(descriptorPool);

	device.destroyImageView(depthImageView);

	destroyImage(depthImage);
	//device.destroyImage(depthImage);
	
	//device.freeMemory(depthImageMemory);

	//device.freeCommandBuffers(commandPool, commandBuffers);

	device.destroyPipeline(graphicsPipeline);
	//device.destroyPipelineLayout(pipelineLayout);
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

	vk::SemaphoreTypeCreateInfoKHR timelineSemaphoreInfo;
	timelineSemaphoreInfo.semaphoreType = vk::SemaphoreTypeKHR::eTimeline;
	timelineSemaphoreInfo.initialValue = 0;

	vk::SemaphoreCreateInfo timeline;
	timeline.setPNext(&timelineSemaphoreInfo);
	
	frameTimelineSemaphore = device.createSemaphore(timeline);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		imageAvailableSemaphores[i] = device.createSemaphore(semaphoreInfo);
		renderFinishedSemaphores[i] = device.createSemaphore(semaphoreInfo);
		inFlightFences[i] = device.createFence(fenceInfo);
	}
}

void VulkanEngine::create_shadow_framebuffer()
{
	shadowPass.width = SHADOWMAP_DIM;
	shadowPass.height = SHADOWMAP_DIM;

	//copy all the parameters from framegraph into the manual vrsion
	FrameGraph::GraphAttachment* shadowAttachment = render_graph.get_attachment("shadow_buffer_1");//&graph.attachments["shadow_buffer_1"];
	RenderPass* pass = render_graph.get_pass("ShadowPass"); //&graph.pass_definitions["ShadowPass"];

	shadowPass.depth.image = shadowAttachment->image;
	shadowPass.depth.view = shadowAttachment->descriptor.imageView;
	shadowPass.depthSampler = shadowAttachment->descriptor.sampler;
	shadowPass.frameBuffer = pass->framebuffer;
	shadowPass.renderPass = pass->built_pass;

	create_shadow_pipeline();
}


void VulkanEngine::create_gbuffer_framebuffer(int width, int height)
{
	//copy all the parameters from framegraph into the manual vrsion
	FrameGraph::GraphAttachment* pos_attachment = render_graph.get_attachment("gbuf_pos");
	FrameGraph::GraphAttachment* norm_attachment = render_graph.get_attachment("gbuf_normal");
	RenderPass* pass = render_graph.get_pass("GBuffer");

	gbuffPass.normal.view = norm_attachment->descriptor.imageView;
	gbuffPass.posdepth.image = pos_attachment->image;
	gbuffPass.normal.image = norm_attachment->image;
	gbuffPass.posdepth.view = pos_attachment->descriptor.imageView;
	gbuffPass.normalSampler = norm_attachment->descriptor.sampler;
	gbuffPass.posdepthSampler = pos_attachment->descriptor.sampler;
	gbuffPass.frameBuffer = pass->framebuffer;
	gbuffPass.renderPass = pass->built_pass;
	
	create_gbuffer_pipeline();
}


void VulkanEngine::rebuild_pipeline_resource(PipelineResource* resource)
{	
	resource->effect->reload_shaders(device);

	if (resource->renderPassName != "") {

		//RenderPass &pass = graph.pass_definitions[resource->renderPassName];

		resource->pipeline = resource->pipelineBuilder->build_pipeline(device, render_graph.get_pass(resource->renderPassName)->built_pass, 
			0, resource->effect);
	}	
}

bool VulkanEngine::load_model(const char* model_path, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices)
{

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
	

	glm::vec3 bmin = { mesh->mAABB.mMin.x,mesh->mAABB.mMin.y,mesh->mAABB.mMin.z };
	glm::vec3 bmax = { mesh->mAABB.mMax.x,mesh->mAABB.mMax.y,mesh->mAABB.mMax.z };

	ObjectBounds bounds;

	bounds.center_rad = glm::vec4((bmax + bmin) / 2.f,1.f);

	bounds.extent = glm::abs(glm::vec4((bmax - bmin) / 2.f, 0.f));
	bounds.center_rad.w = glm::length(bounds.extent);

	newMesh.bounds = bounds;

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


bool GrabTextureLoadRequest(const aiScene* scene, aiMaterial* material, aiTextureType textype, const std::string &scenepath,TextureLoadRequest& LoadRequest) {
	
	aiString texpath;
	if (material->GetTextureCount(textype))
	{
		material->GetTexture(textype, 0, &texpath);

		if (auto texture = scene->GetEmbeddedTexture(texpath.C_Str())) {
			return false;
			//size_t tex_size;
			//if (texture->mHeight == 0) {
			//	tex_size = texture->mWidth;
			//}
			//int x, y, c;
			//stbi_load_from_memory((stbi_uc*)texture->pcData, tex_size, &x, &y, &c, STBI_default);
			//std::cout << x << y << c << std::endl;
		}

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
	int texcount = material->GetTextureCount(textype);
	for ( int m = 0; m < texcount; m++)
	{
		material->GetTexture(textype, m, &texpath);

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

void Coutproperty(aiMaterialProperty* property) {

	switch (property->mType) {
	case aiPTI_Float:
		for (int i = 0; i < property->mDataLength; i += sizeof(float)) {
			std::cout << *((float*)&property->mData[i]) << " - ";
		}
		
		return;
	case aiPTI_Integer:
		for (int i = 0; i < property->mDataLength; i += sizeof(int)) {
			std::cout << *((int*)&property->mData[i]) << " - ";
		}
		return;
	case aiPTI_String:
	case aiPTI_Buffer:
		for (int i = 0; i < property->mDataLength; i +=1) {
			std::cout << property->mData[i];
		}
		return;
	
	}
}


bool VulkanEngine::load_scene(const char* db_path, const char* scene_path, glm::mat4 rootMatrix)
{
	auto start1 = std::chrono::system_clock::now();

	std::filesystem::path sc_path{ std::string(scene_path) };
	Assimp::Importer importer;
	const aiScene* scene;
	{
		ZoneScopedNC("Assimp load", tracy::Color::Magenta);
		scene = importer.ReadFile(scene_path, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_GenBoundingBoxes); //aiProcess_Triangulate | aiProcess_OptimizeMeshes | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_GenBoundingBoxes);
	}
	
	
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
	textureLoadRequests.reserve(scene->mNumMaterials * 4);


	struct SimpleMaterial {
		std::array<EntityID, 8> textureIDs;
		
	};
	std::vector<SimpleMaterial> materials;
	sp::SceneLoader* loader = sp::SceneLoader::Create();
	{
		ZoneScopedNC("Opening database", tracy::Color::Blue);
		

		loader->open_db(db_path);
	}

	{
		const bool outputMaterialInfo = true;
		std::string scenepath = sc_path.parent_path().string();
		ZoneScopedNC("Texture request building", tracy::Color::Green);
		tex_loader->load_all_textures(loader, scenepath);
		//for (int i = 0; i < scene->mNumMaterials; i++)
		//{
		//	std::string matname = scene->mMaterials[i]->GetName().C_Str();
		//	
		//	//std::string find = "Pavement";
		//	bool bdebug = true;// (matname.find(find) != std::string::npos);
		//
		//
		//	if (bdebug || outputMaterialInfo) {
		//
		//
		//		std::cout << std::endl;
		//		std::cout << "iterating material" << scene->mMaterials[i]->GetName().C_Str() << std::endl;
		//	}
		//	TextureLoadRequest request;
		//	std::string scenepath = sc_path.parent_path().string();
		//
		//	tex_loader->add_request_from_assimp_db(loader,scene->mMaterials[i], aiTextureType_DIFFUSE, scenepath);
		//	tex_loader->add_request_from_assimp_db(loader,scene->mMaterials[i], aiTextureType_NORMALS, scenepath);
		//	tex_loader->add_request_from_assimp_db(loader,scene->mMaterials[i], aiTextureType_SPECULAR, scenepath);
		//	tex_loader->add_request_from_assimp_db(loader,scene->mMaterials[i], aiTextureType_METALNESS, scenepath);
		//	tex_loader->add_request_from_assimp_db(loader,scene->mMaterials[i], aiTextureType_EMISSIVE, scenepath);
		//	tex_loader->add_request_from_assimp_db(loader,scene->mMaterials[i], aiTextureType_OPACITY, scenepath);
		//	tex_loader->add_request_from_assimp_db(loader,scene->mMaterials[i], aiTextureType_DIFFUSE_ROUGHNESS, scenepath);
		//	tex_loader->add_request_from_assimp_db(loader,scene->mMaterials[i], aiTextureType_EMISSION_COLOR, scenepath);
		//	tex_loader->add_request_from_assimp_db(loader,scene->mMaterials[i], aiTextureType_BASE_COLOR, scenepath);
		//	tex_loader->add_request_from_assimp_db(loader,scene->mMaterials[i], aiTextureType_UNKNOWN, scenepath);
		//	
		//	if (outputMaterialInfo) {
		//		for (int j = 0; j < scene->mMaterials[i]->mNumProperties; j++)
		//		{
		//			std::cout << "found param:" << scene->mMaterials[i]->mProperties[j]->mKey.C_Str() << " val: ";
		//
		//			Coutproperty(scene->mMaterials[i]->mProperties[j]);
		//			std::cout << std::endl;
		//		}
		//	}
		//}
	}

	{
		ZoneScopedNC("Texture request bulk load", tracy::Color::Yellow);
		//tex_loader->load_all_textures(loader, scenepath);//flush_requests();

		//load_textures_bulk(textureLoadRequests.data(), textureLoadRequests.size());
	}

	

	materials.resize(scene->mNumMaterials);

	std::array<EntityID, 8> blank_textures;
	for (int i = 0; i < 8; i++) {
		if (i < 2)
		{

			blank_textures[i] = blankTexture;
		}
		else {
			blank_textures[i] = blackTexture;
		}
	}
	{
		ZoneScopedNC("Material gather", tracy::Color::Yellow);

		for (int i = 0; i < scene->mNumMaterials; i++)
		{
			SimpleMaterial newMat;
			newMat.textureIDs = blank_textures;

			EntityID textureId; 
			if (GrabTextureID(scene->mMaterials[i], aiTextureType_DIFFUSE, this, textureId)) {
				newMat.textureIDs[0] = textureId;
			}
			if (GrabTextureID(scene->mMaterials[i], aiTextureType_NORMALS, this, textureId)) {
				newMat.textureIDs[1] = textureId;
			}
			if (GrabTextureID(scene->mMaterials[i], aiTextureType_SPECULAR, this, textureId)) {
				newMat.textureIDs[2] = textureId;
			}
			if (GrabTextureID(scene->mMaterials[i], aiTextureType_UNKNOWN, this, textureId)) {
				newMat.textureIDs[3] = textureId;
			}
			//if (GrabTextureID(scene->mMaterials[i], aiTextureType_METALNESS, this, textureId)) {
			//	mat.textureIDs[3] = textureId;
			//}
			//else{
			//	
			//}
			if (GrabTextureID(scene->mMaterials[i], aiTextureType_EMISSION_COLOR, this, textureId)) {
				newMat.textureIDs[4] = textureId;
			}
			if (GrabTextureID(scene->mMaterials[i], aiTextureType_BASE_COLOR, this, textureId)) {
				newMat.textureIDs[5] = textureId;
			}
			if (GrabTextureID(scene->mMaterials[i], aiTextureType_DIFFUSE_ROUGHNESS, this, textureId)) {
				newMat.textureIDs[6] = textureId;
			}

			materials[i] = newMat;
		}

	}

	EntityID pipeline_id;
	if (!doesResourceExist<PipelineResource>("pipeline_basiclit")) {
		PipelineResource pipeline;
		pipeline.pipeline = graphicsPipeline;
		pipeline.effect = getResource<ShaderEffectHandle>("basiclit").handle;
		pipeline.pipelineBuilder = gfxPipelineBuilder;
		pipeline.renderPassName = "MainPass";
		pipeline_id = createResource("pipeline_basiclit", pipeline);
	}
	else {
		pipeline_id = resourceMap["pipeline_basiclit"];
	}
	

	
	auto blank_descriptor = create_basic_descriptor_sets(pipeline_id,"blank" ,blank_textures);
	std::vector<EntityID> loaded_meshes;
	std::vector<EntityID> mesh_descriptors;
	{
		ZoneScopedNC("Descriptor creation", tracy::Color::Magenta);

		for (int i = 0; i < scene->mNumMeshes; i++)
		{
			loaded_meshes.push_back(load_assimp_mesh(scene->mMeshes[i]));

			try {
				std::string matname = scene->mMaterials[scene->mMeshes[i]->mMaterialIndex]->GetName().C_Str();
				//blank_textures[0] = textures;
				auto descriptor_id = create_basic_descriptor_sets(pipeline_id, matname, materials[scene->mMeshes[i]->mMaterialIndex].textureIDs);
				mesh_descriptors.push_back(descriptor_id);
			}
			catch (std::runtime_error & e) {
				std::cout << "error creating descriptor:" << e.what() << std::endl;
				mesh_descriptors.push_back(blank_descriptor);
			}
		}
	}


	entt::registry& registry = render_registry;
	int nodemeshes = 0;
	std::function<void(aiNode * node, aiMatrix4x4 & parentmat)> process_node = [&](aiNode* node, aiMatrix4x4& parentmat) {

		aiMatrix4x4 node_mat = parentmat *node->mTransformation;
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
			nodemeshes++;
			RenderMeshComponent renderable;
			renderable.pass_descriptors[(size_t)MeshPasIndex::MainPass] = mesh_descriptors[node->mMeshes[msh]];			

			renderable.mesh_resource_entity = loaded_meshes[node->mMeshes[msh]];// node->//mesh_id;
			//renderable.pipeline_entity = pipeline_id;

			renderable.pass_pipelines[(size_t)MeshPasIndex::MainPass] = pipeline_id;
			//renderable.pass_pipelines[MeshPasIndex::ShadowPass] = shadowPipeline;


			
			


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

			ObjectBounds meshBounds = getResource<MeshResource>(renderable.mesh_resource_entity).bounds;

			glm::vec4 center = meshBounds.center_rad;
			center.w = 1;

			center = modelmat * center;
			
			meshBounds.center_rad.x = center.x;
			meshBounds.center_rad.y = center.y;
			meshBounds.center_rad.z = center.z;

			registry.assign<ObjectBounds>(id, meshBounds);

			
		}
		
		for (int ch = 0; ch < node->mNumChildren; ch++)
		{
			process_node(node->mChildren[ch],node_mat);
		}
	};

	aiMatrix4x4 mat{};

	
	for (int y = 0; y < 4; y++)
	{
		for (int x = 0; x < 4; x++)
		{
			mat[x][y] = rootMatrix[y][x];
		}
	}

	{
		ZoneScopedNC("Node transform Processing", tracy::Color::Blue);
		process_node(scene->mRootNode, mat);

		std::cout << nodemeshes << "   " << scene->mNumMeshes;
	}
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
