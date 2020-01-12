#include <vector>
#include <unordered_map>
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

struct BindInfo {
	int set;
	int binding;
	int range;
	VkDescriptorType type;
};

struct BindInfoPushConstants {
	int size;
};
struct BindReflection {
	std::unordered_map<std::string, BindInfo> DataBindings;
	std::vector<BindInfoPushConstants> PushConstants;
};
//holds all information for a given shader set for pipeline
struct ShaderEffect {

	
	//pImpl
	struct ShaderEffectPrivateData* privData;
	//add a shader to the effect
	bool add_shader_from_file(const char* path);

	bool build_effect(VkDevice device);

	bool reload_shaders(VkDevice device);

	//returns a vkPipelineLayout
	VkPipelineLayout build_pipeline_layout(VkDevice device);

	std::array< VkDescriptorSetLayout, 4> build_descriptor_layouts(VkDevice device);
	std::vector<VkPipelineShaderStageCreateInfo> get_stage_infos();
	std::vector<std::string> get_loaded_shaders();
	BindReflection* get_reflection();


	ShaderEffect();
	~ShaderEffect();
};

bool compile_shader(const char* path, ShaderModule* outModule);