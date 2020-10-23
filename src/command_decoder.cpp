#include <command_encoder.h>
#include <vulkan_render.h>
#include "vulkan_types.h"
#include <shader_processor.h>
#include <TracyVulkan.hpp>

struct DescriptorBindState {
	uint64_t descriptor{ (uint64_t)-1 };

	bool valid() {
		return descriptor != (uint64_t)-1;
	}


	bool operator!=(const DescriptorBindState& other)const {
		return other.descriptor != descriptor;
	};
};
struct IndexBindState {
	uint64_t offset;
	uint64_t indexBuffer;
	bool operator!=(const IndexBindState& other)const {
		return other.indexBuffer != indexBuffer || offset != other.offset;
	};
};
struct CommandDecodeState {
	VkCommandBuffer cmd;
	VulkanEngine* engine;
	tracy::VkCtxManualScope* tracyScope{nullptr};
	VkViewport viewport;
	VkRect2D scissorRect;
	float depthBiasConstantFactor;
	float depthBiasClamp;
	float depthBiasSlopeFactor;

	uint64_t boundPipeline{ (uint64_t)-1 };
	uint64_t wantsPipeline{ (uint64_t)-1 };

	VkPipelineLayout boundLayout;

	std::array<DescriptorBindState, 4> boundDescriptors;
	std::array<DescriptorBindState, 4> wantsDescriptors;

	IndexBindState boundIndex;
	IndexBindState wantsIndex;
};

void Decode_BindPipeline(CommandDecodeState* state, ICommand* command) {

	CMD_BindPipeline* cmd = static_cast<CMD_BindPipeline*>(command);

	state->wantsPipeline = cmd->pipeline;

}
void Decode_BindDescriptorSet(CommandDecodeState* state, ICommand* command) {

	CMD_BindDescriptorSet* cmd = static_cast<CMD_BindDescriptorSet*>(command);

	state->wantsDescriptors[cmd->setNumber] = { cmd->descriptorSet };

}
void Decode_BindIndexBuffer(CommandDecodeState* state, ICommand* command) {

	CMD_BindIndexBuffer* cmd = static_cast<CMD_BindIndexBuffer*>(command);

	state->wantsIndex.indexBuffer = cmd->indexBuffer;
	state->wantsIndex.offset = cmd->offset;
}

