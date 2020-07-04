// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vulkan/vulkan.h>

namespace vkf {

	enum class ShaderStageFlagBits : VkShaderStageFlags
	{
		eVertex = VK_SHADER_STAGE_VERTEX_BIT,
		eTessellationControl = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		eTessellationEvaluation = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		eGeometry = VK_SHADER_STAGE_GEOMETRY_BIT,
		eFragment = VK_SHADER_STAGE_FRAGMENT_BIT,
		eCompute = VK_SHADER_STAGE_COMPUTE_BIT,
		eAllGraphics = VK_SHADER_STAGE_ALL_GRAPHICS,
		eAll = VK_SHADER_STAGE_ALL,
		eRaygenKHR = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
		eAnyHitKHR = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
		eClosestHitKHR = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		eMissKHR = VK_SHADER_STAGE_MISS_BIT_KHR,
		eIntersectionKHR = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
		eCallableKHR = VK_SHADER_STAGE_CALLABLE_BIT_KHR,
		eTaskNV = VK_SHADER_STAGE_TASK_BIT_NV,
		eMeshNV = VK_SHADER_STAGE_MESH_BIT_NV,
		eRaygenNV = VK_SHADER_STAGE_RAYGEN_BIT_NV,
		eAnyHitNV = VK_SHADER_STAGE_ANY_HIT_BIT_NV,
		eClosestHitNV = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV,
		eMissNV = VK_SHADER_STAGE_MISS_BIT_NV,
		eIntersectionNV = VK_SHADER_STAGE_INTERSECTION_BIT_NV,
		eCallableNV = VK_SHADER_STAGE_CALLABLE_BIT_NV
	};
	enum class DescriptorType
	{
		eSampler = VK_DESCRIPTOR_TYPE_SAMPLER,
		eCombinedImageSampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		eSampledImage = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		eStorageImage = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		eUniformTexelBuffer = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
		eStorageTexelBuffer = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
		eUniformBuffer = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		eStorageBuffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		eUniformBufferDynamic = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		eStorageBufferDynamic = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
		eInputAttachment = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
		eInlineUniformBlockEXT = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT,
		eAccelerationStructureKHR = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
		eAccelerationStructureNV = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV
	};

	enum class PipelineStageFlagBits : VkPipelineStageFlags
	{
		eTopOfPipe = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		eDrawIndirect = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
		eVertexInput = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		eVertexShader = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
		eTessellationControlShader = VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
		eTessellationEvaluationShader = VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
		eGeometryShader = VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
		eFragmentShader = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		eEarlyFragmentTests = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		eLateFragmentTests = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		eColorAttachmentOutput = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		eComputeShader = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		eTransfer = VK_PIPELINE_STAGE_TRANSFER_BIT,
		eBottomOfPipe = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		eHost = VK_PIPELINE_STAGE_HOST_BIT,
		eAllGraphics = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
		eAllCommands = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		eTransformFeedbackEXT = VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
		eConditionalRenderingEXT = VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT,
		eRayTracingShaderKHR = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		eAccelerationStructureBuildKHR = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		eShadingRateImageNV = VK_PIPELINE_STAGE_SHADING_RATE_IMAGE_BIT_NV,
		eTaskShaderNV = VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV,
		eMeshShaderNV = VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV,
		eFragmentDensityProcessEXT = VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT,
		eCommandPreprocessNV = VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
		eRayTracingShaderNV = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV,
		eAccelerationStructureBuildNV = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV
	};

