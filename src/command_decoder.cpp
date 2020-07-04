#include <command_encoder.h>
#include <vulkan_render.h>
#include "vulkan_types.h"
#include <shader_processor.h>


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

void Decode_DrawIndexedIndirect(CommandDecodeState* state, ICommand* command) {

	CMD_DrawIndexedIndirect* cmd = static_cast<CMD_DrawIndexedIndirect*>(command);
	
	VulkanEngine* eng = state->engine;
	VkCommandBuffer vkCmd = state->cmd;
	if (state->wantsPipeline != state->boundPipeline) {
		//needs pipeline rebind

		const PipelineResource& pipeline = eng->render_registry.get<PipelineResource>(entt::entity{ state->wantsPipeline });

		state->boundLayout = pipeline.effect->build_pipeline_layout(eng->device);
		vkCmdBindPipeline(vkCmd,VK_PIPELINE_BIND_POINT_GRAPHICS , pipeline.pipeline);

		//rebind constants
		vkCmdSetViewport(vkCmd, 0, 1, &state->viewport);
	
		vkCmdSetScissor(vkCmd, 0, 1, &state->scissorRect);
		
		vkCmdSetDepthBias(vkCmd, state->depthBiasConstantFactor, state->depthBiasClamp, state->depthBiasSlopeFactor);

		state->boundPipeline = state->wantsPipeline;
		
	}	

	//check descriptor sets
	for (int i = 0; i < 4; i++) {

		if (state->boundDescriptors[i] != state->wantsDescriptors[i] && state->wantsDescriptors[i].valid()) {
			VkDescriptorSet dSet = reinterpret_cast<VkDescriptorSet>(state->wantsDescriptors[i].descriptor);

			vkCmdBindDescriptorSets(vkCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->boundLayout, i, 1, &dSet, 0, nullptr);

			state->boundDescriptors[i] = state->wantsDescriptors[i];
		}
	}

	//check index buffer
	if(state->boundIndex != state->wantsIndex) {

		VkBuffer buffer = reinterpret_cast<VkBuffer>(state->wantsIndex.indexBuffer);

		vkCmdBindIndexBuffer(vkCmd, buffer, VkDeviceSize{ state->wantsIndex.offset }, VK_INDEX_TYPE_UINT32);

		state->boundIndex.indexBuffer = state->wantsIndex.indexBuffer;
		state->boundIndex.offset = state->wantsIndex.offset;
	}


	VkBuffer buffer = reinterpret_cast<VkBuffer>(cmd->indirectBuffer);

	vkCmdDrawIndexedIndirect(vkCmd, buffer, cmd->offset, cmd->count, sizeof(VkDrawIndexedIndirectCommand));

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
	case CommandType::SetViewport:
		Decode_SetViewport(state, command);
		break;
	case CommandType::SetScissor:
		Decode_SetScissor(state, command);
		break;
	case CommandType::SetDepthBias:
		Decode_SetDepthBias(state, command);
		break;
	}
}




void VulkanEngine::DecodeCommands(VkCommandBuffer cmd, struct CommandEncoder* encoder)
{
	auto gn = encoder->command_generator();

	CommandDecodeState state;
	state.cmd = cmd;
	state.engine = this;

	for (ICommand* command : gn) {
		DecodeCommand(&state, command);
	}

	encoder->clear_encoder();
}