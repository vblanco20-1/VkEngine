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
#include <future>

#include <autobind.h>

#include <murmurhash.h>

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
	ZoneScopedNC("Engine Graph", tracy::Color::Blue);
	VulkanEngine* engine = this;
	FrameGraph& graph = engine->render_graph;
	VkDevice device = (VkDevice)engine->device;
	VmaAllocator allocator = engine->allocator;
	VkExtent2D swapChainSize = engine->swapChainExtent;
	swapChainSize.height *= 1;
	swapChainSize.width *= 1;

	graph.swapchainSize = swapChainSize;


	auto gbuffer_pass = graph.add_pass("GBuffer", [&](vk::CommandBuffer cmd, RenderPass* pass) {

		RenderGBufferPass(cmd);
		}, PassType::Graphics, true);

	//order is very important
	auto shadow_pass = graph.add_pass("ShadowPass", [&](vk::CommandBuffer cmd, RenderPass* pass) {
		
		this->render_shadow_pass(cmd, pass->render_height, pass->render_width);
		}, PassType::Graphics, true);


	

	//auto ssao0_pass = graph.add_pass("SSAO-pre", [&](vk::CommandBuffer cmd, RenderPass* pass) {
	//	
	//	render_ssao_pass(cmd, pass->render_height, pass->render_width);
	//}, PassType::Graphics, false);

	auto ssao1_pass = graph.add_pass("SSAO-pre-comp", [&](vk::CommandBuffer cmd, RenderPass* pass) {

		render_ssao_compute(cmd);
		}, PassType::Compute, false);


#if 1
	auto blurx_pass = graph.add_pass("SSAO-blurx", [&](vk::CommandBuffer cmd, RenderPass* pass) {	

		render_ssao_blurx(cmd, pass->render_height, pass->render_width);
		}, PassType::Graphics);

	auto ssao_taa_pass = graph.add_pass("SSAO-taa", [&](vk::CommandBuffer cmd, RenderPass* pass) {

		render_ssao_taa(cmd, pass->render_height, pass->render_width);
		}, PassType::Graphics);

	auto ssao_taa_flip = graph.add_pass("SSAO-flip", [&](vk::CommandBuffer cmd, RenderPass* pass) {

		render_ssao_flip(cmd, pass->render_height, pass->render_width);
		}, PassType::Graphics);
	//auto blury_pass = graph.add_pass("SSAO-blury", [&](vk::CommandBuffer cmd, RenderPass* pass) {
	//
	//	render_ssao_blury(cmd, pass->render_height, pass->render_width);
	//	}, PassType::Graphics);
#else
	auto blurx_pass = graph.add_pass("SSAO-blurx", [&](vk::CommandBuffer cmd, RenderPass* pass) {
		glm::vec2 blur_dir;
		blur_dir.x = 2.f;
		blur_dir.y = 0;

		render_ssao_blur_compute(cmd, "ssao_pre", "ssao_mid", blur_dir);
		
		}, PassType::Compute);

	auto blury_pass = graph.add_pass("SSAO-blury", [&](vk::CommandBuffer cmd, RenderPass* pass) {

		glm::vec2 blur_dir;		
		blur_dir.x = 0; 
		blur_dir.y = 2.f;

		render_ssao_blur_compute(cmd, "ssao_mid", "ssao_post", blur_dir);

		}, PassType::Compute);
#endif
	auto forward_pass = graph.add_pass("MainPass", [&](vk::CommandBuffer cmd, RenderPass* pass) {
		//vkCmdPipelineBarrier(
		//	cmd,
		//	VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // source stage
		//	VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,    // destination stage
		//	);
		RenderMainPass(cmd);
	}, PassType::Graphics,false);
	
	//auto test_pass = graph.add_pass("test", [&](vk::CommandBuffer cmd, RenderPass* pass) {
	//	if (engine->globalFrameNumber > 3)
	//	{
	//
	//	}
	//	}, PassType::CPU);

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

	RenderAttachmentInfo ssao_accumulate;
	ssao_accumulate.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	ssao_accumulate.set_clear_color({ 0.0f, 0.0f, 0.0f, 1.0f });

	RenderAttachmentInfo gbuffer_normal;
	gbuffer_normal.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	gbuffer_position.set_clear_color({ 0.0f, 0.0f, 0.0f, 1.0f });

	RenderAttachmentInfo gbuffer_depth;
	gbuffer_depth.format = (VkFormat)engine->findDepthFormat();
	gbuffer_depth.set_clear_depth(0.0f, 0);

	RenderAttachmentInfo ssao_pre;

	ssao_pre.format = VK_FORMAT_R8_UNORM;
#if 1
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

	RenderAttachmentInfo main_image = gbuffer_position;

	RenderAttachmentInfo output_image = main_image;


	shadow_pass->set_depth_attachment("shadow_buffer_1", shadowbuffer);

	
	gbuffer_pass->add_color_attachment("gbuf_pos", gbuffer_position);
	gbuffer_pass->add_color_attachment("gbuf_normal", gbuffer_normal);
	gbuffer_pass->add_color_attachment("motion_vectors", gbuffer_position);
	gbuffer_pass->set_depth_attachment("depth_prepass", gbuffer_depth);

	//ssao0_pass->add_image_dependency("gbuf_pos");
	//ssao0_pass->add_image_dependency("gbuf_normal");
	//ssao0_pass->add_color_attachment("ssao_pre", ssao_pre);

	ssao1_pass->add_image_dependency("gbuf_pos");
	ssao1_pass->add_image_dependency("gbuf_normal");
	//ssao1_pass->add_image_dependency("gbuf_normal");
	ssao1_pass->add_color_attachment("ssao_pre", ssao_pre);

	//blurx_pass->add_image_dependency("ssao_pre");
	//blurx_pass->add_color_attachment("ssao_mid", ssao_midblur);
	//
	//blury_pass->add_image_dependency("ssao_mid");
	//blury_pass->add_color_attachment("ssao_post", ssao_post);

	blurx_pass->add_image_dependency("ssao_pre");
	blurx_pass->add_image_dependency("gbuf_normal");
	blurx_pass->add_color_attachment("ssao_mid", ssao_post);

	ssao_taa_pass->add_image_dependency("ssao_mid");
	ssao_taa_pass->add_image_dependency("gbuf_pos");
	ssao_taa_pass->add_image_dependency("ssao_accumulate");
	ssao_taa_pass->add_image_dependency("motion_vectors");
	ssao_taa_pass->add_color_attachment("ssao_post", ssao_post);

	ssao_taa_flip->add_image_dependency("ssao_post");
	ssao_taa_flip->add_image_dependency("gbuf_pos");
	ssao_taa_flip->add_color_attachment("ssao_accumulate", ssao_accumulate);


	//blury_pass->add_image_dependency("ssao_mid");
	//blury_pass->add_color_attachment("ssao_post", ssao_post);

	forward_pass->add_image_dependency("gbuf_pos");
	forward_pass->add_image_dependency("gbuf_normal");
	forward_pass->add_image_dependency("shadow_buffer_1");
	forward_pass->add_image_dependency("ssao_post");

	forward_pass->add_color_attachment("main_image", main_image);
	forward_pass->set_depth_attachment("depth_prepass", gbuffer_depth);

	display_pass->add_image_dependency("main_image");
	display_pass->add_color_attachment("_output_", output_image);

	graph.build(engine);
}


