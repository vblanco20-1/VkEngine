#include <vulkan_render.h>
#include <vulkan_textures.h>
#include <shader_processor.h>
#include <vulkan_descriptors.h>

#include <glm/gtx/vector_angle.hpp> 
#include "vk_flags.h"
#include "command_encoder.h"
void inline set_pipeline_state_depth(int width, int height, const vk::CommandBuffer& cmd, bool bDoBias = true, bool bSetBias = true)
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

static CommandEncoder* encoder{nullptr};


struct DrawUnitEncoder {
	uint32_t index_count;
	uint32_t object_idx;
	uint32_t index_offset;

	uint64_t pipeline;
	uint64_t material_set;

	uint64_t vertexBuffer;
	uint64_t indexBuffer;
};

void VulkanEngine::RenderMainPass_Other(const vk::CommandBuffer& cmd)
{
	
	TextureResource bluenoise = getResource<TextureResource>(bluenoiseTexture);

	ZoneScopedNC("Main Pass", tracy::Color::BlueViolet);
	EntityID LastMesh = entt::null;

	static std::vector<DrawUnitEncoder> drawables;

	if (encoder == nullptr) { encoder = new CommandEncoder(); };

	drawables.clear();
	drawables.reserve(render_registry.view<RenderMeshComponent>().size());

	render_registry.group<RenderMeshComponent, TransformComponent, ObjectBounds>().each([&](RenderMeshComponent& renderable, TransformComponent& tf, ObjectBounds& bounds) {
		const MeshResource& mesh = render_registry.get<MeshResource>(renderable.mesh_resource_entity);
		const PipelineResource& pipeline = render_registry.get<PipelineResource>(renderable.pass_pipelines[(size_t)MeshPasIndex::MainPass]);
		const DescriptorResource& descriptor = render_registry.get<DescriptorResource>(renderable.pass_descriptors[(size_t)MeshPasIndex::MainPass]);

		bool bVisible = false;

		glm::vec3 bounds_center = glm::vec3(bounds.center_rad);
		glm::vec3 dir = normalize(mainCam.eyeLoc - bounds_center);

		float angle = glm::angle(dir, mainCam.eyeDir);

		glm::vec3 bmin = bounds_center - glm::vec3(bounds.extent);
		glm::vec3 bmax = bounds_center + glm::vec3(bounds.extent);

		bVisible = mainCam.camfrustum.IsBoxVisible(bmin, bmax);

		if (bVisible) {
			DrawUnitEncoder newDrawUnit;

			VkBuffer index = mesh.indexBuffer.buffer;
			VkDescriptorSet matset = descriptor.materialSet;

			newDrawUnit.indexBuffer = reinterpret_cast<uint64_t>(index);
			newDrawUnit.vertexBuffer = 0;//reinterpret_cast<uint64_t>(mesh.vertexBuffer.buffer);
			newDrawUnit.index_count = mesh.indices.size();
			newDrawUnit.material_set = reinterpret_cast<uint64_t>(matset);
			newDrawUnit.object_idx = renderable.object_idx;
			EntityID pipelineID = renderable.pass_pipelines[(size_t)MeshPasIndex::MainPass];
			
			newDrawUnit.pipeline = uint32_t(pipelineID);//pipeline.pipeline;
			//newDrawUnit.effect = pipeline.effect;
			newDrawUnit.index_offset = mesh.index_offset;
			drawables.push_back(newDrawUnit);
		}
	});

	//std::sort(drawables.begin(), drawables.end(), [](const DrawUnit& a, const DrawUnit& b) {
	//	return a.vertexBuffer < b.vertexBuffer;
	//	});

	//convert to indirect
	void* indirect = mapBuffer(mainpass_indirect_buffers[currentFrameIndex]);
	{
		ZoneScopedN("Indirect Upload")
			VkDrawIndexedIndirectCommand* commands = (VkDrawIndexedIndirectCommand*)indirect;

		for (int i = 0; i < drawables.size(); i++) {

			auto draw = drawables[i];
			commands[i].indexCount = draw.index_count;
			commands[i].instanceCount = 1;
			commands[i].firstIndex = draw.index_offset;
			commands[i].vertexOffset = 0;
			commands[i].firstInstance = draw.object_idx;

			//commands[i]. = drawUnits[i].object_idx;
		}
	}
	unmapBuffer(mainpass_indirect_buffers[currentFrameIndex]);


	//for(const DrawUnit& unit : drawables)
	{
		const DrawUnitEncoder& unit = drawables[0];
		const bool bShouldBindPipeline = true;

		auto pass = render_graph.get_pass("GBuffer");
		
		

		const PipelineResource& pipeline = render_registry.get<PipelineResource>(entt::entity{ unit.pipeline });


		DescriptorSetBuilder setBuilder{ pipeline.effect,&descriptorMegapool };

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


		VkDescriptorBufferInfo shadowBufferInfo = make_buffer_info<UniformBufferObject>(shadowDataBuffers[currentFrameIndex]);

		VkDescriptorBufferInfo sceneBufferInfo = make_buffer_info<GPUSceneParams>(sceneParamBuffers[currentFrameIndex]);

		VkDescriptorBufferInfo transformBufferInfo = make_buffer_info(object_buffers[currentFrameIndex].buffer, sizeof(GpuObjectData) * 10000);

		VkDescriptorImageInfo shadowInfo = {};
		shadowInfo.imageLayout = +vkf::ImageLayout::eDepthStencilReadOnlyOptimal;
		shadowInfo.imageView = shadowPass.depth.view;
		shadowInfo.sampler = bluenoise.textureSampler;

		VkDescriptorImageInfo noiseImage = {};
		noiseImage.imageLayout = +vkf::ImageLayout::eShaderReadOnlyOptimal;
		noiseImage.imageView = bluenoise.imageView;
		noiseImage.sampler = bluenoise.textureSampler;

		setBuilder.bind_image_array(0, 10, texCache->all_images.data(), texCache->all_images.size());//all_images.data(),all_images.size());

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

		std::array<VkDescriptorSet, 2> descriptors;
		descriptors[0] = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);
		descriptors[1] = setBuilder.build_descriptor(1, DescriptorLifetime::PerFrame);

		encoder->set_depthbias(0, 0, 0);
		encoder->set_scissor(0, 0, pass->render_width, pass->render_height);
		encoder->set_viewport(0, 0, pass->render_width, pass->render_height, 0, 1);
		encoder->bind_pipeline((unit.pipeline));

		encoder->bind_descriptor_set(0, reinterpret_cast<uint64_t>(descriptors[0]));
		encoder->bind_descriptor_set(1, reinterpret_cast<uint64_t>(descriptors[1]));
		

		encoder->bind_index_buffer(0, unit.indexBuffer);

		
		VkBuffer indirectBuffer = mainpass_indirect_buffers[currentFrameIndex].buffer;

		encoder->draw_indexed_indirect(reinterpret_cast<uint64_t>(indirectBuffer), drawables.size(), 0);	
	}

	{
		tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
		TracyVkZoneC(profilercontext, VkCommandBuffer(cmd), "Forward Pass", tracy::Color::Red);

		ZoneScopedN("Command decoding")

		DecodeCommands(cmd, encoder);		
	}
}



