#include <vector>
#include "vulkan/vulkan_core.h"

enum class ShaderType : char{
	Vertex,
	TessControl,
	TessEvaluation,
	Geometry,
	Fragment,
	Compute,
	RayGenNV,
	IntersectNV,
	AnyHitNV,
	ClosestHitNV,
	MissNV,
	CallableNV,
	TaskNV,
	MeshNV,
	Count
};

struct ShaderModule {
	ShaderType type;
	std::vector<unsigned int> SpirV;
};

struct ShaderCompileUnit {
	const char* path;
	bool bSuccess;
	ShaderModule resultModule;
};

struct VulkanProgramReflectionData {
	VkPushConstantRange pushConstantRange;
};

//holds all information for a given shader set for pipeline
struct ShaderEffect {

	std::vector< ShaderModule> modules;

	//pImpl
	struct ShaderEffectPrivateData* privData;
	//add a shader to the effect
	bool add_shader_from_file(const char* path);

	bool build_effect(VkDevice device);

	//returns a vkPipelineLayout
	VkPipelineLayout build_pipeline_layout(VkDevice device);

	std::array< VkDescriptorSetLayout, 4> build_descriptor_layouts(VkDevice device);
	std::vector<VkPipelineShaderStageCreateInfo> get_stage_infos();


	ShaderEffect();
	~ShaderEffect();
};

bool compile_shader(const char* path, ShaderModule* outModule);