void VulkanEngine::init_vulkan()
{
	ZoneScopedNC("Engine init", tracy::Color::Blue);
	
	vk::ApplicationInfo appInfo{ "VkEngine",VK_MAKE_VERSION(0,0,0),"VkEngine:Demo",VK_MAKE_VERSION(0,0,0),VK_API_VERSION_1_2 };

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

	autobinder = new AutobindState();
	autobinder->Engine = this;

	
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
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	allocatorInfo.instance = instance;
	vmaCreateAllocator(&allocatorInfo, &allocator);

	createSwapChain();

	createImageViews();

	create_render_pass();

	create_engine_graph();

	DisplayImage = "main_image";

	create_gfx_pipeline();	

#if 0
	const int total_count = 100000;

	ShaderEffectHandle ssaopip = getResource<ShaderEffectHandle>("basiclit");
	ShaderEffect* effect = ssaopip.handle;
	VkDescriptorSetLayout layout = effect->build_descriptor_layouts((VkDevice)device)[0];
	for (int n = 0; n < 10; n++)
	{
		{
			ZoneScopedNC("Flip", tracy::Color::Red);
			descriptorMegapool.allocatorPool->Flip();
		}
		{
			ZoneScopedNC("Linear bench", tracy::Color::Red);
			vke::DescriptorAllocatorHandle handle;
			{
				ZoneScopedNC("grab allocator", tracy::Color::Yellow);
				handle = descriptorMegapool.allocatorPool->GetAllocator(vke::DescriptorAllocatorLifetime::PerFrame);
			}
			for (int i = 0; i < total_count; i++) {
				VkDescriptorSet set;
				ZoneScopedNC("alloc", tracy::Color::Orange);
				handle.Allocate(layout, set);
			}
		}
		{
			ZoneScopedNC("Flip", tracy::Color::Red);
			descriptorMegapool.allocatorPool->Flip();
		}
		{
			ZoneScopedNC("Thread bench", tracy::Color::Blue);

			std::vector<int> idx = { 0,1,2 ,3,4,5,6,7 };
			int per_it = total_count / idx.size();

			std::vector<std::future<void>> futures;
			for (auto id : idx) {
				auto handle = std::async(std::launch::async,
					[&](auto idx) {
						ZoneScopedNC("worker", tracy::Color::Red);
						vke::DescriptorAllocatorHandle handle;
						{
							ZoneScopedNC("grab allocator", tracy::Color::Yellow);
							handle = descriptorMegapool.allocatorPool->GetAllocator(vke::DescriptorAllocatorLifetime::PerFrame);
						}
						for (int i = idx * per_it; i < (idx + 1) * per_it; i++) {
							VkDescriptorSet set;
							ZoneScopedNC("alloc", tracy::Color::Orange);
							handle.Allocate(layout, set);
						}
					}, id);
				futures.push_back(std::move(handle));
			}

			for (auto&& f : futures) {
				f.get();
			}
		}
	}

#endif
	create_ssao_pipelines();

	create_descriptor_pool();

	create_command_pool();

	render_graph.build_command_pools();

	create_depth_resources();

	create_shadow_framebuffer();

	create_gbuffer_framebuffer(swapChainExtent.width, swapChainExtent.height);

	create_framebuffers();

	create_semaphores();

	

	create_depth_resources();

	create_command_buffers();

	create_uniform_buffers();

	create_descriptor_sets();
	
	
	tex_loader = TextureLoader::create_new_loader(this);

	blankTexture = load_texture(MAKE_ASSET_PATH("sprites/blank.png"), "blank");
	blackTexture = load_texture(MAKE_ASSET_PATH("sprites/black.png"), "black");
	//testCubemap = load_texture(MAKE_ASSET_PATH("models/SunTemple_Skybox.hdr"), "cubemap", true);

	bluenoiseTexture = load_texture(MAKE_ASSET_PATH("sprites/bluenoise256.png"), "blunoise");
	//testCubemap = load_texture(MAKE_ASSET_PATH("sprites/cubemap_yokohama_bc3_unorm.ktx"), "cubemap",true);
	testCubemap = load_texture(MAKE_ASSET_PATH("sprites/pisa_cube.ktx"), "cubemap", true);
	//testCubemap = load_texture(MAKE_ASSET_PATH("sprites/pisa_cube.ktx"), "cubemap", true);
	load_texture(MAKE_ASSET_PATH("sprites/ibl_brdf_lut.png"), "brdf", false);






	autobinder->image_infos["_txBluenoise"] = get_resource_image_info("blunoise");

	autobinder->image_infos["_txWhite"] = get_resource_image_info("blank");

	autobinder->image_infos["_txBlack"] = get_resource_image_info("black");

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


	auto mainpass = render_graph.get_pass("MainPass");

	sceneParameters.viewport = {0.f,0.f,mainpass->render_width,mainpass->render_height};
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
	//loadConfig.bLoadMeshes = true;
	//loadConfig.bLoadNodes = true;
	//textures take a huge amount of time
	//loadConfig.bLoadTextures = true;
	
	//loadConfig.rootMatrix = &glm::rotate(glm::scale(glm::vec3(100.f)), glm::radians(90.f), glm::vec3(1, 0, 0))[0][0];
	//loader->transform_scene(MAKE_ASSET_PATH("models/sponza/sponza_light.glb"),//glm::mat4(100.f));
	//	loadConfig);

	
	loadConfig.bLoadMaterials = true;
	loadConfig.database_name = "bistro_ext.db";
	loadConfig.rootMatrix =& glm::mat4(1.f)[0][0];//&glm::rotate(glm::scale(glm::vec3(100.f)), glm::radians(90.f), glm::vec3(1, 0, 0))[0][0];
	//loader->transform_scene(MAKE_ASSET_PATH("models/Bistro_v4/Bistro_Exterior.fbx"),//glm::mat4(100.f));
	//	loadConfig);

	load_scene("bistro_ext.db",MAKE_ASSET_PATH("models/Bistro_v4/Bistro_Exterior.fbx"), glm::rotate(glm::scale(glm::vec3(100.f)), glm::radians(90.f), glm::vec3(1, 0, 0)));

#ifdef RTX_ON
	build_ray_structures();
#endif
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




vk::DescriptorImageInfo VulkanEngine::get_resource_image_info(const char* resource_name)
{
	TextureResource res = getResource<TextureResource>(resource_name);

	vk::DescriptorImageInfo image = {};
	image.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	image.imageView = res.imageView;
	image.sampler = res.textureSampler;

	return image;
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



void VulkanEngine::name_object(vk::Semaphore object, const char* name)
{
	static auto func = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");

	if (func != nullptr)//features.bDebugNames) {
	{
		vk::DebugUtilsObjectNameInfoEXT inf;
		inf.objectHandle = (uint64_t)VkSemaphore(object);
		inf.objectType = vk::ObjectType::eSemaphore;
		inf.pObjectName = name;

		func(device,(VkDebugUtilsObjectNameInfoEXT*)&inf);
		//device.setDebugUtilsObjectNameEXT(inf, extensionDispatcher);
	}	
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


	gfxPipelineBuilder->data.vertexInputInfo = VkPipelineInitializers::build_empty_vertex_input();//Vertex::getPipelineCreateInfo();
	gfxPipelineBuilder->data.inputAssembly = VkPipelineInitializers::build_input_assembly(vk::PrimitiveTopology::eTriangleList);
	gfxPipelineBuilder->data.viewport = VkPipelineInitializers::build_viewport(swapChainExtent.width, swapChainExtent.height);
	gfxPipelineBuilder->data.scissor = VkPipelineInitializers::build_rect2d(0, 0, swapChainExtent.width, swapChainExtent.height);
	gfxPipelineBuilder->data.depthStencil = VkPipelineInitializers::build_depth_stencil(true, false,vk::CompareOp::eEqual);
	gfxPipelineBuilder->data.rasterizer = VkPipelineInitializers::build_rasterizer();
	gfxPipelineBuilder->data.multisampling = VkPipelineInitializers::build_multisampling();
	gfxPipelineBuilder->data.colorAttachmentStates.push_back(VkPipelineInitializers::build_color_blend_attachment_state());	
	gfxPipelineBuilder->data.dynamicStates = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
		vk::DynamicState::eDepthBias
	};
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
	shadowPipelineBuilder.data.vertexInputInfo = VkPipelineInitializers::build_empty_vertex_input();//Vertex::getPipelineCreateInfo();
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
	ShaderEffect* ssaoEffect = build_shader_effect(
		{ MAKE_ASSET_PATH("shaders/fullscreen.vert"),
		  MAKE_ASSET_PATH("shaders/ssao.frag")
		},
		"ssao");
	
	ShaderEffect* ssaoEffect_Comp = build_shader_effect(
		{ MAKE_ASSET_PATH("shaders/ssao.comp") },		
		"ssao-comp");

	ShaderEffect* ssaoBlur = build_shader_effect(
		{ MAKE_ASSET_PATH("shaders/fullscreen.vert"),
		  MAKE_ASSET_PATH("shaders/blur.frag")
		},
		"ssao-blur");

	ShaderEffect* ssaoTaa = build_shader_effect(
		{ MAKE_ASSET_PATH("shaders/fullscreen.vert"),
		  MAKE_ASSET_PATH("shaders/ssao_taa.frag")
		},
		"ssao-taa");

	ShaderEffect* ssaoFlip = build_shader_effect(
		{ MAKE_ASSET_PATH("shaders/fullscreen.vert"),
		  MAKE_ASSET_PATH("shaders/ssao_flip.frag")
		},
		"ssao-flip");

	ShaderEffect* ssaoBlur_Comp = build_shader_effect(
		{ MAKE_ASSET_PATH("shaders/blur.comp") },
		"ssao-comp-blur");

	ShaderEffect* rayEffect = build_shader_effect(
		{ MAKE_ASSET_PATH("shaders/rayshadows.comp") },
		"rayshadow-gen");
	
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


	auto ssaoCompBuilder = new ComputePipelineBuilder();
	vk::Pipeline ssaoPipeline_comp = ssaoCompBuilder->build_pipeline(device,ssaoEffect_Comp);

#ifdef RTX_ON
	vk::Pipeline rayPipeline_comp = ssaoCompBuilder->build_pipeline(device, rayEffect);
#endif
	vk::Pipeline ssaoBlurPipeline_comp = ssaoCompBuilder->build_pipeline(device, ssaoBlur_Comp);

	RenderPass* pass = render_graph.get_pass("SSAO-pre");
	vk::Pipeline ssaoPipeline;// = ssaoPipelineBuilder->build_pipeline(device, pass->built_pass, 0, pipelineEffect);

	
	PipelineResource pipeline;
	pipeline.pipeline = ssaoPipeline;
	pipeline.effect = ssaoEffect;
	pipeline.pipelineBuilder = ssaoPipelineBuilder;
	pipeline.renderPassName = "SSAO-pre";
	auto pipeline_id = createResource("pipeline_ssao", pipeline);

	PipelineResource pipeline_Comp;
	pipeline_Comp.pipeline = ssaoPipeline_comp;
	pipeline_Comp.effect = ssaoEffect_Comp;
	pipeline_Comp.computePipelineBuilder = ssaoCompBuilder;
	pipeline_Comp.renderPassName = "SSAO-pre";
	auto pipeline_id_comp = createResource("pipeline_ssao_comp", pipeline_Comp);

	PipelineResource pipelineBlur_Comp;
	pipelineBlur_Comp.pipeline = ssaoBlurPipeline_comp;
	pipelineBlur_Comp.effect = ssaoBlur_Comp;
	pipelineBlur_Comp.computePipelineBuilder = ssaoCompBuilder;
	pipelineBlur_Comp.renderPassName = "SSAO-blur-comp";
	auto pipelineblur_id_comp = createResource("pipeline_ssao_blur_comp", pipelineBlur_Comp);
#ifdef RTX_ON
	PipelineResource pipelineRay_Comp;
	pipelineRay_Comp.pipeline = rayPipeline_comp;
	pipelineRay_Comp.effect = rayEffect;
	pipelineRay_Comp.computePipelineBuilder = ssaoCompBuilder;
	pipelineRay_Comp.renderPassName = "SSAO-pre";
	auto raypip_id_comp = createResource("ray_shadow", pipelineRay_Comp);
#endif

	//cam buffer
	createBuffer(sizeof(glm::vec4) * SSAO_KERNEL_SIZE, vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_UNKNOWN, ssaoSamples);


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


	blurPipelineBuilder->data.dynamicStates = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};

	if (render_graph.get_pass("SSAO-blurx"))
	{
		vk::Pipeline blurx = blurPipelineBuilder->build_pipeline(device, render_graph.get_pass("SSAO-blurx")->built_pass, 0, ssaoBlur);

		pipeline.pipeline = blurx;
		pipeline.effect = ssaoBlur;
		pipeline.pipelineBuilder = blurPipelineBuilder;
		pipeline.renderPassName = "SSAO-blurx";
		pipeline_id = createResource("pipeline_ssao_blurx", pipeline);
	}
	if (render_graph.get_pass("SSAO-blury"))
	{
		vk::Pipeline blury = blurPipelineBuilder->build_pipeline(device, render_graph.get_pass("SSAO-blury")->built_pass, 0, ssaoBlur);
		pipeline.pipeline = blury;
		pipeline.effect = ssaoBlur;
		pipeline.pipelineBuilder = blurPipelineBuilder;
		pipeline.renderPassName = "SSAO-blury";
		pipeline_id = createResource("pipeline_ssao_blury", pipeline);
	}
	if (render_graph.get_pass("SSAO-taa"))
	{
		vk::Pipeline newpip = blurPipelineBuilder->build_pipeline(device, render_graph.get_pass("SSAO-taa")->built_pass, 0, ssaoTaa);
		pipeline.pipeline = newpip;
		pipeline.effect = ssaoTaa;
		pipeline.pipelineBuilder = blurPipelineBuilder;
		pipeline.renderPassName = "SSAO-taa";
		pipeline_id = createResource("pipeline_ssao_taa", pipeline);
	}
	if (render_graph.get_pass("SSAO-flip"))
	{
		vk::Pipeline newpip = blurPipelineBuilder->build_pipeline(device, render_graph.get_pass("SSAO-flip")->built_pass, 0, ssaoFlip);
		pipeline.pipeline = newpip;
		pipeline.effect = ssaoFlip;
		pipeline.pipelineBuilder = blurPipelineBuilder;
		pipeline.renderPassName = "SSAO-flip";
		pipeline_id = createResource("pipeline_ssao_flip", pipeline);
	}
		
}


