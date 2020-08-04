#include <command_encoder.h>



template<typename C>
C* encode(LinearAlloc& buffer) {
	C* cmd = buffer.construct<C>();
	cmd->type = C::cmd_type;
	cmd->size = sizeof(C);
	return cmd;
}

void CommandEncoder::custom_command(uint64_t id, void* data)
{
	CMD_Custom* cmd = encode<CMD_Custom>(encode_buffer);
	
	cmd->id = id;
	cmd->data = data;
}



void CommandEncoder::begin_trace(void* profilerSourceLocation)
{
	CMD_BeginTrace *cmd = encode<CMD_BeginTrace>(encode_buffer);

	cmd->profilerSourceLocation = profilerSourceLocation;
}

void CommandEncoder::bind_pipeline(uint64_t pipeline)
{
	CMD_BindPipeline* cmd = encode<CMD_BindPipeline>(encode_buffer);

	cmd->pipeline = pipeline;
}




void CommandEncoder::bind_index_buffer(uint64_t offset, uint64_t indexBuffer)
{
	CMD_BindIndexBuffer* cmd = encode<CMD_BindIndexBuffer>(encode_buffer);

	cmd->offset = offset;
	cmd->indexBuffer = indexBuffer;
}



void CommandEncoder::draw_indexed_indirect(uint64_t indirectBuffer, uint32_t count, uint32_t offset)
{
	CMD_DrawIndexedIndirect* cmd = encode<CMD_DrawIndexedIndirect>(encode_buffer);

	cmd->indirectBuffer = indirectBuffer;
	cmd->count = count;
	cmd->offset = offset;
}




void CommandEncoder::draw_indexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	CMD_DrawIndexed* cmd = encode<CMD_DrawIndexed>(encode_buffer);

	cmd->indexCount = indexCount;
	cmd->instanceCount=instanceCount;
	cmd->firstIndex=firstIndex;
	cmd->vertexOffset=vertexOffset;
	cmd->firstInstance=firstInstance;
}

void CommandEncoder::draw_indexed(const IndexedDraw& draw)
{
	CMD_DrawIndexedCompound* cmd = encode<CMD_DrawIndexedCompound>(encode_buffer);

	cmd->draw = draw;
}

void CommandEncoder::set_viewport(float x,
float y,
float w,
float h,
float depthMin,
float depthMax)
{
	CMD_SetViewport* cmd = encode<CMD_SetViewport>(encode_buffer);

	cmd->x = x;
	cmd->y = y;
	cmd->w = w;
	cmd->h = h;
	cmd->depthMin = depthMin;
	cmd->depthMax = depthMax;
}





void CommandEncoder::set_scissor(float x,
	float y,
	float w,
	float h)
{
	CMD_SetScissor* cmd = encode<CMD_SetScissor>(encode_buffer);

	cmd->x = x;
	cmd->y = y;
	cmd->w = w;
	cmd->h = h;
}



void CommandEncoder::set_depthbias(float biasConstantFactor,
	float biasClamp,
	float biasSlopeFactor)
{
	CMD_SetDepthBias* cmd = encode<CMD_SetDepthBias>(encode_buffer);

	cmd->biasConstantFactor = biasConstantFactor;
	cmd->biasClamp = biasClamp;
	cmd->biasSlopeFactor = biasSlopeFactor;
}





void CommandEncoder::bind_descriptor_set(uint8_t setNumber,uint64_t descriptorSet)
{
	CMD_BindDescriptorSet* cmd = encode<CMD_BindDescriptorSet>(encode_buffer);

	cmd->type = CommandType::BindDescriptorSet;
	cmd->setNumber = setNumber;
	cmd->descriptorSet = descriptorSet;
}


void CommandEncoder::clear_encoder()
{
	encode_buffer.clear();
}

cppcoro::generator<ICommand*> CommandEncoder::command_generator()
{
	size_t cursor = 0;

	LinearAlloc::DataChunk* chunk = encode_buffer.first();
	while (chunk) {

		while (cursor < chunk->last) {
			ICommand* cmd = (ICommand*)&chunk->bytes[cursor];

			co_yield cmd;

			cursor += cmd->size;
		}		
		chunk = chunk->next;
		cursor = 0;
	}
}
