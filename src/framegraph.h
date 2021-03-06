#pragma once
#include <pcheader.h>
#include <functional>
#include <array>
#include "frame_resource.h"
#include <vulkan_types.h>
enum class RenderGraphResourceLifetime {
	PerFrame,
	Persistent
};
enum class RenderGraphResourceAccess {
	Write,
	Read
};
enum class RenderGraphImageAccess {
	Write,
	Read,
	RenderTargetColor,
	RenderTargetDepth
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

struct CachedImageView {
	VkImageViewCreateInfo createInfo;
	VkImageView imageView;
};

struct CachedImage {
	AllocatedImage image;
	VkImageCreateInfo createInfo;
	std::vector<CachedImageView> imageViews;
};



//bundled up render pass create info. NOT COPYABLE, allways use by pointer
struct RenderPassCreateInfo {
	VkRenderPassCreateInfo topInfo;
	VkSubpassDescription subpass;
	std::array < VkSubpassDependency, 2> pass_dependencies;
	std::array< VkAttachmentDescription, 6> attachments;
	std::array< VkAttachmentReference, 6> attachment_refs;
	int numAttachments;
	int depthAttachments;

	VkRenderPassCreateInfo makeInfo();

	bool operator==(const RenderPassCreateInfo& other) const {
		if (numAttachments != other.numAttachments) return false;
		if (depthAttachments != other.depthAttachments) return false;
		if (topInfo.attachmentCount != other.topInfo.attachmentCount) return false;
		for (int i = 0; i < topInfo.attachmentCount;i++) {
			bool bIsSame = memcmp(&attachments[i], &other.attachments[i], sizeof(VkAttachmentDescription)) == 0;
			if (!bIsSame) return false;
		}

		return true;
	}
};

struct CachedRenderPass {
	std::unique_ptr<RenderPassCreateInfo> createinfo;
	VkRenderPass pass;
};

std::unique_ptr<RenderPassCreateInfo> bundle_renderpass_info(VkRenderPassCreateInfo* info);

struct FrameGraphCache {
	std::unordered_map<uint64_t, CachedImage> cachedImages;

	std::vector<CachedRenderPass> cachedRenderPasses;

	std::vector<uint64_t> reusableImages;

	uint64_t last_index{1};

	VkRenderPass requestRenderPass(class VulkanEngine* eng, VkRenderPassCreateInfo* info);
};

struct RenderGraphImageInfo {
	VkFormat format{ VK_FORMAT_UNDEFINED };
	VkClearValue clearValue;
};

struct RenderAttachmentInfo
{
	SizeClass size_class{ SizeClass::SwapchainRelative };
	float size_x{ 1.0f };
	float size_y{ 1.0f };
	VkFormat format{ VK_FORMAT_UNDEFINED };
	VkClearValue clearValue;
	//std::string name;
	bool bClear{ false };

	void set_clear_color(std::array<float, 4> col);
	void set_clear_depth(float depth, uint32_t stencil);
};
struct RenderPassCommands {
	VkCommandBuffer commandBuffer;
	class RenderPass* renderPass;
	struct CommandEncoder* commandEncoder;
	void* profilerContext;
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
	struct ImageAttachment {
		std::string name;
		RenderGraphResourceAccess access;
	};
	std::vector<VkClearValue> clearValues;
	std::vector<ImageAttachment> image_dependencies;
	std::vector<PassAttachment > color_attachments;
	std::vector<PhysicalAttachment> physical_attachments;

	PassAttachment depth_attachment;

	VkRenderPass built_pass{ VK_NULL_HANDLE };
	VkFramebuffer framebuffer;

	std::function<void(RenderPassCommands*)> draw_callback;

	int render_width = 0;
	int render_height = 0;
	class FrameGraph* owner;
	std::string name;
	PassType type;
	bool perform_submit;
	std::vector<VkImageMemoryBarrier> startBarriers;
	std::vector<VkImageMemoryBarrier> endBarriers;
	//find image in local resources and check how its used 
	bool find_image_access(std::string name, RenderGraphImageAccess& outAccess);
	

	void add_image_dependency(std::string name,RenderGraphResourceAccess accessMode = RenderGraphResourceAccess::Read);