void VulkanEngine::build_ray_structures()
{
	//std::vector<accelerationstructure
	render_registry.view<MeshResource>().each([&](auto eid, MeshResource& resource) {
		build_mesh_accel_structure(resource);
	});

	
	
	std::vector<vk::AccelerationStructureCreateGeometryTypeInfoKHR> geoInfos;

	//for (int i = 0; i < render_registry.view<RenderMeshComponent>().size(); i++) {
	{
		vk::AccelerationStructureCreateGeometryTypeInfoKHR geoInfo;
		geoInfo.geometryType = vk::GeometryTypeKHR::eInstances;
		geoInfo.maxPrimitiveCount = render_registry.view<RenderMeshComponent>().size(); // 1;
		geoInfo.allowsTransforms = true;
		geoInfos.push_back(geoInfo);
	}

	vk::AccelerationStructureCreateInfoKHR accelCreateInfo;
	accelCreateInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
	accelCreateInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
	accelCreateInfo.deviceAddress = 0;
	accelCreateInfo.compactedSize = 0;
	accelCreateInfo.maxGeometryCount = 1;//render_registry.view<RenderMeshComponent>().size();
	accelCreateInfo.pGeometryInfos = geoInfos.data();

	vk::AccelerationStructureKHR accelStructure = device.createAccelerationStructureKHR(accelCreateInfo,nullptr,extensionDispatcher);

	vk::AccelerationStructureMemoryRequirementsInfoKHR memoryRequirements;
	memoryRequirements.type = vk::AccelerationStructureMemoryRequirementsTypeKHR::eObject;
	memoryRequirements.buildType = vk::AccelerationStructureBuildTypeKHR::eHostOrDevice;
	memoryRequirements.accelerationStructure = accelStructure;

	vk::AccelerationStructureMemoryRequirementsInfoKHR memoryRequirements_scratch;
	memoryRequirements_scratch.type = vk::AccelerationStructureMemoryRequirementsTypeKHR::eBuildScratch;
	memoryRequirements_scratch.buildType = vk::AccelerationStructureBuildTypeKHR::eHostOrDevice;
	memoryRequirements_scratch.accelerationStructure = accelStructure;

	vk::MemoryRequirements2 buildRequirements = device.getAccelerationStructureMemoryRequirementsKHR(memoryRequirements, extensionDispatcher);
	vk::MemoryRequirements2 buildRequirements_scratch = device.getAccelerationStructureMemoryRequirementsKHR(memoryRequirements_scratch, extensionDispatcher);

	vk::MemoryAllocateInfo allocInfo;
	allocInfo.allocationSize = buildRequirements.memoryRequirements.size;
	allocInfo.memoryTypeIndex = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	VmaAllocationCreateInfo alloccreateinfo;
	alloccreateinfo.memoryTypeBits = 0;
	alloccreateinfo.pool = VK_NULL_HANDLE;
	alloccreateinfo.flags = 0;
	alloccreateinfo.requiredFlags = (VkMemoryPropertyFlagBits)vk::MemoryPropertyFlagBits::eDeviceLocal;
	alloccreateinfo.preferredFlags = 0;
	alloccreateinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	alloccreateinfo.pUserData = nullptr;
	VmaAllocation top_alloc;
	VmaAllocationInfo top_alloc_info;

	vmaAllocateMemory(allocator, (VkMemoryRequirements*)&buildRequirements.memoryRequirements, &alloccreateinfo, &top_alloc, &top_alloc_info);
	//top_level_acceleration_structure.memory= device.allocateMemory(allocInfo);
	top_level_acceleration_structure.allocation = top_alloc;

	AllocatedBuffer scratchBuffer;
	createBuffer(buildRequirements_scratch.memoryRequirements.size, vk::BufferUsageFlagBits::eRayTracingKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_GPU_ONLY, scratchBuffer);

	vk::BindAccelerationStructureMemoryInfoKHR memoryBinding;
	memoryBinding.accelerationStructure = accelStructure;
	
	memoryBinding.memory = top_alloc_info.deviceMemory;//top_level_acceleration_structure.allocation->GetMemory();
	memoryBinding.memoryOffset = top_alloc_info.offset;//top_level_acceleration_structure.allocation->GetOffset();	

	device.bindAccelerationStructureMemoryKHR(1, &memoryBinding, extensionDispatcher);

	top_level_acceleration_structure.acceleration_structure = accelStructure;


	//device.getAccelerationStructureHandleNV(accelStructure, sizeof(uint64_t), &top_level_acceleration_structure.handle, extensionDispatcher);

	

	std::vector<vk::AccelerationStructureInstanceKHR > blas_instances;

	render_registry.group<RenderMeshComponent, TransformComponent, ObjectBounds>().each([&](RenderMeshComponent& renderable, TransformComponent& tf, ObjectBounds& bounds) {
		MeshResource& mesh = render_registry.get<MeshResource>(renderable.mesh_resource_entity);

		vk::AccelerationStructureInstanceKHR new_instance;
		new_instance.accelerationStructureReference = mesh.accelStructure.handle;
		new_instance.flags = (VkGeometryInstanceFlagsKHR)vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable;
		new_instance.instanceCustomIndex = renderable.object_idx;
		new_instance.instanceShaderBindingTableRecordOffset = 0;
		new_instance.mask = 1;

		glm::mat3x4 transform_matrix = tf.model;

		memcpy(&new_instance.transform.matrix, &transform_matrix, sizeof(glm::mat3x4));

		blas_instances.push_back(new_instance);
		});

	size_t bfsize = blas_instances.size() * sizeof(vk::AccelerationStructureInstanceKHR);
	AllocatedBuffer instances_Buffer;
	createBuffer(bfsize,vk::BufferUsageFlagBits::eRayTracingKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress ,vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent , VMA_MEMORY_USAGE_UNKNOWN, instances_Buffer);


	void* data;
	vmaMapMemory(allocator, instances_Buffer.allocation, &data);

	memcpy(data, blas_instances.data(), bfsize);

	vmaUnmapMemory(allocator, instances_Buffer.allocation);


	std::vector<vk::AccelerationStructureGeometryKHR> geometries;

	//for (int i = 0; i < blas_instances.size(); i++) {
	{
		vk::AccelerationStructureGeometryKHR geo;
		geo.geometryType = vk::GeometryTypeKHR::eInstances;
		geo.geometry.instances.data.deviceAddress = instances_Buffer.address;//get_buffer_adress(instances_Buffer);
		//geo.geometry.instances.data.hostAddress = &blas_instances[i];
		geo.geometry.instances.arrayOfPointers = false;
		geometries.push_back(geo);
	}

	auto dataptr = geometries.data();
	vk::AccelerationStructureGeometryKHR* const* ptr = &dataptr;
	vk::AccelerationStructureBuildGeometryInfoKHR buildinfo;
	buildinfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
	buildinfo.geometryCount = 1;//geometries.size();

	buildinfo.ppGeometries = ptr;
	buildinfo.geometryArrayOfPointers = false;
	buildinfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
	buildinfo.dstAccelerationStructure = accelStructure;
	buildinfo.scratchData.deviceAddress = scratchBuffer.address;//get_buffer_adress(scratchBuffer);//device.getBufferAddressKHR(scratchadressinfo, extensionDispatcher);


	vk::CommandBuffer commandBuffer = beginSingleTimeCommands();
	

	vk::AccelerationStructureBuildOffsetInfoKHR offset;
	offset.primitiveCount = blas_instances.size();
	auto poffset = &offset;

	commandBuffer.buildAccelerationStructureKHR(1, &buildinfo, &poffset, extensionDispatcher);

	endSingleTimeCommands(commandBuffer);
}

