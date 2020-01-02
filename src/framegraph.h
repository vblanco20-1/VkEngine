#pragma once
#include <pcheader.h>
#include <functional>

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
struct AttachmentInfo
{
	SizeClass size_class{ SizeClass::SwapchainRelative};
	float size_x{1.0f};
	float size_y{1.0f};
	VkFormat format{ VK_FORMAT_UNDEFINED };
	
	bool persistent{true};
};

struct ClearPassSet {
	//5 max
	std::array<vk::ClearValue, 5> clearValues;
	int num;
};
class FrameGraph;
struct RenderPass{

	struct PassAttachment {
		AttachmentInfo info;
		std::string name;
		bool bIsDepth;
		bool bWrite;
	};

	struct PhysicalAttachment
	{
		vk::AttachmentDescription desc;
		int index;
		bool bIsDepth;
	};

	void add_color_attachment(std::string name, const AttachmentInfo& info, RenderGraphResourceAccess access);
	void add_depth_attachment(std::string name, const AttachmentInfo& info, RenderGraphResourceAccess access);


	std::vector<PassAttachment > color_attachments;
	std::vector<PassAttachment > depth_attachments;

	//true attachments by name
	std::unordered_map<std::string,PhysicalAttachment > real_attachments;
	vk::RenderPass built_pass;
	vk::Framebuffer framebuffer;

	std::function<ClearPassSet()> clear_callback;

	std::function<void(vk::CommandBuffer,RenderPass*)> draw_callback;

	int render_width = 0;
	int render_height = 0;
	bool bIsOutputPass = false;
	FrameGraph* owner;
	std::string name;
};

class FrameGraph {
public:
	struct FrameGraphPriv;
	struct GraphAttachment {
		AttachmentInfo info;
		int reads{0};
		int writes{0};
		bool bIsDepth{false};
		//bool bIsColor{ false };
		std::string name;


		int real_height;
		int real_width;
		vk::ImageUsageFlags usageFlags;
		VkSampler sampler;
		VkDescriptorImageInfo descriptor;
		VmaAllocation imageAlloc;
		vk::Image image;
		VkDeviceMemory mem;
		vk::ImageView view;
	};


	FrameGraph();
	void build(VkDevice device, VmaAllocator allocator);
	//void register_resource(std::string name, const AttachmentInfo &attachment);

	RenderPass * add_pass(std::string pass_name);

	void execute(vk::CommandBuffer cmd);

	FrameGraphPriv* priv;

	std::unordered_map<std::string, GraphAttachment > attachments;
	std::unordered_map<std::string, RenderPass > pass_definitions;

	VkExtent2D swapchainSize;

	std::vector<RenderPass*> passes;
};

void create_engine_graph(struct VulkanEngine* engine);