void VulkanEngine::RenderMainPass(const vk::CommandBuffer& cmd)
{
	tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
	TracyVkZoneC(profilercontext, VkCommandBuffer(cmd), "Forward Pass", tracy::Color::Red);
	TextureResource bluenoise = getResource<TextureResource>(bluenoiseTexture);

	ZoneScopedNC("Main Pass", tracy::Color::BlueViolet);
	EntityID LastMesh = entt::null;

	VkPipeline last_pipeline;
	VkPipelineLayout piplayout;
	VkBuffer last_vertex_buffer;
	VkBuffer last_index_buffer;
	bool first_render = true;

	static std::vector<DrawUnit> drawables;

	drawables.clear();
	drawables.reserve(render_registry.view<RenderMeshComponent>().size());

	render_registry.group<RenderMeshComponent, TransformComponent, ObjectBounds>().each([&](RenderMeshComponent& renderable, TransformComponent& tf, ObjectBounds& bounds) {
		const MeshResource& mesh = render_registry.get<MeshResource>(renderable.mesh_resource_entity);
		const PipelineResource& pipeline = render_registry.get<PipelineResource>(renderable.pass_pipelines[(size_t)MeshPasIndex::MainPass]);
		const DescriptorResource& descriptor = render_registry.get<DescriptorResource>(renderable.pass_descriptors[(size_t)MeshPasIndex::MainPass]);

		bool bVisible = false;

		glm::vec3 bounds_center = glm::vec3(bounds.center_rad);
		glm::vec3 dir = normalize(mainCam.eyeLoc - bounds_center);

		float angle = glm::angle(dir, mainCam.eyeDir);

		glm::vec3 bmin = bounds_center - glm::vec3(bounds.extent);
		glm::vec3 bmax = bounds_center + glm::vec3(bounds.extent);

		bVisible = mainCam.camfrustum.IsBoxVisible(bmin, bmax);

		if (bVisible) {
			DrawUnit newDrawUnit;
			newDrawUnit.indexBuffer = mesh.indexBuffer.buffer;
			newDrawUnit.vertexBuffer = mesh.vertexBuffer.buffer;
			newDrawUnit.index_count = mesh.indices.size();
			newDrawUnit.material_set = descriptor.materialSet;
			newDrawUnit.object_idx = renderable.object_idx;
			newDrawUnit.pipeline = pipeline.pipeline;
			newDrawUnit.effect = pipeline.effect;
			newDrawUnit.index_offset = mesh.index_offset;
			drawables.push_back(newDrawUnit);
		}
	});

	std::sort(drawables.begin(), drawables.end(), [](const DrawUnit& a, const DrawUnit& b) {
		return a.vertexBuffer < b.vertexBuffer;
		});

	//convert to indirect
	void* indirect = mapBuffer(mainpass_indirect_buffers[currentFrameIndex]);
	{
		ZoneScopedN("Indirect Upload")
			VkDrawIndexedIndirectCommand* commands = (VkDrawIndexedIndirectCommand*)indirect;

		for (int i = 0; i < drawables.size(); i++) {

			auto draw = drawables[i];
			commands[i].indexCount = draw.index_count;
			commands[i].instanceCount = 1;
			commands[i].firstIndex = draw.index_offset;
			commands[i].vertexOffset = 0;
			commands[i].firstInstance = draw.object_idx;

			//commands[i]. = drawUnits[i].object_idx;
		}
	}
	unmapBuffer(mainpass_indirect_buffers[currentFrameIndex]);


	//for(const DrawUnit& unit : drawables)
	{
		const DrawUnit& unit = drawables[0];
		const bool bShouldBindPipeline = true;//unit.pipeline != last_pipeline;

		if (bShouldBindPipeline) {
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, unit.pipeline);


			auto pass = render_graph.get_pass("GBuffer");

			set_pipeline_state_depth(pass->render_width, pass->render_height, cmd, false);
			piplayout = unit.effect->build_pipeline_layout(device);
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


			VkDescriptorBufferInfo shadowBufferInfo = make_buffer_info<UniformBufferObject>(shadowDataBuffers[currentFrameIndex]);
			
			VkDescriptorBufferInfo sceneBufferInfo = make_buffer_info<GPUSceneParams>(sceneParamBuffers[currentFrameIndex]);
			
			VkDescriptorBufferInfo transformBufferInfo = make_buffer_info(object_buffers[currentFrameIndex].buffer, sizeof(GpuObjectData) * 10000);

			VkDescriptorImageInfo shadowInfo = {};
			shadowInfo.imageLayout = +vkf::ImageLayout::eDepthStencilReadOnlyOptimal;
			shadowInfo.imageView = shadowPass.depth.view;
			shadowInfo.sampler = bluenoise.textureSampler;

			VkDescriptorImageInfo noiseImage = {};
			noiseImage.imageLayout = +vkf::ImageLayout::eShaderReadOnlyOptimal;
			noiseImage.imageView = bluenoise.imageView;
			noiseImage.sampler = bluenoise.textureSampler;

			setBuilder.bind_image_array(0, 10, texCache->all_images.data(), texCache->all_images.size());//all_images.data(),all_images.size());

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

			std::array<VkDescriptorSet, 2> descriptors;
			descriptors[0] = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);
			descriptors[1] = setBuilder.build_descriptor(1, DescriptorLifetime::PerFrame);

			vkCmdBindDescriptorSets(cmd,+vkf::PipelineBindPoint::eGraphics,piplayout, 0, 2, &descriptors[0], 0, nullptr);
		}

		if (last_index_buffer != unit.indexBuffer) {

			cmd.bindIndexBuffer(unit.indexBuffer, 0, vk::IndexType::eUint32);

			last_index_buffer = unit.indexBuffer;
		}

		cmd.drawIndexedIndirect(mainpass_indirect_buffers[currentFrameIndex].buffer, vk::DeviceSize(0), (uint32_t)drawables.size(), sizeof(VkDrawIndexedIndirectCommand));

	
		eng_stats.drawcalls++;

		first_render = false;
	}
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
			newDrawUnit.index_offset = mesh.index_offset;
			drawables.push_back(newDrawUnit);
		}
		});

	std::sort(drawables.begin(), drawables.end(), [](const DrawUnit& a, const DrawUnit& b) {
		return a.vertexBuffer < b.vertexBuffer;
		});


	//convert to indirect
	void* indirect = mapBuffer(gbuffer_indirect_buffers[currentFrameIndex]);
	{
		ZoneScopedN("Indirect Upload")
			VkDrawIndexedIndirectCommand* commands = (VkDrawIndexedIndirectCommand*)indirect;

		for (int i = 0; i < drawables.size(); i++) {

			auto draw = drawables[i];
			commands[i].indexCount = draw.index_count;
			commands[i].instanceCount = 1;
			commands[i].firstIndex = draw.index_offset;
			commands[i].vertexOffset = 0;
			commands[i].firstInstance = draw.object_idx;

			//commands[i]. = drawUnits[i].object_idx;
		}
	}
	unmapBuffer(gbuffer_indirect_buffers[currentFrameIndex]);

	//eng_stats.drawcalls++;
	//eng_stats.shadow_drawcalls++;



	eng_stats.gbuffer_drawcalls = 0;
	//for (const DrawUnit& unit : drawables) {
	//
	//	const bool bShouldBindPipeline = unit.pipeline != last_pipeline;

	if (true)//bShouldBindPipeline) 
	{
		const DrawUnit& unit = drawables[0];
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

	//if (last_index_buffer != unit.indexBuffer) {
	{
		cmd.bindIndexBuffer(megabuffer.buffer, 0, vk::IndexType::eUint32);
		//	cmd.bindIndexBuffer(unit.indexBuffer, 0, vk::IndexType::eUint32);

		//	last_index_buffer = unit.indexBuffer;
	}

	cmd.drawIndexedIndirect(gbuffer_indirect_buffers[currentFrameIndex].buffer, vk::DeviceSize(0), (uint32_t)drawables.size(), sizeof(VkDrawIndexedIndirectCommand));


	//cmd.drawIndexed(static_cast<uint32_t>(unit.index_count), 1, unit.index_offset, 0, unit.object_idx);
	eng_stats.drawcalls++;
	eng_stats.gbuffer_drawcalls++;
	first_render = false;
	//}
}