	void add_color_attachment(std::string name, const RenderAttachmentInfo& info);
	void set_depth_attachment(std::string name, const RenderAttachmentInfo& info);
};
class VulkanEngine;



class FrameGraph {
public:
	struct GraphAttachmentUsages {
		//points to array of passes in framegraph
		std::vector<int> users;
		std::vector<RenderGraphImageAccess> usages;
	};
	struct GraphAttachment {
		RenderAttachmentInfo info;

		std::string name;
		int real_height;
		int real_width;
		VkImageUsageFlags usageFlags;
		VkDescriptorImageInfo descriptor;
		uint64_t image_id{0};
		//VmaAllocation imageAlloc;
		//VkImage image;
		//VkDeviceMemory mem;

		GraphAttachmentUsages uses;
		
		VkImage get_image(FrameGraph* ownerGraph);
		
	};

	int find_attachment_user(const FrameGraph::GraphAttachmentUsages& usages, std::string name);
	 
	void create_render_pass(int color_attachments, VkAttachmentReference* references, int depth_attachments, VkAttachmentDescription* attachments, RenderPass* pass, VulkanEngine* eng);
	void build_render_pass(RenderPass* pass, VulkanEngine* eng);
	void build_compute_barriers(RenderPass* pass, VulkanEngine* eng);

	void return_image(uint64_t image_ID);
	uint64_t request_image(VkImageCreateInfo* createInfo);
	VkImageView request_image_view(uint64_t image_ID, VkImageViewCreateInfo* createInfo);
	

	bool build(VulkanEngine* engine);

	void return_images();

	void reset(VulkanEngine* engine);

	RenderPass* add_pass(std::string pass_name, std::function<void(RenderPassCommands*)> execution, PassType type, bool bPerformSubmit = false);
	RenderPass* get_pass(std::string name);
	VkDescriptorImageInfo get_image_descriptor(std::string name);
	GraphAttachment* get_attachment(std::string name);
	
	VkCommandBuffer create_graphics_buffer(int threadID);
	void execute(VkCommandBuffer cmd);

	void transform_images_to_write(RenderPass* pass, std::vector<VkImageMemoryBarrier>& image_barriers, VkCommandBuffer& cmd);

	void transform_images_to_read(RenderPass* pass, std::vector<VkImageMemoryBarrier>& image_barriers, VkCommandBuffer& cmd);

	void submit_commands(VkCommandBuffer cmd, int wait_pass_index, int signal_pass_index);

	void build_command_pools();

	int get_reads(const GraphAttachment* attachment);
	int get_writes(const GraphAttachment* attachment);
	bool is_used_as_depth(const GraphAttachment* attachment);

	std::vector<RenderPass*> passes;
	std::unordered_map<std::string, RenderPass > pass_definitions;
	std::unordered_map<std::string, GraphAttachment> graph_attachments;

	FrameResource < std::array<VkCommandPool,4> , 3 > commandPools;
	FrameResource< std::array<std::vector<VkCommandBuffer>, 4>, 3>  usableCommands;
	FrameResource< std::array<std::vector<VkCommandBuffer>, 4>, 3>  pendingCommands;
	VkSampler mainSampler{VK_NULL_HANDLE};

	FrameGraphCache cache;

	VkExtent2D swapchainSize;
	std::vector<const char*> attachmentNames;
	VulkanEngine* owner;
	int current_submission_id = 0;
	int end_render_id = 0;
};


struct PassBuilder {
	class Rendergraph* render_graph;

	
	void set_pass_type(PassType type) {};
	void set_sort_key(uint64_t key) {};

	void request_image(std::string_view name, const RenderGraphImageInfo& default_info) {};
	void request_buffer(std::string_view name, RenderGraphResourceAccess resource_access) {};

	void add_color_attachment(std::string_view name, const RenderAttachmentInfo& info) {};
	void set_depth_attachment(std::string_view name, const RenderAttachmentInfo& info) {};
};

struct RenderPassDefinition{
	std::string name;

	std::function<void(PassBuilder*)> setup;
	std::function<void(RenderPassCommands*, void* passData)> execute;
};


class Rendergraph {

	std::vector<std::unique_ptr<RenderPassDefinition>> passes;


	void add_pass(RenderPassDefinition&& pass_definition) {};
};

std::array < VkSubpassDependency, 2> build_basic_subpass_dependencies();