void VulkanEngine::build_mesh_accel_structure(MeshResource& mesh)
{
	ZoneScoped;
	
	std::cout << "building accel structure for: " << mesh.name << std::endl;

	vk::AccelerationStructureCreateGeometryTypeInfoKHR geoInfo;
	geoInfo.geometryType = vk::GeometryTypeKHR::eTriangles;
	geoInfo.indexType = vk::IndexType::eUint32;
	geoInfo.vertexFormat = vk::Format::eR32G32B32Sfloat;
	geoInfo.allowsTransforms = false;
	geoInfo.maxPrimitiveCount = mesh.indices.size() / 3;
	geoInfo.maxVertexCount = mesh.vertices.size();	

	vk::AccelerationStructureCreateInfoKHR accelCreateInfo;
	accelCreateInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
	accelCreateInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
	accelCreateInfo.deviceAddress = 0;
	accelCreateInfo.compactedSize = 0;
	accelCreateInfo.maxGeometryCount = 1;
	accelCreateInfo.pGeometryInfos = &geoInfo;

	vk::AccelerationStructureKHR accelStructure = device.createAccelerationStructureKHR(accelCreateInfo, nullptr, extensionDispatcher);

	vk::AccelerationStructureMemoryRequirementsInfoKHR memoryRequirements;
	memoryRequirements.type = vk::AccelerationStructureMemoryRequirementsTypeKHR::eObject;
	memoryRequirements.buildType = vk::AccelerationStructureBuildTypeKHR::eHostOrDevice;
	memoryRequirements.accelerationStructure = accelStructure;

	vk::AccelerationStructureMemoryRequirementsInfoKHR memoryRequirements_scratch;
	memoryRequirements_scratch.type = vk::AccelerationStructureMemoryRequirementsTypeKHR::eBuildScratch;
	memoryRequirements_scratch.buildType = vk::AccelerationStructureBuildTypeKHR::eHostOrDevice;
	memoryRequirements_scratch.accelerationStructure = accelStructure;

	vk::MemoryRequirements2 buildRequirements = device.getAccelerationStructureMemoryRequirementsKHR(memoryRequirements, extensionDispatcher);
	vk::MemoryRequirements2 buildRequirements_scratch = device.getAccelerationStructureMemoryRequirementsKHR(memoryRequirements_scratch, extensionDispatcher);

	vk::MemoryAllocateInfo allocInfo;
	allocInfo.allocationSize = buildRequirements.memoryRequirements.size;
	allocInfo.memoryTypeIndex = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	VmaAllocationCreateInfo alloccreateinfo;
	alloccreateinfo.memoryTypeBits = 0;
	alloccreateinfo.pool = VK_NULL_HANDLE;
	alloccreateinfo.flags = 0;
	alloccreateinfo.requiredFlags = (VkMemoryPropertyFlagBits)vk::MemoryPropertyFlagBits::eDeviceLocal;
	alloccreateinfo.preferredFlags = 0;
	alloccreateinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	alloccreateinfo.pUserData = nullptr;
	VmaAllocation top_alloc;
	VmaAllocationInfo top_alloc_info;

	vmaAllocateMemory(allocator, (VkMemoryRequirements*)&buildRequirements.memoryRequirements, &alloccreateinfo, &top_alloc, &top_alloc_info);
	//top_level_acceleration_structure.memory= device.allocateMemory(allocInfo);
	//top_level_acceleration_structure.allocation = top_alloc;

	AllocatedBuffer scratchBuffer;
	createBuffer(buildRequirements_scratch.memoryRequirements.size, vk::BufferUsageFlagBits::eRayTracingKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_GPU_ONLY, scratchBuffer);


	vk::BindAccelerationStructureMemoryInfoKHR memoryBinding;
	memoryBinding.accelerationStructure = accelStructure;

	memoryBinding.memory = top_alloc_info.deviceMemory;
	memoryBinding.memoryOffset = top_alloc_info.offset;

	device.bindAccelerationStructureMemoryKHR(1, &memoryBinding, extensionDispatcher);

	mesh.accelStructure.acceleration_structure = accelStructure;


	vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

	//vk::BufferDeviceAddressInfo scratchadressinfo;
	//scratchadressinfo.buffer = scratchBuffer.buffer;

	vk::AccelerationStructureGeometryKHR Geo;
	Geo.flags = vk::GeometryFlagBitsKHR::eOpaque;
	Geo.geometryType = vk::GeometryTypeKHR::eTriangles;
	Geo.geometry.triangles.vertexData = get_buffer_adress(mesh.vertexBuffer);
	Geo.geometry.triangles.indexData = get_buffer_adress(mesh.indexBuffer);
	Geo.geometry.triangles.indexType = vk::IndexType::eUint32;
	Geo.geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
	Geo.geometry.triangles.vertexStride = sizeof(Vertex);
	Geo.geometry.triangles.transformData = {};
	vk::AccelerationStructureGeometryKHR* pgeo;
	pgeo = &Geo;

	vk::AccelerationStructureBuildGeometryInfoKHR buildinfo;
	buildinfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
	buildinfo.geometryCount = 1;	
	buildinfo.ppGeometries = &pgeo;	
	buildinfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
	buildinfo.dstAccelerationStructure = accelStructure;
	buildinfo.scratchData.deviceAddress = get_buffer_adress(scratchBuffer);

	vk::AccelerationStructureBuildOffsetInfoKHR offset;
	offset.primitiveCount = geoInfo.maxPrimitiveCount;
	offset.firstVertex = 0;
	offset.primitiveOffset = 0;
	offset.transformOffset = 0;
	auto poffset = &offset;

	commandBuffer.buildAccelerationStructureKHR(1, &buildinfo, &poffset, extensionDispatcher);

	endSingleTimeCommands(commandBuffer);

	vk::AccelerationStructureDeviceAddressInfoKHR handleinfo;
	handleinfo.accelerationStructure = mesh.accelStructure.acceleration_structure;
	

	mesh.accelStructure.handle = device.getAccelerationStructureAddressKHR(handleinfo, extensionDispatcher);

	destroyBuffer(scratchBuffer);	
}

vk::DeviceAddress VulkanEngine::get_buffer_adress(const AllocatedBuffer& buffer)
{
	vk::BufferDeviceAddressInfo bufferinfo;
	bufferinfo.buffer = buffer.buffer;

	return device.getBufferAddressKHR(bufferinfo, extensionDispatcher);
}

ShaderEffect* VulkanEngine::build_shader_effect(std::vector<const char*> shader_paths, const char* effect_name)
{
	ShaderEffect* effect;
	if (!doesResourceExist<ShaderEffectHandle>(effect_name)) {

		ShaderEffectHandle newShader;
		newShader.handle = new ShaderEffect();

		for (auto s : shader_paths) {
			newShader.handle->add_shader_from_file(s);
		}
		

		newShader.handle->build_effect(device);

		effect = newShader.handle;

		createResource<ShaderEffectHandle>(effect_name, newShader);
	}
	else
	{
		effect = getResource<ShaderEffectHandle>(effect_name).handle;
	}	

	return effect;
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

	gbufferPipelineBuilder->data.vertexInputInfo = VkPipelineInitializers::build_empty_vertex_input();//Vertex::getPipelineCreateInfo();
	gbufferPipelineBuilder->data.inputAssembly = VkPipelineInitializers::build_input_assembly(vk::PrimitiveTopology::eTriangleList);
	gbufferPipelineBuilder->data.viewport = VkPipelineInitializers::build_viewport(swapChainExtent.width, swapChainExtent.height);
	gbufferPipelineBuilder->data.scissor = VkPipelineInitializers::build_rect2d(0, 0, swapChainExtent.width, swapChainExtent.height);
	gbufferPipelineBuilder->data.depthStencil = VkPipelineInitializers::build_depth_stencil(true, true,vk::CompareOp::eGreaterOrEqual);
	gbufferPipelineBuilder->data.rasterizer = VkPipelineInitializers::build_rasterizer();
	gbufferPipelineBuilder->data.multisampling = VkPipelineInitializers::build_multisampling();
	//2 oclor attachments
	gbufferPipelineBuilder->data.colorAttachmentStates.push_back(VkPipelineInitializers::build_color_blend_attachment_state());
	gbufferPipelineBuilder->data.colorAttachmentStates.push_back(VkPipelineInitializers::build_color_blend_attachment_state());

	gbufferPipelineBuilder->data.colorAttachmentStates.push_back(VkPipelineInitializers::build_color_blend_attachment_state());

	gbufferPipelineBuilder->data.dynamicStates = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
		vk::DynamicState::eDepthBias
	};

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

uint32_t VulkanEngine::findMemoryType(uint32_t typeFilter,const vk::MemoryPropertyFlags& properties) {

	vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");

}