void VulkanEngine::render_shadow_pass(const vk::CommandBuffer& cmd, int height, int width)
{
	struct ShadowDrawUnit {
		//vk::Pipeline pipeline;
		const AllocatedBuffer* indexBuffer;
		uint32_t object_idx;
		uint32_t index_count;
		uint32_t index_offset;
	};

	struct InstancedDraw {
		uint32_t first_index;
		uint32_t instance_count;
		uint32_t index_count;
		uint32_t index_offset;
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
		unit.index_offset = mesh.index_offset;
		drawUnits.push_back(unit);
		});

	std::sort(drawUnits.begin(), drawUnits.end(), [](const ShadowDrawUnit& a, const ShadowDrawUnit& b) {

		if (a.indexBuffer->buffer == b.indexBuffer->buffer)
		{
			return  a.index_offset < b.index_offset;
		}
		else {

			return a.indexBuffer->buffer < b.indexBuffer->buffer;
		}
		});



	void* matdata = mapBuffer(shadow_instance_buffers[currentFrameIndex]);
	{
		ZoneScopedN("Instance Upload")
			int32_t* data = (int32_t*)matdata;

		for (int i = 0; i < drawUnits.size(); i++) {
			data[i] = drawUnits[i].object_idx;
		}
	}
	unmapBuffer(shadow_instance_buffers[currentFrameIndex]);

	int idx_ofs = -1;
	InstancedDraw pendingDraw;
	for (int i = 0; i < drawUnits.size(); i++)
	{
		auto& unit = drawUnits[i];
		if (LastIndex != unit.indexBuffer->buffer || idx_ofs != unit.index_offset) {
			if (i != 0) {
				instancedDraws.push_back(pendingDraw);
			}

			pendingDraw.first_index = i;
			pendingDraw.index_buffer = unit.indexBuffer->buffer;
			pendingDraw.index_count = unit.index_count;
			pendingDraw.instance_count = 1;
			pendingDraw.index_offset = unit.index_offset;
			LastIndex = unit.indexBuffer->buffer;
			idx_ofs = unit.index_offset;
		}
		else {
			pendingDraw.instance_count++;
		}
	}
	instancedDraws.push_back(pendingDraw);



	int binds = 0;
	int draw_idx = 0;