	enum class AccessFlagBits : VkAccessFlags
	{
		eIndirectCommandRead = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
		eIndexRead = VK_ACCESS_INDEX_READ_BIT,
		eVertexAttributeRead = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
		eUniformRead = VK_ACCESS_UNIFORM_READ_BIT,
		eInputAttachmentRead = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
		eShaderRead = VK_ACCESS_SHADER_READ_BIT,
		eShaderWrite = VK_ACCESS_SHADER_WRITE_BIT,
		eColorAttachmentRead = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
		eColorAttachmentWrite = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		eDepthStencilAttachmentRead = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
		eDepthStencilAttachmentWrite = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		eTransferRead = VK_ACCESS_TRANSFER_READ_BIT,
		eTransferWrite = VK_ACCESS_TRANSFER_WRITE_BIT,
		eHostRead = VK_ACCESS_HOST_READ_BIT,
		eHostWrite = VK_ACCESS_HOST_WRITE_BIT,
		eMemoryRead = VK_ACCESS_MEMORY_READ_BIT,
		eMemoryWrite = VK_ACCESS_MEMORY_WRITE_BIT,
		eTransformFeedbackWriteEXT = VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,
		eTransformFeedbackCounterReadEXT = VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT,
		eTransformFeedbackCounterWriteEXT = VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
		eConditionalRenderingReadEXT = VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT,
		eColorAttachmentReadNoncoherentEXT = VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT,
		eAccelerationStructureReadKHR = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
		eAccelerationStructureWriteKHR = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		eShadingRateImageReadNV = VK_ACCESS_SHADING_RATE_IMAGE_READ_BIT_NV,
		eFragmentDensityMapReadEXT = VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,
		eCommandPreprocessReadNV = VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV,
		eCommandPreprocessWriteNV = VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV,
		eAccelerationStructureReadNV = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV,
		eAccelerationStructureWriteNV = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV
	};
	enum class DependencyFlagBits : VkDependencyFlags
	{
		eByRegion = VK_DEPENDENCY_BY_REGION_BIT,
		eDeviceGroup = VK_DEPENDENCY_DEVICE_GROUP_BIT,
		eViewLocal = VK_DEPENDENCY_VIEW_LOCAL_BIT,
		eViewLocalKHR = VK_DEPENDENCY_VIEW_LOCAL_BIT_KHR,
		eDeviceGroupKHR = VK_DEPENDENCY_DEVICE_GROUP_BIT_KHR
	};
	enum class SampleCountFlagBits : VkSampleCountFlags
	{
		e1 = VK_SAMPLE_COUNT_1_BIT,
		e2 = VK_SAMPLE_COUNT_2_BIT,
		e4 = VK_SAMPLE_COUNT_4_BIT,
		e8 = VK_SAMPLE_COUNT_8_BIT,
		e16 = VK_SAMPLE_COUNT_16_BIT,
		e32 = VK_SAMPLE_COUNT_32_BIT,
		e64 = VK_SAMPLE_COUNT_64_BIT
	};

	enum class AttachmentLoadOp : uint32_t
	{
		eLoad = VK_ATTACHMENT_LOAD_OP_LOAD,
		eClear = VK_ATTACHMENT_LOAD_OP_CLEAR,
		eDontCare = VK_ATTACHMENT_LOAD_OP_DONT_CARE
	};
	enum class AttachmentStoreOp : uint32_t
	{
		eStore = VK_ATTACHMENT_STORE_OP_STORE,
		eDontCare = VK_ATTACHMENT_STORE_OP_DONT_CARE
	};

	enum class ImageLayout
	{
		eUndefined = VK_IMAGE_LAYOUT_UNDEFINED,
		eGeneral = VK_IMAGE_LAYOUT_GENERAL,
		eColorAttachmentOptimal = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		eDepthStencilAttachmentOptimal = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		eDepthStencilReadOnlyOptimal = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		eShaderReadOnlyOptimal = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		eTransferSrcOptimal = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		eTransferDstOptimal = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		ePreinitialized = VK_IMAGE_LAYOUT_PREINITIALIZED,
		eDepthReadOnlyStencilAttachmentOptimal = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
		eDepthAttachmentStencilReadOnlyOptimal = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
		eDepthAttachmentOptimal = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		eDepthReadOnlyOptimal = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
		eStencilAttachmentOptimal = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
		eStencilReadOnlyOptimal = VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL,
		ePresentSrcKHR = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		eSharedPresentKHR = VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR,
		eShadingRateOptimalNV = VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV,
		eFragmentDensityMapOptimalEXT = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,
		eDepthReadOnlyStencilAttachmentOptimalKHR = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR,
		eDepthAttachmentStencilReadOnlyOptimalKHR = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR,
		eDepthAttachmentOptimalKHR = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR,
		eDepthReadOnlyOptimalKHR = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL_KHR,
		eStencilAttachmentOptimalKHR = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL_KHR,
		eStencilReadOnlyOptimalKHR = VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL_KHR
	};