void VulkanEngine::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, VmaMemoryUsage vmaUsage, AllocatedBuffer& allocatedbuffer)
{
	vk::BufferCreateInfo bufferInfo;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = vk::SharingMode::eExclusive;

	VkBufferCreateInfo vkbinfo = bufferInfo;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = vmaUsage;
	vmaallocInfo.requiredFlags = VkMemoryPropertyFlags(properties);
	//vmaallocInfo.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;
	VkBuffer vkbuffer;
	VmaAllocation allocation;
	vk::Result result = vk::Result(vmaCreateBuffer(allocator, &vkbinfo, &vmaallocInfo, &vkbuffer, &allocation, nullptr));

	assert(result == vk::Result::eSuccess);
	allocatedbuffer.buffer = vkbuffer;
	allocatedbuffer.allocation = allocation;

	//want this with adress
	if (vk::BufferUsageFlagBits::eShaderDeviceAddress & usage) {
		allocatedbuffer.address = get_buffer_adress(allocatedbuffer);
	}
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

void VulkanEngine::create_uniform_buffers()
{
	vk::DeviceSize UBOSize = (sizeof(UniformBufferObject));

	shadowDataBuffers.resize(swapChainImages.size());
	cameraDataBuffers.resize(swapChainImages.size());
	sceneParamBuffers.resize(swapChainImages.size());
	object_buffers.resize(swapChainImages.size());
	shadow_instance_buffers.resize(swapChainImages.size());
	//uniformBuffersMemory.resize(swapChainImages.size());

	for (size_t i = 0; i < swapChainImages.size(); i++) {
		//cam buffer
		createBuffer(sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_CPU_TO_GPU, cameraDataBuffers[i]);
	
		//shadow buffer
		createBuffer(sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_CPU_TO_GPU, shadowDataBuffers[i]);


		//scene data buffer
		createBuffer(sizeof(GPUSceneParams), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_CPU_TO_GPU, sceneParamBuffers[i]);

		//mesh transform buffer
		createBuffer(sizeof(GpuObjectData) * 10000, vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_CPU_TO_GPU, object_buffers[i]);
	
		//shadow instance id buffer
		createBuffer(sizeof(int32_t) * 10000, vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_CPU_TO_GPU, shadow_instance_buffers[i]);

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
	renderPassInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
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
		ZoneScopedNC("Real Fence", tracy::Color::Yellow);
		device.waitForFences(1, &inFlightFences[currentFrameIndex], VK_TRUE, 100000000);
		device.resetFences(1, &inFlightFences[currentFrameIndex]);

	}
	{
		ZoneScopedNC("WaitFences", tracy::Color::Red);

		if (globalFrameNumber == 0) {

			vk::SemaphoreSignalInfoKHR signal;
			signal.semaphore = frameTimelineSemaphore;
			signal.value = current_frame_timeline_value();
			device.signalSemaphoreKHR(signal, extensionDispatcher);
		}

		uint64_t waitValue = last_frame_timeline_value();

		vk::SemaphoreWaitInfoKHR waitInfo;
		waitInfo.flags = vk::SemaphoreWaitFlagBitsKHR{};
		waitInfo.semaphoreCount = 1;
		waitInfo.pSemaphores = &frameTimelineSemaphore;
		waitInfo.pValues = &waitValue;
		//std::cout << "cpu pre pass: " << globalFrameNumber << " - " << device.getSemaphoreCounterValueKHR(frameTimelineSemaphore, extensionDispatcher) << std::endl;

		//try
		{
			device.waitSemaphoresKHR(waitInfo, 300000000, extensionDispatcher);
		}
		//catch (vk::SystemError &e)
		//{
		//	std::cout << e.what() << std::endl;
		//}
		
		
		//std::cout << "cpu frame pass: " << globalFrameNumber << " - " << device.getSemaphoreCounterValueKHR(frameTimelineSemaphore, extensionDispatcher) << std::endl;

		//vkWaitSemaphoresKHR(device, (VkSemaphoreWaitInfoKHR*)&waitInfo, UINT64_MAX);
	}
	//{
	//	ZoneScopedNC("Real Fence", tracy::Color::Yellow);
	//	device.waitForFences(1, &inFlightFences[currentFrameIndex], VK_TRUE, 30000000);
	//	device.resetFences(1, &inFlightFences[currentFrameIndex]);
	//	
	//}
	{
		ZoneScopedNC("Reset descriptor pool", tracy::Color::Orange);
		descriptorMegapool.set_frame(currentFrameIndex);
	}
	vk::ResultValue<uint32_t> imageResult = vk::ResultValue(vk::Result::eSuccess, 0u);
	{
		ZoneScopedNC("Aquire next image", tracy::Color::Orange);
		imageResult = device.acquireNextImageKHR(swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphores[currentFrameIndex], nullptr);

		if (imageResult.result == vk::Result::eErrorOutOfDateKHR) {
			recreate_swapchain();
			return;
		}
	}


	uint32_t imageIndex = imageResult.value;
	//std::cout << "swapchain image is " << imageIndex << "engine index is " << currentFrameIndex << std::endl;


	update_uniform_buffer(currentFrameIndex/*imageIndex*/);

	eng_stats.drawcalls = 0;

	vk::CommandBuffer cmd = commandBuffers.get(globalFrameNumber);

	begin_frame_command_buffer(cmd);
	tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);

	{
		ZoneScopedNC("RenderSort", tracy::Color::Violet);
		//render_registry.sort<RenderMeshComponent>([](const RenderMeshComponent& A, const RenderMeshComponent& B) {
		//	return A.mesh_resource_entity < B.mesh_resource_entity;
		//	}, entt::std_sort{});
	}

	{
		ZoneScopedNC("Rendergraph", tracy::Color::Yellow);
		render_graph.execute(cmd);
	}


	{
		ZoneScopedNC("Begin frame renderpass", tracy::Color::Grey);
		start_frame_renderpass(cmd, swapChainFramebuffers[imageIndex]);
	}

	{
		VkCommandBuffer commandbuffer = VkCommandBuffer(cmd);
		TracyVkZone(profilercontext, commandbuffer, "All Frame");
		{
			ZoneScopedNC("Final blit", tracy::Color::Grey);
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

			setBuilder.bind_image(0, 0, render_graph.get_image_descriptor(DisplayImage));

			std::array<vk::DescriptorSet, 2> descriptors;
			descriptors[0] = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);

			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, blit->effect->build_pipeline_layout(device), 0, 1, &descriptors[0], 0, nullptr);

			cmd.draw(3, 1, 0, 0);
		}
		{

			ZoneScopedNC("Imgui Update", tracy::Color::Grey);
			TracyVkZone(profilercontext, commandbuffer, "Imgui pass");
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
		}

	}
	{
		ZoneScopedNC("End frame renderpass", tracy::Color::Grey);
		end_frame_command_buffer(cmd);
	

	vk::Semaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrameIndex],  frameTimelineSemaphore };
	{
		const uint64_t waitValue3[] = {
			current_frame_timeline_value(98),current_frame_timeline_value(98)
		};
		const uint64_t signalValues[] = {
			current_frame_timeline_value(100),current_frame_timeline_value(100)
		};

		vk::TimelineSemaphoreSubmitInfo timelineInfo3;		
		timelineInfo3.waitSemaphoreValueCount = 2;
		timelineInfo3.pWaitSemaphoreValues = waitValue3;
		timelineInfo3.signalSemaphoreValueCount = 2;
		timelineInfo3.pSignalSemaphoreValues = signalValues;

		vk::SubmitInfo submitInfo;


		vk::Semaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrameIndex],frameTimelineSemaphore };
		vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput,vk::PipelineStageFlagBits::eColorAttachmentOutput };

		submitInfo.waitSemaphoreCount = 2;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmd;
		submitInfo.signalSemaphoreCount = 2;
		submitInfo.pSignalSemaphores = signalSemaphores;
		//submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrameIndex];
		//submitInfo.pNext = nullptr;
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
}

uint64_t VulkanEngine::last_frame_timeline_value()
{
	uint64_t waitValue = (globalFrameNumber - MAX_FRAMES_IN_FLIGHT) * 100;
	if (globalFrameNumber < MAX_FRAMES_IN_FLIGHT) {
		return 0;
	}
	else {
		return waitValue + 1000;
	}
}

uint64_t VulkanEngine::current_frame_timeline_value(int pass_id /*= 0*/)
{
	return (globalFrameNumber * 100) + 1000 + pass_id;
}

void set_pipeline_state_depth(int width,int height, const vk::CommandBuffer& cmd, bool bDoBias = true, bool bSetBias = true)
{
	vk::Viewport viewport;
	viewport.height = height;
	viewport.width = width;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	cmd.setViewport(0, viewport);

	vk::Rect2D scissor;
	scissor.extent.width = width;
	scissor.extent.height = height;

	cmd.setScissor(0, scissor);

	if (bSetBias) {
		// Set depth bias (aka "Polygon offset")
				// Required to avoid shadow mapping artefacts

		float depthBiasConstant = bDoBias ? 1.25f : 0.f;
		// Slope depth bias factor, applied depending on polygon's slope
		float depthBiasSlope = bDoBias ? 1.75f : 0.f;

		cmd.setDepthBias(depthBiasConstant, 0, depthBiasSlope);
	}
	
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
	struct ShadowDrawUnit {
		//vk::Pipeline pipeline;
		const AllocatedBuffer* indexBuffer;
		uint32_t object_idx;
		uint32_t index_count;
	};

	struct InstancedDraw {
		uint32_t first_index;
		uint32_t instance_count;
		uint32_t index_count;
		vk::Buffer index_buffer;
	};
	eng_stats.shadow_drawcalls = 0;
	static std::vector<ShadowDrawUnit> drawUnits;
	drawUnits.clear();
	static std::vector<InstancedDraw> instancedDraws;
	instancedDraws.clear();

	tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
	TracyVkZoneC(profilercontext, VkCommandBuffer(cmd), "Shadow pass", tracy::Color::Blue);

	ShaderEffect* effect = getResource<ShaderEffectHandle>("shadowpipeline").handle;
	vk::Pipeline last_pipeline = shadowPipeline;
	vk::PipelineLayout piplayout;

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, shadowPipeline);

	set_pipeline_state_depth(width, height, cmd);

	piplayout = (vk::PipelineLayout)effect->build_pipeline_layout(device);
	last_pipeline = shadowPipeline;

	DescriptorSetBuilder setBuilder{ effect,&descriptorMegapool };

	vk::DescriptorBufferInfo shadowBufferInfo = make_buffer_info<UniformBufferObject>(shadowDataBuffers[currentFrameIndex]);

	vk::DescriptorBufferInfo transformBufferInfo = make_buffer_info(object_buffers[currentFrameIndex].buffer, sizeof(GpuObjectData) * 10000);
	vk::DescriptorBufferInfo instanceBufferInfo = make_buffer_info(shadow_instance_buffers[currentFrameIndex].buffer, sizeof(uint32_t) * 10000);

	setBuilder.bind_buffer("ubo", shadowBufferInfo);
	setBuilder.bind_buffer("MainObjectBuffer", transformBufferInfo);
	setBuilder.bind_buffer("InstanceIDBuffer", instanceBufferInfo);

	std::array<vk::DescriptorSet, 2> descriptors;
	descriptors[0] = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, piplayout, 0, 1, &descriptors[0], 0, nullptr);



	ZoneScopedNC("Shadow Pass", tracy::Color::BlueViolet);
	vk::Buffer LastIndex{};


	render_registry.view<RenderMeshComponent>().each([&](RenderMeshComponent& renderable) {

		const MeshResource& mesh = render_registry.get<MeshResource>(renderable.mesh_resource_entity);

		ShadowDrawUnit unit;
		unit.object_idx = renderable.object_idx;
		unit.indexBuffer = &mesh.indexBuffer;
		unit.index_count = mesh.indices.size();

		drawUnits.push_back(unit);
		});

	std::sort(drawUnits.begin(), drawUnits.end(), [](const ShadowDrawUnit& a, const ShadowDrawUnit& b) {

		return a.indexBuffer->buffer < b.indexBuffer->buffer;
		});


	static std::vector<int32_t> instances;
	instances.clear();
	for (auto& unit : drawUnits) {
		instances.push_back(unit.object_idx);
	}

	InstancedDraw pendingDraw;
	for (int i = 0; i < drawUnits.size(); i++)
	{
		auto& unit = drawUnits[i];
		if (LastIndex != unit.indexBuffer->buffer) {
			if (i != 0) {
				instancedDraws.push_back(pendingDraw);
			}

			pendingDraw.first_index = i;
			pendingDraw.index_buffer = unit.indexBuffer->buffer;
			pendingDraw.index_count = unit.index_count;
			pendingDraw.instance_count = 1;
			LastIndex = unit.indexBuffer->buffer;
		}
		else {
			pendingDraw.instance_count++;
		}
	}
	instancedDraws.push_back(pendingDraw);

	void* matdata = mapBuffer(shadow_instance_buffers[currentFrameIndex]);
	memcpy(matdata, instances.data(), instances.size() * sizeof(int32_t));

	unmapBuffer(shadow_instance_buffers[currentFrameIndex]);

	int binds = 0;
	int draw_idx = 0;
