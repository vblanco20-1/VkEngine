#pragma once
#include <pcheader.h>
#include <functional>
#include <array>
#include "frame_resource.h"
enum class RenderGraphResourceLifetime {
	PerFrame,
	Persistent
};
enum class RenderGraphResourceAccess {
	Write,
	Read
};
enum class SizeClass {
	SwapchainRelative,
	Absolute
}; 

enum class PassType : uint8_t {
	Graphics,
	Compute,
	CPU
};
struct RenderAttachmentInfo
{
	SizeClass size_class{ SizeClass::SwapchainRelative };
	float size_x{ 1.0f };
	float size_y{ 1.0f };
	VkFormat format{ VK_FORMAT_UNDEFINED };
	VkClearValue clearValue;
	bool bClear{ false };

	void set_clear_color(std::array<float, 4> col);
	void set_clear_depth(float depth, uint32_t stencil);
};

class RenderPass
{
public:
	struct PassAttachment {
		RenderAttachmentInfo info{};
		std::string name{ "" };
	};
	struct PhysicalAttachment
	{
		VkAttachmentDescription desc;
		int index;
		bool bIsDepth;
		std::string name;
	};

	std::vector<VkClearValue> clearValues;
	std::vector<std::string> image_dependencies;
	std::vector<PassAttachment > color_attachments;
	std::vector<PhysicalAttachment> physical_attachments;

	PassAttachment depth_attachment;

	vk::RenderPass built_pass;
	vk::Framebuffer framebuffer;

	std::function<void(vk::CommandBuffer, RenderPass*)> draw_callback;

	int render_width = 0;
	int render_height = 0;
	class FrameGraph* owner;
	std::string name;
	PassType type;
	bool perform_submit;

	void add_image_dependency(std::string name);

	void add_color_attachment(std::string name, const RenderAttachmentInfo& info);
	void set_depth_attachment(std::string name, const RenderAttachmentInfo& info);
};

class FrameGraph {
public:
	struct GraphAttachment {
		RenderAttachmentInfo info;
		int reads{ 0 };
		int writes{ 0 };
		bool bIsDepth{ false };
		std::string name;
		int real_height;
		int real_width;
		VkImageUsageFlags usageFlags;
		VkDescriptorImageInfo descriptor;
		VmaAllocation imageAlloc;
		VkImage image;
		VkDeviceMemory mem;


		//pass that creates this
		std::string creator_pass{ "" };
		//last pass that writes this
		std::string last_writer_pass{ "" };
		//last pass to read from this
		std::string last_read_pass{ "" };
	};

	bool build(class VulkanEngine* engine);

	RenderPass* add_pass(std::string pass_name, std::function<void(vk::CommandBuffer, RenderPass*)> execution, PassType type, bool bPerformSubmit = false);
	RenderPass* get_pass(std::string name);
	VkDescriptorImageInfo get_image_descriptor(std::string name);
	GraphAttachment* get_attachment(std::string name);
	
	vk::CommandBuffer create_graphics_buffer(int threadID);
	void execute(vk::CommandBuffer cmd);

	void transform_images_to_write(RenderPass* pass, std::vector<vk::ImageMemoryBarrier>& image_barriers, vk::CommandBuffer& cmd);

	void transform_images_to_read(RenderPass* pass, std::vector<vk::ImageMemoryBarrier>& image_barriers, vk::CommandBuffer& cmd);

	void submit_commands(vk::CommandBuffer cmd, int wait_pass_index, int signal_pass_index);

	void build_command_pools();

	std::vector<RenderPass*> passes;
	std::unordered_map<std::string, RenderPass > pass_definitions;
	std::unordered_map<std::string, GraphAttachment> graph_attachments;

	FrameResource < std::array<vk::CommandPool,4> , 3 > commandPools;
	FrameResource< std::array<std::vector<vk::CommandBuffer>, 4>, 3>  usableCommands;
	FrameResource< std::array<std::vector<vk::CommandBuffer>, 4>, 3>  pendingCommands;

	VkExtent2D swapchainSize;
	std::vector<const char*> attachmentNames;
	VulkanEngine* owner;
};

std::array < vk::SubpassDependency, 2> build_basic_subpass_dependencies();