	 enum class PipelineBindPoint
  {
    eGraphics = VK_PIPELINE_BIND_POINT_GRAPHICS,
    eCompute = VK_PIPELINE_BIND_POINT_COMPUTE,
    eRayTracingKHR = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
    eRayTracingNV = VK_PIPELINE_BIND_POINT_RAY_TRACING_NV
  };
	 enum class ImageAspectFlagBits : VkImageAspectFlags
	 {
		 eColor = VK_IMAGE_ASPECT_COLOR_BIT,
		 eDepth = VK_IMAGE_ASPECT_DEPTH_BIT,
		 eStencil = VK_IMAGE_ASPECT_STENCIL_BIT,
		 eMetadata = VK_IMAGE_ASPECT_METADATA_BIT,
		 ePlane0 = VK_IMAGE_ASPECT_PLANE_0_BIT,
		 ePlane1 = VK_IMAGE_ASPECT_PLANE_1_BIT,
		 ePlane2 = VK_IMAGE_ASPECT_PLANE_2_BIT,
		 eMemoryPlane0EXT = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT,
		 eMemoryPlane1EXT = VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT,
		 eMemoryPlane2EXT = VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT,
		 eMemoryPlane3EXT = VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT,
		 ePlane0KHR = VK_IMAGE_ASPECT_PLANE_0_BIT_KHR,
		 ePlane1KHR = VK_IMAGE_ASPECT_PLANE_1_BIT_KHR,
		 ePlane2KHR = VK_IMAGE_ASPECT_PLANE_2_BIT_KHR
	 };

	 enum class ImageUsageFlagBits : VkImageUsageFlags
	 {
		 eTransferSrc = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		 eTransferDst = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		 eSampled = VK_IMAGE_USAGE_SAMPLED_BIT,
		 eStorage = VK_IMAGE_USAGE_STORAGE_BIT,
		 eColorAttachment = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		 eDepthStencilAttachment = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		 eTransientAttachment = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
		 eInputAttachment = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
		 eShadingRateImageNV = VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV,
		 eFragmentDensityMapEXT = VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT
	 };
	 enum class Filter
	 {
		 eNearest = VK_FILTER_NEAREST,
		 eLinear = VK_FILTER_LINEAR,
		 eCubicIMG = VK_FILTER_CUBIC_IMG,
		 eCubicEXT = VK_FILTER_CUBIC_EXT
	 };
	 enum class SamplerMipmapMode
	 {
		 eNearest = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		 eLinear = VK_SAMPLER_MIPMAP_MODE_LINEAR
	 };
	 enum class SamplerAddressMode
	 {
		 eRepeat = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		 eMirroredRepeat = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
		 eClampToEdge = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		 eClampToBorder = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		 eMirrorClampToEdge = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
		 eMirrorClampToEdgeKHR = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE_KHR
	 };
}

inline VkImageLayout operator+(const vkf::ImageLayout l) {
	return static_cast<VkImageLayout>(l);
};
inline VkImageAspectFlagBits operator+(const vkf::ImageAspectFlagBits l) {
	return static_cast<VkImageAspectFlagBits>(l);
};
inline VkAccessFlagBits operator+(const vkf::AccessFlagBits l) {
	return static_cast<VkAccessFlagBits>(l);
};
inline VkPipelineStageFlagBits operator+(const vkf::PipelineStageFlagBits l) {
	return static_cast<VkPipelineStageFlagBits>(l);
}
inline VkPipelineBindPoint operator+(const vkf::PipelineBindPoint l) {
	return static_cast<VkPipelineBindPoint>(l);
};
inline VkImageUsageFlagBits operator+(const vkf::ImageUsageFlagBits l) {
	return static_cast<VkImageUsageFlagBits>(l);
};

inline VkFilter operator+(const vkf::Filter l) {
	return static_cast<VkFilter>(l);
};
inline VkSamplerMipmapMode operator+(const vkf::SamplerMipmapMode l) {
	return static_cast<VkSamplerMipmapMode>(l);
};
inline VkSamplerAddressMode operator+(const vkf::SamplerAddressMode l) {
	return static_cast<VkSamplerAddressMode>(l);
};
inline VkSampleCountFlagBits operator+(const vkf::SampleCountFlagBits l) {
	return static_cast<VkSampleCountFlagBits>(l);
};


//inline constexpr vkf::ImageUsageFlagBits operator|(vkf::ImageUsageFlagBits bit0, vkf::ImageUsageFlagBits bit1) 
//{
//	return (bit0) | bit1;
//}


inline bool operator==(const VkDescriptorType& tA, const vkf::DescriptorType& tB) {
	return tA == static_cast<VkDescriptorType>(tB);
}