#if 1
	for (auto& draw : instancedDraws) {
		cmd.bindIndexBuffer(draw.index_buffer, 0, vk::IndexType::eUint32);
		cmd.drawIndexed(draw.index_count, draw.instance_count,0 , 0, draw.first_index);
		eng_stats.drawcalls++;
		eng_stats.shadow_drawcalls++;
	}
#else 
	for (auto& unit : drawUnits)
	{
		if (LastIndex != unit.indexBuffer->buffer) {

			vk::DeviceSize offsets[] = { 0 };

			cmd.bindIndexBuffer(unit.indexBuffer->buffer, 0, vk::IndexType::eUint32);

			LastIndex = unit.indexBuffer->buffer;
			binds++;
		}

		cmd.drawIndexed(unit.index_count, 1, 0, 0, draw_idx);//unit.object_idx);
		eng_stats.drawcalls++;
		eng_stats.shadow_drawcalls++;
		draw_idx++;
	}
#endif
	return;
#if 0
	render_registry.view<RenderMeshComponent>().each([&](RenderMeshComponent& renderable) {

		const MeshResource& mesh = render_registry.get<MeshResource>(renderable.mesh_resource_entity);

		bool bShouldBindPipeline = first_render;// ? true : pipeline.pipeline != last_pipeline;

		if (bShouldBindPipeline) {
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, shadowPipeline);

			set_pipeline_state_depth(width,height,  cmd);

			piplayout = (vk::PipelineLayout)effect->build_pipeline_layout(device);
			last_pipeline = shadowPipeline;

			DescriptorSetBuilder setBuilder{ effect,&descriptorMegapool };		

			vk::DescriptorBufferInfo shadowBufferInfo = make_buffer_info<UniformBufferObject>(shadowDataBuffers[currentFrameIndex]);

			vk::DescriptorBufferInfo transformBufferInfo = make_buffer_info(object_buffers[currentFrameIndex].buffer, sizeof(GpuObjectData) * 10000);

			setBuilder.bind_buffer("ubo", shadowBufferInfo);
			setBuilder.bind_buffer("MainObjectBuffer", transformBufferInfo);

			std::array<vk::DescriptorSet, 2> descriptors;
			descriptors[0] = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);			

			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, piplayout, 0, 1, &descriptors[0], 0, nullptr);
		}

		if (LastMesh != renderable.mesh_resource_entity) {
			vk::Buffer meshbuffers[] = { mesh.vertexBuffer.buffer };
			vk::DeviceSize offsets[] = { 0 };
			//cmd.bindVertexBuffers(0, 1, meshbuffers, offsets);

			cmd.bindIndexBuffer(mesh.indexBuffer.buffer, 0, vk::IndexType::eUint32);

			LastMesh = renderable.mesh_resource_entity;
		}		
		
		cmd.drawIndexed(static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, renderable.object_idx);
		eng_stats.drawcalls++;
		
		first_render = false;
	});

#endif

#if 0
	int copyidx = 0;
	std::vector<GpuObjectData> object_matrices;

	object_matrices.resize(render_registry.capacity<RenderMeshComponent>());

	render_registry.group<RenderMeshComponent, TransformComponent, ObjectBounds>().each([&](EntityID id, RenderMeshComponent& renderable, const TransformComponent& transform, ObjectBounds& bounds) {

		object_matrices[copyidx].model_matrix = transform.model;


		const MeshResource& mesh = render_registry.get<MeshResource>(renderable.mesh_resource_entity);
		object_matrices[copyidx].vertex_buffer_adress = mesh.vertexBuffer.address;//get_buffer_adress(mesh.vertexBuffer);


		renderable.object_idx = copyidx;
		copyidx++;
		});

	if (copyidx > 0) {
		ZoneScopedN("Uniform copy");

		void* matdata = mapBuffer(object_buffers[currentImage]);
		memcpy(matdata, object_matrices.data(), object_matrices.size() * sizeof(GpuObjectData));

		unmapBuffer(object_buffers[currentImage]);
	}
#endif
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

void VulkanEngine::render_ssao_compute(const vk::CommandBuffer& cmd)
{
	tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
	TracyVkZoneC(profilercontext, VkCommandBuffer(cmd), "SSAO pass Compute", tracy::Color::Orange);

	ZoneScopedNC("SSAO Pass Compute", tracy::Color::BlueViolet);

#ifdef RTX_ON

	PipelineResource ssaopip = getResource<PipelineResource>("ray_shadow");//("pipeline_ssao_comp");

#else
	PipelineResource ssaopip = getResource<PipelineResource>("pipeline_ssao_comp");
#endif
	ShaderEffect* effect = ssaopip.effect;

	VkPipelineLayout layout = effect->build_pipeline_layout((VkDevice)device);

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, ssaopip.pipeline);

	DescriptorSetBuilder setBuilder{ effect,&descriptorMegapool };

	vk::DescriptorBufferInfo camBufferInfo = make_buffer_info<UniformBufferObject>(cameraDataBuffers[currentFrameIndex]);
	vk::DescriptorBufferInfo ssaoSamplesbuffer = make_buffer_info(ssaoSamples.buffer, sizeof(glm::vec4) * SSAO_KERNEL_SIZE);



	setBuilder.bind_buffer("ubo", camBufferInfo);
	


	autobinder->fill_descriptor(&setBuilder);

	//vk::DescriptorBufferInfo raySceneBufferInfo = make_buffer_info<UniformBufferObject>(RaySceneBuffer);
#ifdef RTX_ON
	vk::WriteDescriptorSetAccelerationStructureKHR accel;
	accel.accelerationStructureCount = 1;
	accel.pAccelerationStructures = (vk::AccelerationStructureKHR*)&top_level_acceleration_structure.acceleration_structure;
	setBuilder.bind_raystructure(0, 7, accel);
#else
	setBuilder.bind_buffer("uboSSAOKernel", ssaoSamplesbuffer);
#endif

	setBuilder.bind_image(0, 3, render_graph.get_image_descriptor("ssao_pre"),true);

	build_and_bind_descriptors(setBuilder, 0, cmd);

	//std::array<vk::DescriptorSet, 2> descriptors;
	//descriptors[0] = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);
	//
	//cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0, 1, &descriptors[0], 0, nullptr);


	int width = render_graph.graph_attachments["ssao_pre"].real_width;
	int height = render_graph.graph_attachments["ssao_pre"].real_height;
#ifdef RTX_ON
	float eng_time = rand();
	cmd.pushConstants(layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(float), &eng_time);
#endif
	cmd.dispatch(width / 32 +1, height / 32 + 1, 1);
	//cmd.draw(3, 1, 0, 0);
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



	set_pipeline_state_depth(width, height, cmd, false,false);

	DescriptorSetBuilder setBuilder{ effect,&descriptorMegapool };	
	

	setBuilder.bind_image(0, 0, render_graph.get_image_descriptor("ssao_pre"));
	setBuilder.bind_image(0, 1, render_graph.get_image_descriptor("gbuf_pos"));
	setBuilder.bind_image(0, 2, render_graph.get_image_descriptor("gbuf_normal"));

	std::array<vk::DescriptorSet, 2> descriptors;
	descriptors[0] = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, 1, &descriptors[0], 0, nullptr);

	//render_graph.get_image_descriptor("gbuf_pos")

	glm::vec4 blur_dir;
	blur_dir.x = (1.f / (float)width/*swapChainExtent.width*/); //* 2.f;
	blur_dir.y = (1.f / (float)height/*swapChainExtent.height*/);// * 2.f; //0;
	blur_dir.z = sceneParameters.ssao_roughness;
	blur_dir.w = sceneParameters.kernel_width;
	cmd.pushConstants(layout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(glm::vec4), &blur_dir);

	cmd.draw(3, 1, 0, 0);
}


void VulkanEngine::render_ssao_taa(const vk::CommandBuffer& cmd, int height, int width)
{
	tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
	TracyVkZoneC(profilercontext, VkCommandBuffer(cmd), "SSAO taa", tracy::Color::Red);

	ZoneScopedNC("SSAO Blur X", tracy::Color::BlueViolet);

	PipelineResource ssaopip = getResource<PipelineResource>("pipeline_ssao_taa");

	ShaderEffect* effect = ssaopip.effect;

	VkPipelineLayout layout = effect->build_pipeline_layout((VkDevice)device);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ssaopip.pipeline);

	set_pipeline_state_depth(width, height, cmd, false, false);

	DescriptorSetBuilder setBuilder{ effect,&descriptorMegapool };

	//fill_descriptor
	autobinder->fill_descriptor(&setBuilder);	

	build_and_bind_descriptors(setBuilder, 0, cmd);

	std::array<glm::mat4, 4> push_matrices;
	push_matrices[0] = LastFrameMatrices.proj * LastFrameMatrices.view;
	push_matrices[1] = glm::inverse( MainMatrices.proj * MainMatrices.view);
	push_matrices[2] = glm::inverse(LastFrameMatrices.proj * LastFrameMatrices.view);
	push_matrices[3] = MainMatrices.inv_view;
	cmd.pushConstants(layout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(glm::mat4)*4, &push_matrices[0]);

	cmd.draw(3, 1, 0, 0);
}

