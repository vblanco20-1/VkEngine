#include <linear_allocator.h>
#include <cppcoro/generator.hpp>

enum class CommandType : uint8_t {
	Custom,
	BindPipeline,
	BindDescriptorSet,
	BindIndexBuffer,
	DrawIndexedIndirect,
	DrawIndexed,
	DrawIndexedCompound,
	DrawTask,
	SetViewport,
	SetScissor,
	SetDepthBias,
	BeginTrace
};

struct ICommand {
	CommandType type;
	uint8_t size;
};

template<CommandType cmd>
struct Command: public ICommand {

	constexpr static CommandType cmd_type = cmd;
};

struct alignas(8) CMD_Custom : public Command<CommandType::Custom> {

	uint64_t id;
	void* data;

	//constexpr static CommandType cmd_type = CommandType::Custom;
};

struct alignas(8) CMD_BindPipeline : public Command<CommandType::BindPipeline> {

	uint64_t pipeline;

	//constexpr static CommandType cmd_type = CommandType::BindPipeline;
};

struct alignas(8) CMD_BindIndexBuffer : public Command<CommandType::BindIndexBuffer> {

	uint64_t offset;
	uint64_t indexBuffer;

	//constexpr static CommandType cmd_type = CommandType::BindIndexBuffer;
};

struct alignas(8) CMD_SetViewport : public Command< CommandType::SetViewport> {

	float x;
	float y;
	float w;
	float h;
	float depthMin;
	float depthMax;

	//constexpr static CommandType cmd_type = CommandType::SetViewport;
};

struct alignas(8) CMD_SetScissor : public Command< CommandType::SetScissor> {

	float x;
	float y;
	float w;
	float h;

	//constexpr static CommandType cmd_type = CommandType::SetScissor;
};

struct alignas(8) CMD_SetDepthBias : public Command< CommandType::SetDepthBias> {

	float biasConstantFactor;
	float biasClamp;
	float biasSlopeFactor;

	//constexpr static CommandType cmd_type = CommandType::SetDepthBias;
};

struct CMD_BindDescriptorSet : public Command< CommandType::BindDescriptorSet> {

	uint8_t setNumber;
	uint64_t descriptorSet;

	//constexpr static CommandType cmd_type = CommandType::BindDescriptorSet;
};

struct alignas(8) CMD_DrawIndexedIndirect : public Command< CommandType::DrawIndexedIndirect> {

	uint64_t indirectBuffer;
	uint32_t count;
	uint32_t offset;

	//constexpr static CommandType cmd_type = CommandType::DrawIndexedIndirect;
};

struct alignas(8) CMD_DrawMeshTask : public Command< CommandType::DrawTask> {
	
	uint32_t count;
	uint32_t first;
	//constexpr static CommandType cmd_type = CommandType::DrawIndexedIndirect;
};

struct alignas(8) CMD_DrawIndexed : public Command< CommandType::DrawIndexed> {

	uint32_t indexCount;
	uint32_t instanceCount; 
	uint32_t firstIndex; 
	int32_t vertexOffset; 
	uint32_t firstInstance;

	//constexpr static CommandType cmd_type = CommandType::DrawIndexed;
};

struct IndexedDraw {
	uint64_t pipeline;
	uint64_t descriptors[4];
	uint64_t indexBuffer;
	uint32_t indexOffset;
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t firstIndex;
	int32_t vertexOffset;
	uint32_t firstInstance;

	IndexedDraw() {
		for (int i = 0; i < 4; i++) { descriptors[i] = -1; };
	}
};

struct alignas(8) CMD_DrawIndexedCompound : public  Command< CommandType::DrawIndexedCompound> {

	IndexedDraw draw;
};


struct alignas(8) CMD_BeginTrace : public  Command< CommandType::BeginTrace> {

	void* profilerSourceLocation;	
};


struct CommandEncoder {

	void bind_descriptor_set(uint8_t setNumber, uint64_t descriptorSet);
	void bind_pipeline(uint64_t pipeline);
	void bind_index_buffer(uint64_t offset, uint64_t indexBuffer);

	void draw_indexed_indirect(uint64_t indirectBuffer, uint32_t count, uint32_t offset);
	void draw_indexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

	void draw_indexed(const IndexedDraw& draw);

	void draw_meshtask(uint32_t count, uint32_t first);

	void set_viewport(float x, float y, float w, float h, float depthMin, float depthMax);
	void set_scissor(float x, float y, float w, float h);
	void set_depthbias(float biasConstantFactor, float biasClamp, float biasSlopeFactor);
	
	void custom_command(uint64_t id, void* data);

	void begin_trace(void* profilerSourceLocation);

	void clear_encoder();
	cppcoro::generator<ICommand*> command_generator();
private:

	LinearAlloc encode_buffer;
};
