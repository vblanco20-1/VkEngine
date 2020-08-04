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






bool IsVisible(ObjectBounds& bounds, Camera& mainCam)
{
	glm::vec3 bounds_center = glm::vec3(bounds.center_rad);
	glm::vec3 dir = normalize(mainCam.eyeLoc - bounds_center);

	float angle = glm::angle(dir, mainCam.eyeDir);

	glm::vec3 bmin = bounds_center - glm::vec3(bounds.extent);
	glm::vec3 bmax = bounds_center + glm::vec3(bounds.extent);

	return mainCam.camfrustum.IsBoxVisible(bmin, bmax);	
}

void VulkanEngine::RenderMainPass_Other(const vk::CommandBuffer& cmd)
{
	static CommandEncoder* encoder{ nullptr };
	TextureResource bluenoise = getResource<TextureResource>(bluenoiseTexture);

	ZoneScopedNC("Main Pass", tracy::Color::BlueViolet);
	EntityID LastMesh = entt::null;

	//static std::vector<DrawUnitEncoder> drawables;

	std::vector<DrawUnitEncoder>& drawables = MeshPassState.MainPassDrawables;

	if (encoder == nullptr) { encoder = new CommandEncoder(); };

	int numRenderables = render_registry.view<RenderMeshComponent>().size();
	if (numRenderables == 0) return;

#if 0

	drawables.clear();
	drawables.reserve(render_registry.view<RenderMeshComponent>().size());

	render_registry.group<RenderMeshComponent, TransformComponent, ObjectBounds>().each([&](RenderMeshComponent& renderable, TransformComponent& tf, ObjectBounds& bounds) {
		const MeshResource& mesh = render_registry.get<MeshResource>(renderable.mesh_resource_entity);
		const PipelineResource& pipeline = render_registry.get<PipelineResource>(renderable.pass_pipelines[(size_t)MeshPasIndex::MainPass]);
		const DescriptorResource& descriptor = render_registry.get<DescriptorResource>(renderable.pass_descriptors[(size_t)MeshPasIndex::MainPass]);

		

		if (IsVisible(bounds, mainCam)) {
			DrawUnitEncoder newDrawUnit;

			VkBuffer index = mesh.indexBuffer.buffer;
			VkDescriptorSet matset = descriptor.materialSet;

			newDrawUnit.indexBuffer = reinterpret_cast<uint64_t>(index);
			newDrawUnit.vertexBuffer = 0;//reinterpret_cast<uint64_t>(mesh.vertexBuffer.buffer);
			newDrawUnit.index_count = mesh.indices.size();
			newDrawUnit.material_set = reinterpret_cast<uint64_t>(matset);
			newDrawUnit.object_idx = renderable.object_idx;

			glm::vec3 bounds_center = glm::vec3(bounds.center_rad);			
			newDrawUnit.distance = (mainCam.eyeLoc - bounds_center).length();
			EntityID pipelineID = renderable.pass_pipelines[(size_t)MeshPasIndex::MainPass];
			
			newDrawUnit.pipeline = uint32_t(pipelineID);//pipeline.pipeline;
			//newDrawUnit.effect = pipeline.effect;
			newDrawUnit.index_offset = mesh.index_offset;
			drawables.push_back(newDrawUnit);
		}
	});

	std::sort(drawables.begin(), drawables.end(), [](const DrawUnitEncoder& a, const DrawUnitEncoder& b) {
		return a.distance < b.distance;//a.vertexBuffer < b.vertexBuffer;
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

#endif
	{
		const DrawUnitEncoder& unit = drawables[0];

		auto pass = render_graph.get_pass("GBuffer");		

		const PipelineResource& pipeline = render_registry.get<PipelineResource>(entt::entity{ unit.pipeline });


		DescriptorSetBuilder setBuilder{ pipeline.effect,&descriptorMegapool };

		VkDescriptorBufferInfo camBufferInfo;
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

		setBuilder.bind_image_array(0, 10, texCache->all_images.data(), texCache->all_images.size());

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
	
}

void VulkanEngine::PrepareMeshPasses()
{
	int numRenderables = render_registry.view<RenderMeshComponent>().size();



	MeshPassState.MainPassDrawables.clear();
	MeshPassState.MainPassDrawables.reserve(render_registry.view<RenderMeshComponent>().size());

	if (numRenderables == 0) return;
	{
		ZoneScopedNC("Gbuffer Pass", tracy::Color::Blue);
		//GBUFFER PASS

		MeshPassState.GBufferDrawables.clear();
		MeshPassState.GBufferDrawables.reserve(numRenderables);

		MeshPassState.PrepassDrawables.clear();
		MeshPassState.PrepassDrawables.reserve(numRenderables);

		render_registry.group<RenderMeshComponent, TransformComponent, ObjectBounds>().each([&](RenderMeshComponent& renderable, TransformComponent& tf, ObjectBounds& bounds) {
			const MeshResource& mesh = render_registry.get<MeshResource>(renderable.mesh_resource_entity);

			const DescriptorResource& descriptor = render_registry.get<DescriptorResource>(renderable.pass_descriptors[(size_t)MeshPasIndex::MainPass]);

			bool bVisible = false;

			bVisible = IsVisible(bounds, mainCam);

			if (IsVisible(bounds, mainCam)) {
				{
					DrawUnitEncoder newDrawUnit;
				
					VkBuffer index = mesh.indexBuffer.buffer;
					VkDescriptorSet matset = descriptor.materialSet;
				
					newDrawUnit.indexBuffer = reinterpret_cast<uint64_t>(index);
					newDrawUnit.vertexBuffer = 0;
					newDrawUnit.index_count = mesh.indices.size();
					newDrawUnit.material_set = 0;
					newDrawUnit.object_idx = renderable.object_idx;
					glm::vec3 bounds_center = glm::vec3(bounds.center_rad);
					newDrawUnit.distance = (mainCam.eyeLoc - bounds_center).length();
					newDrawUnit.pipeline = 0;
					newDrawUnit.index_offset = mesh.index_offset;
				
					//MeshPassState.GBufferDrawables.push_back(newDrawUnit);
					//MeshPassState.MainPassDrawables.push_back(newDrawUnit);
				}
				{
					DrawUnitEncoder newDrawUnit;

					VkBuffer index = mesh.indexBuffer.buffer;
					VkDescriptorSet matset = descriptor.materialSet;

					newDrawUnit.indexBuffer = reinterpret_cast<uint64_t>(index);
					newDrawUnit.vertexBuffer = 0;
					newDrawUnit.index_count = mesh.indices.size();
					newDrawUnit.material_set = reinterpret_cast<uint64_t>(matset);
					newDrawUnit.object_idx = renderable.object_idx;

					glm::vec3 bounds_center = glm::vec3(bounds.center_rad);
					newDrawUnit.distance = (mainCam.eyeLoc - bounds_center).length();
					EntityID pipelineID = renderable.pass_pipelines[(size_t)MeshPasIndex::MainPass];

					newDrawUnit.pipeline = uint32_t(pipelineID);					
					newDrawUnit.index_offset = mesh.index_offset;
					MeshPassState.GBufferDrawables.push_back(newDrawUnit);
					MeshPassState.MainPassDrawables.push_back(newDrawUnit);
				}
			}
			});


		std::sort(MeshPassState.GBufferDrawables.begin(), MeshPassState.GBufferDrawables.end(), [](const DrawUnitEncoder& a, const DrawUnitEncoder& b) {
			return a.distance > b.distance;
		});

		//convert to indirect
		void* indirect = mapBuffer(gbuffer_indirect_buffers[currentFrameIndex]);
		{
			ZoneScopedN("Indirect Upload")
				VkDrawIndexedIndirectCommand* commands = (VkDrawIndexedIndirectCommand*)indirect;

			for (int i = 0; i < MeshPassState.GBufferDrawables.size(); i++) {

				auto draw = MeshPassState.GBufferDrawables[i];
				commands[i].indexCount = draw.index_count;
				commands[i].instanceCount = 1;
				commands[i].firstIndex = draw.index_offset;
				commands[i].vertexOffset = 0;
				commands[i].firstInstance = draw.object_idx;
			}
		}
		unmapBuffer(gbuffer_indirect_buffers[currentFrameIndex]);
	}
	// SHADOW PASS ---------------------

	{
		ZoneScopedNC("Shadow Pass", tracy::Color::BlueViolet);

				int numRenderables = render_registry.view<RenderMeshComponent>().size();
		if (numRenderables == 0) return;
		
		VkBuffer LastIndex{};
		
		std::vector<ShadowDrawUnit>& drawUnits = MeshPassState.ShadowDrawUnits;
		std::vector<InstancedDraw>& instancedDraws = MeshPassState.ShadowInstancedDraws;
		
		
		drawUnits.clear();
		instancedDraws.clear();
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

		{
			ZoneScopedN("Command encoding")

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
		}
		int binds = 0;
		int draw_idx = 0;


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
			}
		}
		unmapBuffer(shadow_indirect_buffers[currentFrameIndex]);
	}

	{

	std::vector<DrawUnitEncoder>& drawables = MeshPassState.MainPassDrawables;

	std::sort(drawables.begin(), drawables.end(), [](const DrawUnitEncoder& a, const DrawUnitEncoder& b) {
		return a.distance < b.distance;
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

	}
}

void VulkanEngine::RenderGBufferPass(const vk::CommandBuffer& cmd)
{
	static CommandEncoder* encoder{ nullptr };
	if (encoder == nullptr) { encoder = new CommandEncoder(); };

	tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
	TracyVkZoneC(profilercontext, VkCommandBuffer(cmd), "Gbuffer pass", tracy::Color::Grey);

	ZoneScopedNC("GBuffer Pass", tracy::Color::BlueViolet);
	EntityID LastMesh = entt::null;

	//static std::vector<DrawUnitEncoder> drawables;

	
	ShaderEffect* effect = getResource<ShaderEffectHandle>("basicgbuf").handle;


	eng_stats.gbuffer_drawcalls = 0;

		const DrawUnitEncoder& unit = MeshPassState.GBufferDrawables[0];

		auto pass = render_graph.get_pass("GBuffer");
		uint64_t pipid = (uint32_t)GbufferPipelineID;
		encoder->bind_pipeline(pipid);

		encoder->set_depthbias(0, 0, 0);
		encoder->set_scissor(0, 0, pass->render_width, pass->render_height);
		encoder->set_viewport(0, 0, pass->render_width, pass->render_height, 0, 1);

		const PipelineResource& pipeline = render_registry.get<PipelineResource>(GbufferPipelineID);
		DescriptorSetBuilder setBuilder{ pipeline.effect,&descriptorMegapool };

		VkDescriptorBufferInfo camBufferInfo;
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

		VkDescriptorBufferInfo transformBufferInfo;
		transformBufferInfo.buffer = object_buffers[currentFrameIndex].buffer;
		transformBufferInfo.offset = 0;
		transformBufferInfo.range = sizeof(GpuObjectData) * 10000;

		setBuilder.bind_buffer("ubo", camBufferInfo);
		setBuilder.bind_buffer("MainObjectBuffer", transformBufferInfo);

		//build_and_bind_descriptors(setBuilder, 0, cmd);


	

	VkDescriptorSet descriptor = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);

	encoder->bind_descriptor_set(0, reinterpret_cast<uint64_t>(descriptor));

	encoder->bind_index_buffer(0, unit.indexBuffer);

	VkBuffer indirectBuffer = gbuffer_indirect_buffers[currentFrameIndex].buffer;

	encoder->draw_indexed_indirect(reinterpret_cast<uint64_t>(indirectBuffer), MeshPassState.GBufferDrawables.size(), 0);

	{
		tracy::VkCtx* profilercontext = (tracy::VkCtx*)get_profiler_context(cmd);
		TracyVkZoneC(profilercontext, VkCommandBuffer(cmd), "GBuffer Pass", tracy::Color::Red);

		ZoneScopedN("Command decoding")

		DecodeCommands(cmd, encoder);
	}
}

void VulkanEngine::RenderDepthPrePass(RenderPassCommands* cmd, int height, int width)
{
	

	int numRenderables = render_registry.view<RenderMeshComponent>().size();
	if (numRenderables == 0) return;

	CommandEncoder* encoder = cmd->commandEncoder;

	TracySourceLocation(shadowloc, "Depth Pre Pass", tracy::Color::Grey);

	encoder->begin_trace((void*)&shadowloc);


	const PipelineResource& pipeline = render_registry.get<PipelineResource>(ShadowPipelineID);

	ShaderEffect* effect = pipeline.effect;


	uint64_t pipid = (uint32_t)ShadowPipelineID;

	encoder->set_depthbias(0, 0, 0);
	encoder->set_scissor(0, 0, width, height);
	encoder->set_viewport(0, 0, width, height, 0, 1);

	VkPipelineLayout piplayout = effect->build_pipeline_layout(device);


	DescriptorSetBuilder setBuilder{ effect,&descriptorMegapool };
	
	VkDescriptorBufferInfo transformBufferInfo = make_buffer_info(object_buffers[currentFrameIndex].buffer, sizeof(GpuObjectData) * 10000);
	
	VkDescriptorBufferInfo camBufferInfo;
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

	setBuilder.bind_buffer("ubo", camBufferInfo);
	setBuilder.bind_buffer("MainObjectBuffer", transformBufferInfo);



	std::array<VkDescriptorSet, 2> descriptors;
	descriptors[0] = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);

	encoder->bind_descriptor_set(0, reinterpret_cast<uint64_t>(descriptors[0]));

	int binds = 0;
	int draw_idx = 0;


	eng_stats.drawcalls++;
	
	{

		ZoneScopedN("Command encoding")

		encoder->bind_pipeline(pipid);
		const DrawUnitEncoder& unit = MeshPassState.GBufferDrawables[0];
		encoder->bind_index_buffer(0, unit.indexBuffer);

		VkBuffer indirectBuffer = gbuffer_indirect_buffers[currentFrameIndex].buffer;

		encoder->draw_indexed_indirect(reinterpret_cast<uint64_t>(indirectBuffer), MeshPassState.GBufferDrawables.size(), 0);
	}


	return;
}


void VulkanEngine::render_shadow_pass(RenderPassCommands* cmd, int height, int width)
{
	
	eng_stats.shadow_drawcalls = 0;

	std::vector<ShadowDrawUnit>& drawUnits = MeshPassState.ShadowDrawUnits;
	std::vector<InstancedDraw>& instancedDraws = MeshPassState.ShadowInstancedDraws;
	//static std::vector<ShadowDrawUnit> drawUnits;
	
	//static std::vector<InstancedDraw> instancedDraws;
	//drawUnits.clear();
	//instancedDraws.clear();

	int numRenderables = render_registry.view<RenderMeshComponent>().size();
	if (numRenderables == 0) return;

	CommandEncoder* encoder = cmd->commandEncoder;


	TracySourceLocation(shadowloc, "Shadow Pass", tracy::Color::Grey);

	encoder->begin_trace((void*)&shadowloc);

	//static CommandEncoder* encoder{ nullptr };
	//if (encoder == nullptr) { encoder = new CommandEncoder(); };

	const PipelineResource& pipeline = render_registry.get<PipelineResource>(ShadowPipelineID);

	ShaderEffect* effect = pipeline.effect;

	
	uint64_t pipid = (uint32_t)ShadowPipelineID;
	//encoder->bind_pipeline(pipid);

	encoder->set_depthbias(0, 0, 0);
	encoder->set_scissor(0, 0, width, height);
	encoder->set_viewport(0, 0, width, height, 0, 1);

	VkPipelineLayout piplayout = effect->build_pipeline_layout(device);
	

	DescriptorSetBuilder setBuilder{ effect,&descriptorMegapool };

	VkDescriptorBufferInfo shadowBufferInfo = make_buffer_info<UniformBufferObject>(shadowDataBuffers[currentFrameIndex]);

	VkDescriptorBufferInfo transformBufferInfo = make_buffer_info(object_buffers[currentFrameIndex].buffer, sizeof(GpuObjectData) * 10000);
	VkDescriptorBufferInfo instanceBufferInfo = make_buffer_info(shadow_instance_buffers[currentFrameIndex].buffer, sizeof(uint32_t) * 10000);

	setBuilder.bind_buffer("ubo", shadowBufferInfo);
	setBuilder.bind_buffer("MainObjectBuffer", transformBufferInfo);
	setBuilder.bind_buffer("InstanceIDBuffer", instanceBufferInfo);

	std::array<VkDescriptorSet, 2> descriptors;
	descriptors[0] = setBuilder.build_descriptor(0, DescriptorLifetime::PerFrame);

	encoder->bind_descriptor_set(0, reinterpret_cast<uint64_t>( descriptors[0]));



	//ZoneScopedNC("Shadow Pass", tracy::Color::BlueViolet);
	//VkBuffer LastIndex{};


	//render_registry.view<RenderMeshComponent>().each([&](RenderMeshComponent& renderable) {
	//
	//	const MeshResource& mesh = render_registry.get<MeshResource>(renderable.mesh_resource_entity);
	//
	//	ShadowDrawUnit unit;
	//	unit.object_idx = renderable.object_idx;
	//	unit.indexBuffer = &mesh.indexBuffer;
	//	unit.index_count = mesh.indices.size();
	//	unit.index_offset = mesh.index_offset;
	//	drawUnits.push_back(unit);
	//	});
	//
	//std::sort(drawUnits.begin(), drawUnits.end(), [](const ShadowDrawUnit& a, const ShadowDrawUnit& b) {
	//
	//	if (a.indexBuffer->buffer == b.indexBuffer->buffer)
	//	{
	//		return  a.index_offset < b.index_offset;
	//	}
	//	else {
	//
	//		return a.indexBuffer->buffer < b.indexBuffer->buffer;
	//	}
	//});



	//void* matdata = mapBuffer(shadow_instance_buffers[currentFrameIndex]);
	//{
	//	ZoneScopedN("Instance Upload")
	//		int32_t* data = (int32_t*)matdata;
	//
	//	for (int i = 0; i < drawUnits.size(); i++) {
	//		data[i] = drawUnits[i].object_idx;
	//	}
	//}
	//unmapBuffer(shadow_instance_buffers[currentFrameIndex]);

	//{
	//	ZoneScopedN("Command encoding")
	//
	//	int idx_ofs = -1;
	//	InstancedDraw pendingDraw;
	//	for (int i = 0; i < drawUnits.size(); i++)
	//	{
	//		auto& unit = drawUnits[i];
	//		if (LastIndex != unit.indexBuffer->buffer || idx_ofs != unit.index_offset) {
	//			if (i != 0) {
	//				instancedDraws.push_back(pendingDraw);
	//
	//				//IndexedDraw newDraw{};
	//				//newDraw.pipeline = pipid;
	//				//newDraw.descriptors[0] = reinterpret_cast<uint64_t>(descriptors[0]);
	//				//newDraw.indexBuffer = reinterpret_cast<uint64_t>(pendingDraw.index_buffer);
	//				//newDraw.indexOffset = 0;
	//				//newDraw.firstInstance = pendingDraw.first_index;
	//				//newDraw.firstIndex = pendingDraw.index_offset;
	//				//newDraw.indexCount = pendingDraw.index_count;
	//				//newDraw.instanceCount = pendingDraw.instance_count;
	//				//newDraw.vertexOffset = 0;
	//				//
	//				//encoder->draw_indexed(newDraw);
	//			}
	//
	//			pendingDraw.first_index = i;
	//			pendingDraw.index_buffer = unit.indexBuffer->buffer;
	//			pendingDraw.index_count = unit.index_count;
	//			pendingDraw.instance_count = 1;
	//			pendingDraw.index_offset = unit.index_offset;
	//			LastIndex = unit.indexBuffer->buffer;
	//			idx_ofs = unit.index_offset;
	//		}
	//		else {
	//			pendingDraw.instance_count++;
	//		}
	//	}
	//
	//instancedDraws.push_back(pendingDraw);
	//}
	int binds = 0;
	int draw_idx = 0;	
	
#if 1
	//convert to indirect
	//void* indirect = mapBuffer(shadow_indirect_buffers[currentFrameIndex]);
	//{
	//	ZoneScopedN("Indirect Upload")
	//		VkDrawIndexedIndirectCommand* commands = (VkDrawIndexedIndirectCommand*)indirect;
	//
	//	for (int i = 0; i < instancedDraws.size(); i++) {
	//
	//		auto draw = instancedDraws[i];
	//
	//		commands[i].indexCount = draw.index_count;
	//		commands[i].instanceCount = draw.instance_count;
	//		commands[i].firstIndex = draw.index_offset;
	//		commands[i].vertexOffset = 0;
	//		commands[i].firstInstance = draw.first_index;
	//	}
	//}
	//unmapBuffer(shadow_indirect_buffers[currentFrameIndex]);

	eng_stats.drawcalls++;
	eng_stats.shadow_drawcalls++;
	//cmd.drawIndexedIndirect(shadow_indirect_buffers[currentFrameIndex].buffer, vk::DeviceSize(0), (uint32_t)instancedDraws.size(), sizeof(VkDrawIndexedIndirectCommand));
	{

		ZoneScopedN("Command encoding")

		

		encoder->bind_pipeline(pipid);

		//for (auto& draw : instancedDraws) {
		//
		//	encoder->bind_index_buffer(0, reinterpret_cast<uint64_t>(draw.index_buffer));
		//	encoder->draw_indexed(draw.index_count, draw.instance_count, draw.index_offset, 0, draw.first_index);
		//}

		VkBuffer indirectBuffer = shadow_indirect_buffers[currentFrameIndex].buffer;

		encoder->bind_index_buffer(0, reinterpret_cast<uint64_t>(instancedDraws[0].index_buffer));
		encoder->draw_indexed_indirect(reinterpret_cast<uint64_t>(indirectBuffer), instancedDraws.size(), 0);
	}

#endif
	return;

}