void VulkanEngine::build_and_bind_descriptors(DescriptorSetBuilder& setBuilder,int set ,const vk::CommandBuffer& cmd)
{
	VkPipelineLayout layout = setBuilder.effect->build_pipeline_layout(device);
	
	vk::DescriptorSet descriptor = setBuilder.build_descriptor(set, DescriptorLifetime::PerFrame);

	vk::PipelineBindPoint bindPoint = (vk::PipelineBindPoint)setBuilder.effect->get_bind_point();

	cmd.bindDescriptorSets(bindPoint, layout, set, 1, &descriptor, 0, nullptr);
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


	set_pipeline_state_depth(width, height, cmd, false, false);

	DescriptorSetBuilder setBuilder{ effect,&descriptorMegapool };



	setBuilder.bind_image(0, 0, render_graph.get_image_descriptor("ssao_mid"));
	setBuilder.bind_image(0, 1, render_graph.get_image_descriptor("gbuf_pos"));


	build_and_bind_descriptors(setBuilder, 0, cmd);
	

	glm::vec4 blur_dir;
	blur_dir.y = (1.f / (float)height/*swapChainExtent.height*/)*2.f;
	blur_dir.x = 0;
	blur_dir.z = sceneParameters.ssao_roughness;
	blur_dir.w = sceneParameters.kernel_width;
	cmd.pushConstants(layout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(float) * 4, &blur_dir);

	cmd.draw(3, 1, 0, 0);
}


void VulkanEngine::render_ssao_flip(const vk::CommandBuffer& cmd, int height, int width)
{
	ZoneScopedNC("SSAO Blit", tracy::Color::Grey);

	PipelineResource ssaopip = getResource<PipelineResource>("pipeline_ssao_flip");

	ShaderEffect* effect = ssaopip.effect;

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ssaopip.pipeline);


	set_pipeline_state_depth(width, height, cmd, false, false);

	DescriptorSetBuilder setBuilder{ effect,&descriptorMegapool };

	setBuilder.bind_image(0, 0, render_graph.get_image_descriptor("ssao_post"));
	setBuilder.bind_image(0, 1, render_graph.get_image_descriptor("gbuf_pos"));


	build_and_bind_descriptors(setBuilder, 0, cmd);

	cmd.draw(3, 1, 0, 0);
}