#if 1
	cmd.bindIndexBuffer(megabuffer.buffer, 0, vk::IndexType::eUint32);

	//convert to indirect
	void* indirect = mapBuffer(shadow_indirect_buffers[currentFrameIndex]);
	{
		ZoneScopedN("Indirect Upload")
			VkDrawIndexedIndirectCommand* commands = (VkDrawIndexedIndirectCommand*)indirect;

		for (int i = 0; i < instancedDraws.size(); i++) {

			auto draw = instancedDraws[i];

			commands[i].indexCount = draw.index_count;
			commands[i].instanceCount = draw.instance_count;
			commands[i].firstIndex = draw.index_offset;
			commands[i].vertexOffset = 0;
			commands[i].firstInstance = draw.first_index;

			//commands[i]. = drawUnits[i].object_idx;
		}
	}
	unmapBuffer(shadow_indirect_buffers[currentFrameIndex]);

	eng_stats.drawcalls++;
	eng_stats.shadow_drawcalls++;
	cmd.drawIndexedIndirect(shadow_indirect_buffers[currentFrameIndex].buffer, vk::DeviceSize(0), (uint32_t)instancedDraws.size(), sizeof(VkDrawIndexedIndirectCommand));
	//for (auto& draw : instancedDraws) {		
	//
	//
	//	//cmd.setCheckpointNV("Draw Shadow", extensionDispatcher);
	//	cmd.drawIndexed(draw.index_count, draw.instance_count, draw.index_offset, 0, draw.first_index);
	//	eng_stats.drawcalls++;
	//	eng_stats.shadow_drawcalls++;
	//}
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

			set_pipeline_state_depth(width, height, cmd);

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