void RefreshPipeline(CommandDecodeState* state)
{
	VulkanEngine* eng = state->engine;
	VkCommandBuffer vkCmd = state->cmd;
	if (state->wantsPipeline != state->boundPipeline) {
		//needs pipeline rebind

		const PipelineResource& pipeline = eng->render_registry.get<PipelineResource>(entt::entity{ state->wantsPipeline });

		state->boundLayout = pipeline.effect->build_pipeline_layout(eng->device);
		vkCmdBindPipeline(vkCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

		//rebind constants
		vkCmdSetViewport(vkCmd, 0, 1, &state->viewport);

		vkCmdSetScissor(vkCmd, 0, 1, &state->scissorRect);

		vkCmdSetDepthBias(vkCmd, state->depthBiasConstantFactor, state->depthBiasClamp, state->depthBiasSlopeFactor);

		state->boundPipeline = state->wantsPipeline;

	}
}
void RefreshDescriptors(CommandDecodeState* state)
{
	VulkanEngine* eng = state->engine;
	VkCommandBuffer vkCmd = state->cmd;

	for (int i = 0; i < 4; i++) {

		if (state->boundDescriptors[i] != state->wantsDescriptors[i] && state->wantsDescriptors[i].valid()) {
			VkDescriptorSet dSet = reinterpret_cast<VkDescriptorSet>(state->wantsDescriptors[i].descriptor);

			vkCmdBindDescriptorSets(vkCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->boundLayout, i, 1, &dSet, 0, nullptr);

			state->boundDescriptors[i] = state->wantsDescriptors[i];
		}
	}
}
void RefreshIndexBuffer(CommandDecodeState* state)
{
	VulkanEngine* eng = state->engine;
	VkCommandBuffer vkCmd = state->cmd;

	VkBuffer buffer = reinterpret_cast<VkBuffer>(state->wantsIndex.indexBuffer);

	vkCmdBindIndexBuffer(vkCmd, buffer, VkDeviceSize{ state->wantsIndex.offset }, VK_INDEX_TYPE_UINT32);

	state->boundIndex.indexBuffer = state->wantsIndex.indexBuffer;
	state->boundIndex.offset = state->wantsIndex.offset;
}
void Decode_DrawIndexedIndirect(CommandDecodeState* state, ICommand* command) {

	CMD_DrawIndexedIndirect* cmd = static_cast<CMD_DrawIndexedIndirect*>(command);

	VulkanEngine* eng = state->engine;
	VkCommandBuffer vkCmd = state->cmd;
	if (state->wantsPipeline != state->boundPipeline) {
		RefreshPipeline(state);
	}


	//check descriptor sets
	RefreshDescriptors(state);

	//check index buffer
	if(state->boundIndex != state->wantsIndex) {

		RefreshIndexBuffer(state);
	}


	VkBuffer buffer = reinterpret_cast<VkBuffer>(cmd->indirectBuffer);

	vkCmdDrawIndexedIndirect(vkCmd, buffer, cmd->offset, cmd->count, sizeof(VkDrawIndexedIndirectCommand));

	eng->eng_stats.drawcalls++;
}
void Decode_DrawIndexed(CommandDecodeState* state, ICommand* command) {

	CMD_DrawIndexed* cmd = static_cast<CMD_DrawIndexed*>(command);

	VulkanEngine* eng = state->engine;
	VkCommandBuffer vkCmd = state->cmd;
	if (state->wantsPipeline != state->boundPipeline) {
		RefreshPipeline(state);
	}

	//check descriptor sets
	RefreshDescriptors(state);

	//check index buffer
	if (state->boundIndex != state->wantsIndex) {

		RefreshIndexBuffer(state);
	}

	vkCmdDrawIndexed(vkCmd, cmd->indexCount, cmd->instanceCount, cmd->firstIndex, cmd->vertexOffset, cmd->firstInstance);
	eng->eng_stats.drawcalls++;
}

void Decode_DrawIndexedCompound(CommandDecodeState* state, ICommand* command)
{
	CMD_DrawIndexedCompound* cmd = static_cast<CMD_DrawIndexedCompound*>(command);

	VulkanEngine* eng = state->engine;
	VkCommandBuffer vkCmd = state->cmd;
	if (cmd->draw.pipeline != state->boundPipeline) {
		state->wantsPipeline = cmd->draw.pipeline;
		RefreshPipeline(state);
	}

	for (int i = 0; i < 4; i++) {
		state->wantsDescriptors[i].descriptor = (cmd->draw.descriptors[i]);
	}
	
	//check descriptor sets
	RefreshDescriptors(state);

	state->wantsIndex.indexBuffer = cmd->draw.indexBuffer;
	state->wantsIndex.offset = cmd->draw.indexOffset;
	//check index buffer
	if (state->boundIndex != state->wantsIndex) {

		RefreshIndexBuffer(state);
	}

	vkCmdDrawIndexed(vkCmd, cmd->draw.indexCount, cmd->draw.instanceCount, cmd->draw.firstIndex, cmd->draw.vertexOffset, cmd->draw.firstInstance);
	eng->eng_stats.drawcalls++;
}
PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNVFN = nullptr;
void Decode_DrawMeshTasks(CommandDecodeState* state, ICommand* command) {

	CMD_DrawMeshTask* cmd = static_cast<CMD_DrawMeshTask*>(command);

	VulkanEngine* eng = state->engine;
	VkCommandBuffer vkCmd = state->cmd;
	if (state->wantsPipeline != state->boundPipeline) {
		RefreshPipeline(state);
	}

	//check descriptor sets
	RefreshDescriptors(state);

	//check index buffer
	if (state->boundIndex != state->wantsIndex) {

		RefreshIndexBuffer(state);
	}

	if (vkCmdDrawMeshTasksNVFN == nullptr)
	{
		vkCmdDrawMeshTasksNVFN = (PFN_vkCmdDrawMeshTasksNV)vkGetInstanceProcAddr(state->engine->instance, "vkCmdDrawMeshTasksNV");
	}

	vkCmdDrawMeshTasksNVFN(vkCmd, cmd->count, cmd->first);
	//vkCmdDrawIndexed(vkCmd, cmd->indexCount, cmd->instanceCount, cmd->firstIndex, cmd->vertexOffset, cmd->firstInstance);
	eng->eng_stats.drawcalls++;
}

void Decode_SetViewport(CommandDecodeState* state, ICommand* command) {

	CMD_SetViewport* cmd = static_cast<CMD_SetViewport*>(command);

	state->viewport.x = cmd->x;
	state->viewport.y = cmd->y;
	state->viewport.width = cmd->w;
	state->viewport.height = cmd->h;

	state->viewport.minDepth = cmd->depthMin;
	state->viewport.maxDepth = cmd->depthMax;
}

void Decode_SetScissor(CommandDecodeState* state, ICommand* command) {

	CMD_SetScissor* cmd = static_cast<CMD_SetScissor*>(command);

	state->scissorRect.offset.x = cmd->x;
	state->scissorRect.offset.y = cmd->y;
	state->scissorRect.extent.width = cmd->w;
	state->scissorRect.extent.height = cmd->h;


}
void Decode_SetDepthBias(CommandDecodeState* state, ICommand* command) {

	CMD_SetDepthBias* cmd = static_cast<CMD_SetDepthBias*>(command);

	state->depthBiasClamp = cmd->biasClamp;
	state->depthBiasConstantFactor = cmd->biasConstantFactor;
	state->depthBiasSlopeFactor = cmd->biasSlopeFactor;
}


void Decode_BeginTrace(CommandDecodeState* state, ICommand* command)
{
	CMD_BeginTrace* cmd = static_cast<CMD_BeginTrace*>(command);
	const tracy::SourceLocationData* sourceloc = (const tracy::SourceLocationData*)cmd->profilerSourceLocation;

	if (state->tracyScope == nullptr) {
		tracy::VkCtx* ctx = (tracy::VkCtx*)state->engine->get_profiler_context(state->cmd);
		state->tracyScope = new tracy::VkCtxManualScope(ctx, sourceloc, state->cmd, true);
		state->tracyScope->Start(state->cmd);
	}
	else {
		state->tracyScope->End();
		state->tracyScope->m_srcloc = sourceloc;
	}
}

void DecodeCommand(CommandDecodeState* state, ICommand* command) {

	switch (command->type) {
	case CommandType::Custom:
		break;
	case CommandType::BindPipeline:
		Decode_BindPipeline(state, command);
		break;
	case CommandType::BindDescriptorSet:
		Decode_BindDescriptorSet(state, command);
		break;
	case CommandType::BindIndexBuffer:
		Decode_BindIndexBuffer(state, command);
		break;
	case CommandType::DrawIndexedIndirect:
		Decode_DrawIndexedIndirect(state, command);
		break;
	case CommandType::DrawIndexedCompound:
		Decode_DrawIndexedCompound(state, command);
		break;
	case CommandType::DrawIndexed:
		Decode_DrawIndexed(state, command);
		break;
	case CommandType::SetViewport:
		Decode_SetViewport(state, command);
		break;
	case CommandType::SetScissor:
		Decode_SetScissor(state, command);
		break;
	case CommandType::SetDepthBias:
		Decode_SetDepthBias(state, command);
		break;
	case CommandType::BeginTrace :
		Decode_BeginTrace(state, command);
		break;
	}
}




void VulkanEngine::DecodeCommands(VkCommandBuffer cmd, struct CommandEncoder* encoder)
{
	ZoneScopedN("Command decoding")
	
	tracy::VkCtx* pctx = (tracy::VkCtx*)get_profiler_context(cmd);
	TracyVkZone(pctx, cmd, "Command Decoding");
	auto gn = encoder->command_generator();

	CommandDecodeState state;
	state.cmd = cmd;
	state.engine = this;

	for (ICommand* command : gn) {
		DecodeCommand(&state, command);
	}
	if (state.tracyScope) {
		state.tracyScope->End();
	}

	encoder->clear_encoder();
	
}