void VulkanEngine::render_ssao_blur_compute(const vk::CommandBuffer& cmd, const char* image_source, const char* image_target, glm::vec2 blur_direction)
{	
	int width = render_graph.graph_attachments[image_target].real_width;
	int height = render_graph.graph_attachments[image_target].real_height;


	tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
	TracyVkZoneC(profilercontext, VkCommandBuffer(cmd), "SSAO blurcompute", tracy::Color::Red);
	ZoneScopedNC("SSAO Blur Y", tracy::Color::BlueViolet);

	PipelineResource ssaopip = getResource<PipelineResource>("pipeline_ssao_blur_comp");

	ShaderEffect* effect = ssaopip.effect;

	VkPipelineLayout layout = effect->build_pipeline_layout((VkDevice)device);

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, ssaopip.pipeline);

	DescriptorSetBuilder setBuilder{ effect,&descriptorMegapool };

	setBuilder.bind_image(0, 0, render_graph.get_image_descriptor(image_source));
	setBuilder.bind_image(0, 1, render_graph.get_image_descriptor("gbuf_pos"));
	setBuilder.bind_image(0, 3, render_graph.get_image_descriptor(image_target),true);

	build_and_bind_descriptors(setBuilder, 0, cmd);

	glm::vec4 blur_dir;
	blur_dir.y = (1.f / height) * blur_direction.y;
	blur_dir.x = (1.f / width)*blur_direction.x;
	blur_dir.z = sceneParameters.ssao_roughness;
	blur_dir.w = sceneParameters.kernel_width;
	cmd.pushConstants(layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(float) * 4, &blur_dir);

	cmd.dispatch((width / 32) + 1, (height / 32) + 1, 1);
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


			auto pass = render_graph.get_pass("GBuffer");

			set_pipeline_state_depth(pass->render_width, pass->render_height, cmd,false);
			piplayout = (vk::PipelineLayout)unit.effect->build_pipeline_layout(device);
			last_pipeline = unit.pipeline;

			DescriptorSetBuilder setBuilder{ unit.effect,&descriptorMegapool };

			vk::DescriptorBufferInfo camBufferInfo;
			if (config_parameters.ShadowView)
			//if(globalFrameNumber % 2)
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

		    vk::DescriptorBufferInfo transformBufferInfo = make_buffer_info(object_buffers[currentFrameIndex].buffer, sizeof(GpuObjectData) * 10000);

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
		
		if (last_index_buffer != unit.indexBuffer) {
			
			cmd.bindIndexBuffer(unit.indexBuffer, 0, vk::IndexType::eUint32);

			last_index_buffer = unit.indexBuffer;
		}

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, piplayout, 2, 1, &unit.material_set, 0, nullptr);

		

		cmd.drawIndexed(static_cast<uint32_t>(unit.index_count), 1, 0, 0, unit.object_idx);
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

	
	eng_stats.gbuffer_drawcalls = 0;
	for (const DrawUnit& unit : drawables) {

		const bool bShouldBindPipeline = unit.pipeline != last_pipeline;

		if (bShouldBindPipeline) {
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, unit.pipeline);

			auto pass = render_graph.get_pass("GBuffer");

			set_pipeline_state_depth(pass->render_width, pass->render_height, cmd, false);
			//set_pipeline_state_depth(swapChainExtent.width, swapChainExtent.height, cmd, false);
			piplayout = (vk::PipelineLayout)unit.effect->build_pipeline_layout(device);
			last_pipeline = unit.pipeline;

			DescriptorSetBuilder setBuilder{ unit.effect,&descriptorMegapool };

			vk::DescriptorBufferInfo camBufferInfo;
			if (config_parameters.ShadowView)
			//if (globalFrameNumber % 2)
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
			transformBufferInfo.range = sizeof(GpuObjectData) * 10000;
						
			setBuilder.bind_buffer("ubo", camBufferInfo);			
			setBuilder.bind_buffer("MainObjectBuffer", transformBufferInfo);

			build_and_bind_descriptors(setBuilder, 0, cmd);
		}

		if (last_index_buffer != unit.indexBuffer) {

			cmd.bindIndexBuffer(unit.indexBuffer, 0, vk::IndexType::eUint32);

			last_index_buffer = unit.indexBuffer;
		}		

		cmd.drawIndexed(static_cast<uint32_t>(unit.index_count), 1, 0, 0, unit.object_idx);
		eng_stats.drawcalls++;
		eng_stats.gbuffer_drawcalls++;
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

	auto pass = render_graph.get_pass("GBuffer");
	//set_pipeline_state_depth(pass->render_width, pass->render_height, cmd, false);

	float render_height = pass->render_height;
	float render_width = pass->render_width;

	ubo.proj = glm::perspective(glm::radians(config_parameters.fov), render_width / render_height,  50000.0f, 0.1f);
	glm::mat4 revproj = glm::perspective(glm::radians(config_parameters.fov), render_width / render_height, 0.1f, 50000.0f);
	ubo.proj[1][1] *= -1;
	if (!config_parameters.PlayerCam) {

		ubo.view = glm::lookAt(eye, glm::vec3(0.0f, 400.0f, 0.0f), config_parameters.CamUp);
		
		ubo.eye = glm::vec4(eye, 0.0f);
		//invert projection matrix couse glm is inverted y compared to vulkan
		

		mainCam.eyeLoc = eye;
		mainCam.eyeDir = glm::normalize(eye - glm::vec3(0.0f, 400.0f, 0.0f));
	}
	else {

		//float rx = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
		//float ry = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
		//
		//(&ubo.proj[0][0])[8] = rx*2 / render_width;
		//(&ubo.proj[0][0])[9] = rx * 2 / render_height;

		

		glm::vec3 side = glm::cross(playerCam.camera_location, playerCam.camera_up);

		glm::vec3 offset = glm::vec3(0); //playerCam.camera_up * ry + side * rx;

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

	//big hack, this is last frame
	ubo.inv_model = MainMatrices.proj * MainMatrices.view;  ///glm::inverse(ubo.model);
	ubo.inv_view = glm::inverse(ubo.view);
	ubo.inv_proj = glm::inverse(ubo.proj);

	shadowubo.inv_model = glm::inverse(shadowubo.model);
	shadowubo.inv_view = glm::inverse(shadowubo.view);
	shadowubo.inv_proj = glm::inverse(shadowubo.proj);


	void* data = mapBuffer(cameraDataBuffers[currentImage]);
	//auto alignUbo = align_dynamic_descriptor(sizeof(ubo));
	char* dt = (char*)data;// + alignUbo; ;
	
	
	memcpy(dt, &ubo, sizeof(UniformBufferObject) );
	//keep matrices
	memcpy(&LastFrameMatrices, &MainMatrices, sizeof(UniformBufferObject));
	memcpy(&MainMatrices, &ubo, sizeof(UniformBufferObject));

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
		std::vector<GpuObjectData> object_matrices;

		object_matrices.resize(render_registry.capacity<RenderMeshComponent>());
		
		render_registry.group<RenderMeshComponent, TransformComponent, ObjectBounds>().each([&](EntityID id, RenderMeshComponent& renderable, const TransformComponent& transform, ObjectBounds& bounds) {

			object_matrices[copyidx].model_matrix = transform.model;		

			
			const MeshResource& mesh = render_registry.get<MeshResource>(renderable.mesh_resource_entity);
			object_matrices[copyidx].vertex_buffer_adress = mesh.vertexBuffer.address;//get_buffer_adress(mesh.vertexBuffer);
			

			renderable.object_idx = copyidx;
			copyidx++;
		});

	if (copyidx > 0) {
		ZoneScopedN("Uniform copy");

		void* matdata = mapBuffer(object_buffers[currentImage]);
		memcpy(matdata, object_matrices.data(), object_matrices.size() * sizeof(GpuObjectData));

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

	vk::SemaphoreTypeCreateInfo timelineSemaphoreInfo;
	timelineSemaphoreInfo.semaphoreType = vk::SemaphoreType::eTimeline;
	timelineSemaphoreInfo.initialValue = 0;

	vk::SemaphoreCreateInfo timeline;
	timeline.setPNext(&timelineSemaphoreInfo);

	frameTimelineSemaphore = device.createSemaphore(timeline);
	//passTimelineSemaphore = device.createSemaphore(timeline);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		imageAvailableSemaphores[i] = device.createSemaphore(semaphoreInfo);
		renderFinishedSemaphores[i] = device.createSemaphore(semaphoreInfo);
		inFlightFences[i] = device.createFence(fenceInfo);

		name_object(imageAvailableSemaphores[i], "Image Availible Semaphore");
		name_object(renderFinishedSemaphores[i], "Render Finished Semaphore");		
	}

	name_object(frameTimelineSemaphore, "Timeline Semaphore");

	device.resetFences(inFlightFences.size(), inFlightFences.data());
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

	if (resource->computePipelineBuilder) {
		resource->pipeline =  resource->computePipelineBuilder->build_pipeline(device, resource->effect);
	}
	else {
		if (resource->renderPassName != "") {

			//RenderPass &pass = graph.pass_definitions[resource->renderPassName];

			resource->pipeline = resource->pipelineBuilder->build_pipeline(device, render_graph.get_pass(resource->renderPassName)->built_pass,
				0, resource->effect);
		}
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
	createBuffer(vertexBufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_UNKNOWN, vertex_staging_allocation);

	AllocatedBuffer index_staging_allocation;
	createBuffer(indexBufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_UNKNOWN, index_staging_allocation);

	//copy vertex data
	void* data;
	vmaMapMemory(allocator, vertex_staging_allocation.allocation, &data);

	memcpy(data, newMesh.vertices.data(), (size_t)vertexBufferSize);

	vmaUnmapMemory(allocator, vertex_staging_allocation.allocation);

	//copy index data
	vmaMapMemory(allocator, index_staging_allocation.allocation, &data);

	memcpy(data, newMesh.indices.data(), (size_t)indexBufferSize);

	vmaUnmapMemory(allocator, index_staging_allocation.allocation);


	
	createBuffer(vertexBufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_UNKNOWN, newMesh.vertexBuffer);//vertexBuffer, vertexBufferMemory);
	
	copyBuffer(vertex_staging_allocation.buffer, newMesh.vertexBuffer.buffer, vertexBufferSize);

	vmaDestroyBuffer(allocator, vertex_staging_allocation.buffer, vertex_staging_allocation.allocation);
		
	

	//AllocatedBuffer index_buffer;
	createBuffer(indexBufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_UNKNOWN, newMesh.indexBuffer);

	copyBuffer(index_staging_allocation.buffer, newMesh.indexBuffer.buffer, indexBufferSize);

	vmaDestroyBuffer(allocator, index_staging_allocation.buffer, index_staging_allocation.allocation);

	EntityID id = createResource(modelName.c_str(), newMesh);

	return id;
}
uint32_t hash_index_buffer(std::vector<uint32_t>& indices) {
	return murmurhash((const char*)indices.data(), indices.size() * sizeof(uint32_t), 0);
}
EntityID VulkanEngine::load_assimp_mesh(aiMesh* mesh)
{
	MeshResource newMesh;

	glm::vec3 bmin = { mesh->mAABB.mMin.x,mesh->mAABB.mMin.y,mesh->mAABB.mMin.z };
	glm::vec3 bmax = { mesh->mAABB.mMax.x,mesh->mAABB.mMax.y,mesh->mAABB.mMax.z };

	ObjectBounds bounds;

	bounds.center_rad = glm::vec4((bmax + bmin) / 2.f, 1.f);

	bounds.extent = glm::abs(glm::vec4((bmax - bmin) / 2.f, 0.f));
	bounds.center_rad.w = glm::length(bounds.extent);

	newMesh.bounds = bounds;

	newMesh.vertices.clear();
	newMesh.indices.clear();

	newMesh.vertices.reserve(mesh->mNumVertices);
	newMesh.indices.reserve(mesh->mNumFaces * 3);

	for (int vtx = 0; vtx < mesh->mNumVertices; vtx++) {
		Vertex vertex = {};

		vertex.pos = {
			mesh->mVertices[vtx].x,
			mesh->mVertices[vtx].y,
			mesh->mVertices[vtx].z,
			0.0f
		};
		if (mesh->mTextureCoords[0])
		{
			vertex.texCoord = {
			mesh->mTextureCoords[0][vtx].x,
			mesh->mTextureCoords[0][vtx].y,
			mesh->mTextureCoords[0][vtx].x,
			mesh->mTextureCoords[0][vtx].y,
			};
		}
		else {
			vertex.texCoord = {
				0.5f,0.5f,0.5f,0.5f
			};
		}

		vertex.normal = {
			mesh->mNormals[vtx].x,
			mesh->mNormals[vtx].y,
			mesh->mNormals[vtx].z,
			0.0f
		};
		newMesh.vertices.push_back(vertex);
	}

	for (int face = 0; face < mesh->mNumFaces; face++) {
		newMesh.indices.push_back(mesh->mFaces[face].mIndices[0]);
		newMesh.indices.push_back(mesh->mFaces[face].mIndices[1]);
		newMesh.indices.push_back(mesh->mFaces[face].mIndices[2]);
	}

	//hash the indices to find duplicates already loaded
	uint32_t hash;
	{
		ZoneScopedNC("Hash index buffer", tracy::Color::Blue);
		hash = hash_index_buffer(newMesh.indices);

	}

	bool bUploadIndices = false;

	//search in cache
	auto cached = indexCache.IndexBuffers.find(hash);
	if (cached == indexCache.IndexBuffers.end()) {
		bUploadIndices = true;
	}
	else {
		//std::cout << "index cache hit" << std::endl;

		newMesh.indexBuffer = indexCache.IndexBuffers[hash];
	}


	{
		vk::DeviceSize vertexBufferSize = sizeof(newMesh.vertices[0]) * newMesh.vertices.size();


		AllocatedBuffer vertex_staging_allocation;
		createBuffer(vertexBufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_UNKNOWN, vertex_staging_allocation);


		//copy vertex data
		void* data;
		vmaMapMemory(allocator, vertex_staging_allocation.allocation, &data);

		memcpy(data, newMesh.vertices.data(), (size_t)vertexBufferSize);

		vmaUnmapMemory(allocator, vertex_staging_allocation.allocation);

		createBuffer(vertexBufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_UNKNOWN, newMesh.vertexBuffer);//vertexBuffer, vertexBufferMemory);

		copyBuffer(vertex_staging_allocation.buffer, newMesh.vertexBuffer.buffer, vertexBufferSize);

		vmaDestroyBuffer(allocator, vertex_staging_allocation.buffer, vertex_staging_allocation.allocation);
	}

	if(bUploadIndices)
	{
		vk::DeviceSize indexBufferSize = sizeof(newMesh.indices[0]) * newMesh.indices.size();
		AllocatedBuffer index_staging_allocation;
		createBuffer(indexBufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_UNKNOWN, index_staging_allocation);

		//copy index data
		void* data;
		vmaMapMemory(allocator, index_staging_allocation.allocation, &data);

		memcpy(data, newMesh.indices.data(), (size_t)indexBufferSize);

		vmaUnmapMemory(allocator, index_staging_allocation.allocation);
		

		createBuffer(indexBufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_GPU_ONLY, newMesh.indexBuffer);

		copyBuffer(index_staging_allocation.buffer, newMesh.indexBuffer.buffer, indexBufferSize);

		vmaDestroyBuffer(allocator, index_staging_allocation.buffer, index_staging_allocation.allocation);

		//upload to cache
		indexCache.IndexBuffers[hash] = newMesh.indexBuffer;
	}
	
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
	ZoneScopedNC("Full Scene Load", tracy::Color::Blue);
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
	}


	{
		ZoneScopedNC("Texture request bulk load", tracy::Color::Yellow);
		//tex_loader->load_all_textures(loader, scenepath);//flush_requests();

		//load_textures_bulk(textureLoadRequests.data(), textureLoadRequests.size());
	}


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

		std::vector<sp::DbMaterial> dbmaterials;
		{
			ZoneScopedNC("Database fetch material metadata", tracy::Color::Yellow1);
			loader->load_materials_from_db("", dbmaterials);
			materials.resize(dbmaterials.size());
		}
		for (int i = 0; i < dbmaterials.size(); i++)
		{
			SimpleMaterial newMat;
			newMat.textureIDs = blank_textures;			

			for (auto t : dbmaterials[i].textures) {
				int slot = 0;
				aiTextureType type = (aiTextureType)t.texture_slot;
				switch (type) {
				case aiTextureType_DIFFUSE:
					slot = 0; break;
				case aiTextureType_NORMALS:
					slot = 1; break;
				case aiTextureType_SPECULAR:
					slot = 2; break;
				case aiTextureType_UNKNOWN:
					slot = 3; break;
				case aiTextureType_EMISSION_COLOR:
					slot = 4; break;
				case	aiTextureType_BASE_COLOR:
					slot = 5; break;
				case aiTextureType_DIFFUSE_ROUGHNESS:
					slot = 6; break;
				}
				newMat.textureIDs[slot] = resourceMap[t.texture_name];
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
		ZoneScopedNC("Mesh creation", tracy::Color::Blue);

		for (int i = 0; i < scene->mNumMeshes; i++)
		{
			loaded_meshes.push_back(load_assimp_mesh(scene->mMeshes[i]));
		}
	}
	{
		ZoneScopedNC("Descriptor creation", tracy::Color::Magenta);

		for (int i = 0; i < scene->mNumMeshes; i++)
		{
			//loaded_meshes.push_back(load_assimp_mesh(scene->mMeshes[i]));

			try {
				std::string matname = scene->mMaterials[scene->mMeshes[i]->mMaterialIndex]->GetName().C_Str();
			
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
