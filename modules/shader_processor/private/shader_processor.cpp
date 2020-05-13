

#include "shader_processor.h"

#include <glslang/public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <StandAlone/DirStackFileIncluder.h>

#include "spirv_cross.hpp"

#include <iostream>
#include <fstream>
#include <string>

bool load_file_to_string(const char* path, std::string& contents) {
    std::ifstream fstream(path, std::ios::in);
    if (fstream) {
        
        fstream.seekg(0, std::ios::end);
        contents.reserve((std::string::size_type)fstream.tellg());
        fstream.seekg(0, std::ios::beg);
        contents.assign((std::istreambuf_iterator<char>(fstream)),
            std::istreambuf_iterator<char>());
        return true;
    }
    return false;
}

std::string get_file_path(const std::string& str)
{
    size_t found = str.find_last_of("/\\");
    return str.substr(0, found);
    //size_t FileName = str.substr(found+1);
}

std::string get_suffix(const std::string& name)
{
    const size_t pos = name.rfind('.');
    return (pos == std::string::npos) ? "" : name.substr(name.rfind('.') + 1);
}
EShLanguage get_shader_stage(const std::string& stage)
{
    if (stage == "vert") {
        return EShLangVertex;
    }
    else if (stage == "tesc") {
        return EShLangTessControl;
    }
    else if (stage == "tese") {
        return EShLangTessEvaluation;
    }
    else if (stage == "geom") {
        return EShLangGeometry;
    }
    else if (stage == "frag") {
        return EShLangFragment;
    }
    else if (stage == "comp") {
        return EShLangCompute;
    }
    else {
        //assert(0 && "Unknown shader stage");
        return EShLangCount;
    }
}
static bool glslangInitialized = false;

const TBuiltInResource DefaultTBuiltInResource = {
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationImageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxGeometryVaryingComponents = */ 64,
    /* .MaxTessControlInputComponents = */ 128,
    /* .MaxTessControlOutputComponents = */ 128,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationInputComponents = */ 128,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessPatchComponents = */ 120,
    /* .MaxPatchVertices = */ 32,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .maxMeshOutputVerticesNV = */ 256,
    /* .maxMeshOutputPrimitivesNV = */ 512,
    /* .maxMeshWorkGroupSizeX_NV = */ 32,
    /* .maxMeshWorkGroupSizeY_NV = */ 1,
    /* .maxMeshWorkGroupSizeZ_NV = */ 1,
    /* .maxTaskWorkGroupSizeX_NV = */ 32,
    /* .maxTaskWorkGroupSizeY_NV = */ 1,
    /* .maxTaskWorkGroupSizeZ_NV = */ 1,
    /* .maxMeshViewCountNV = */ 4,

    /* .limits = */ {
    /* .nonInductiveForLoops = */ 1,
    /* .whileLoops = */ 1,
    /* .doWhileLoops = */ 1,
    /* .generalUniformIndexing = */ 1,
    /* .generalAttributeMatrixVectorIndexing = */ 1,
    /* .generalVaryingIndexing = */ 1,
    /* .generalSamplerIndexing = */ 1,
    /* .generalVariableIndexing = */ 1,
    /* .generalConstantMatrixVectorIndexing = */ 1,
} };


bool compile_string(const char* InputCString, EShLanguage ShaderType, std::string IncludePath, ShaderModule* output) {
   // const char* InputCString = shader_str.c_str();

    //EShLanguage ShaderType = get_shader_stage(get_suffix(path));
    glslang::TShader Shader(ShaderType);

    Shader.setStrings(&InputCString, 1);

    int ClientInputSemanticsVersion = 100; // maps to, say, #define VULKAN 100
    glslang::EShTargetClientVersion VulkanClientVersion = glslang::EShTargetVulkan_1_0;
    glslang::EShTargetLanguageVersion TargetVersion = glslang::EShTargetSpv_1_0;

    Shader.setEnvInput(glslang::EShSourceGlsl, ShaderType, glslang::EShClientVulkan, ClientInputSemanticsVersion);
    Shader.setEnvClient(glslang::EShClientVulkan, VulkanClientVersion);
    Shader.setEnvTarget(glslang::EShTargetSpv, TargetVersion);


    TBuiltInResource Resources;
    Resources = DefaultTBuiltInResource;
    EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

    const int DefaultVersion = 100;

    DirStackFileIncluder Includer;

    Includer.pushExternalLocalDirectory(IncludePath);

    //preprocess
    std::string PreprocessedGLSL;

    if (!Shader.preprocess(&Resources, DefaultVersion, ENoProfile, false, false, messages, &PreprocessedGLSL, Includer))
    {
        std::cout << "GLSL Preprocessing Failed for: " << std::endl;// << filename << std::endl;
        std::cout << Shader.getInfoLog() << std::endl;
        std::cout << Shader.getInfoDebugLog() << std::endl;
        return false;
    }

    const char* PreprocessedCStr = PreprocessedGLSL.c_str();
    Shader.setStrings(&PreprocessedCStr, 1);

    if (!Shader.parse(&Resources, 100, false, messages))
    {
        std::cout << "GLSL Parsing Failed for: " << std::endl;// << filename << std::endl;
        std::cout << Shader.getInfoLog() << std::endl;
        std::cout << Shader.getInfoDebugLog() << std::endl;
        return false;
    }

    glslang::TProgram Program;
    Program.addShader(&Shader);

    if (!Program.link(messages))
    {
        std::cout << "GLSL Linking Failed for: " << std::endl;// << filename << std::endl;
        std::cout << Shader.getInfoLog() << std::endl;
        std::cout << Shader.getInfoDebugLog() << std::endl;
        return false;
    }

    spv::SpvBuildLogger logger;
    glslang::SpvOptions spvOptions;

    //finalize shader compilation
    glslang::GlslangToSpv(*Program.getIntermediate(ShaderType), output->SpirV, &logger, &spvOptions);

    //read spirv
    spirv_cross::Compiler comp(output->SpirV); 

    //reflect resources
    spirv_cross::ShaderResources res = comp.get_shader_resources();

    using namespace spirv_cross;
    for (const Resource& resource : res.uniform_buffers)
    {
        auto& type = comp.get_type(resource.base_type_id);
        auto member_count = type.member_types.size();

        auto struct_name = comp.get_name(resource.base_type_id);

        auto set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
        auto binding = comp.get_decoration(resource.id, spv::DecorationBinding);

        std::cout << "Struct reflected: " << struct_name << " Set:" << set << " binding:" << binding << "----------------"<<std::endl;
        for (unsigned i = 0; i < member_count; i++)
        {
            auto& member_type = comp.get_type(type.member_types[i]);
            size_t member_size = comp.get_declared_struct_member_size(type, i);

            // Get member offset within this struct.
            size_t offset = comp.type_struct_member_offset(type, i);

            if (!member_type.array.empty())
            {
                // Get array stride, e.g. float4 foo[]; Will have array stride of 16 bytes.
                size_t array_stride = comp.type_struct_member_array_stride(type, i);
            }

            if (member_type.columns > 1)
            {
                // Get bytes stride between columns (if column major), for float4x4 -> 16 bytes.
                size_t matrix_stride = comp.type_struct_member_matrix_stride(type, i);
            }
            const std::string& name = comp.get_member_name(type.self, i);

            std::cout << "member: " << name << std::endl;
            std::cout << "   type: " <<member_type.basetype << " member_size: " << member_size  <<" offset : " << offset << std::endl;
        }
    }

    for (const Resource& resource : res.sampled_images)
    {
        auto& type = comp.get_type(resource.base_type_id);
        unsigned member_count = type.member_types.size();

        auto struct_name = comp.get_name(resource.base_type_id);

        unsigned set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
        unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);

        std::cout << "Struct reflected: " << struct_name << " Set:" << set << " binding:" << binding << "----------------" << std::endl;
        for (unsigned i = 0; i < member_count; i++)
        {
            auto& member_type = comp.get_type(type.member_types[i]);
            size_t member_size = comp.get_declared_struct_member_size(type, i);

            // Get member offset within this struct.
            size_t offset = comp.type_struct_member_offset(type, i);

            if (!member_type.array.empty())
            {
                // Get array stride, e.g. float4 foo[]; Will have array stride of 16 bytes.
                size_t array_stride = comp.type_struct_member_array_stride(type, i);
            }

            if (member_type.columns > 1)
            {
                // Get bytes stride between columns (if column major), for float4x4 -> 16 bytes.
                size_t matrix_stride = comp.type_struct_member_matrix_stride(type, i);
            }
            const std::string& name = comp.get_member_name(type.self, i);

            std::cout << "member: " << name << std::endl;
            std::cout << "   type: " << member_type.basetype << " member_size: " << member_size << " offset : " << offset << std::endl;
        }
    }

    return true;
}

bool compile_shader(const char* path, ShaderModule* outModule)
{
    if (!glslangInitialized)
    {
        glslang::InitializeProcess();
        glslangInitialized = true;
    }

    std::string filename = path;
    std::string shader_str;
    if (load_file_to_string(path, shader_str)) {

         const char* InputCString = shader_str.c_str();
         EShLanguage ShaderType = get_shader_stage(get_suffix(path));
         std::string Path = get_file_path(path);

         return compile_string(InputCString, ShaderType, Path, outModule);
    }

	return false;
}





struct ShaderDescriptorBindings {
    std::vector<VkDescriptorSetLayoutBinding> descriptorBindings;
    std::vector<VkDescriptorBindingFlags> descriptorFlags;
};

void add_binding(ShaderDescriptorBindings* bindings, VkDescriptorSetLayoutBinding newBind, VkDescriptorBindingFlags flags = 0) {

    //merge stage
    for (VkDescriptorSetLayoutBinding& bind : bindings->descriptorBindings) {
        if (bind.binding == newBind.binding)
        {
            assert(bind.descriptorType == newBind.descriptorType);

            bind.stageFlags |= newBind.stageFlags;
            return;
        }
    }

    bindings->descriptorBindings.push_back(newBind);
    bindings->descriptorFlags.push_back(flags);
}

struct ShaderEffectPrivateData {
	std::vector< ShaderModule> modules;
	std::vector<std::string> loaded_shaders;

    std::vector< glslang::TShader*> Shaders;
    std::vector< VkPipelineShaderStageCreateInfo> ShaderStages;

    std::array<ShaderDescriptorBindings,4> bindingSets;
    std::vector< VkPushConstantRange> pushConstantRanges;

    std::array< VkDescriptorSetLayout, 4> builtSetLayouts;
    bool bSetLayoutsBuilt {false} ;
    VkPipelineLayout builtPipelineLayout;
    bool bPipelineLayoutsBuilt{ false };

    BindReflection reflectionData;

    glslang::TProgram Program;
    VkDevice device;
};

bool ShaderEffect::add_shader_from_file(const char* path)
{
    ShaderModule mod;

    if (!glslangInitialized)
    {
        glslang::InitializeProcess();
        glslangInitialized = true;
    }

    std::string filename = path;
    std::string shader_str;
    if (load_file_to_string(path, shader_str)) {

        const char* InputCString = shader_str.c_str();
        EShLanguage ShaderType = get_shader_stage(get_suffix(path));
        std::string IncludePath = get_file_path(path);

        glslang::TShader* PtrShader = new glslang::TShader(ShaderType);

        glslang::TShader &Shader = *PtrShader;

        Shader.setStrings(&InputCString, 1);

        int ClientInputSemanticsVersion = 100; // maps to, say, #define VULKAN 100
        glslang::EShTargetClientVersion VulkanClientVersion = glslang::EShTargetVulkan_1_0;
        glslang::EShTargetLanguageVersion TargetVersion = glslang::EShTargetSpv_1_0;

        Shader.setEnvInput(glslang::EShSourceGlsl, ShaderType, glslang::EShClientVulkan, ClientInputSemanticsVersion);
        Shader.setEnvClient(glslang::EShClientVulkan, VulkanClientVersion);
        Shader.setEnvTarget(glslang::EShTargetSpv, TargetVersion);

        TBuiltInResource Resources;
        Resources = DefaultTBuiltInResource;
        EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

        const int DefaultVersion = 100;

        DirStackFileIncluder Includer;

        Includer.pushExternalLocalDirectory(IncludePath);

        //preprocess
        std::string PreprocessedGLSL;

        if (!Shader.preprocess(&Resources, DefaultVersion, ENoProfile, false, false, messages, &PreprocessedGLSL, Includer))
        {
            std::cout << "GLSL Preprocessing Failed for: "<< path << std::endl;// << filename << std::endl;
            std::cout << Shader.getInfoLog() << std::endl;
            std::cout << Shader.getInfoDebugLog() << std::endl;
            delete PtrShader;
            return false;
        }

        const char* PreprocessedCStr = PreprocessedGLSL.c_str();
        Shader.setStrings(&PreprocessedCStr, 1);

        if (!Shader.parse(&Resources, 100, false, messages))
        {
            std::cout << "GLSL Parsing Failed for: " << path << std::endl;// << filename << std::endl;
            std::cout << Shader.getInfoLog() << std::endl;
            std::cout << Shader.getInfoDebugLog() << std::endl;
            delete PtrShader;
            return false;
        }
        
       
        privData->Shaders.push_back(PtrShader);
        privData->Program.addShader(PtrShader);

        privData->loaded_shaders.push_back(path);

        return true;
    }

    return false;
}
bool ShaderEffect::reload_shaders(VkDevice device) {
    //save old state to be able to recover
    std::vector< ShaderModule> oldmodules = privData->modules;

    ShaderEffectPrivateData* oldPrivData = privData;
	std::vector<std::string> shaders = privData->loaded_shaders;

    privData->loaded_shaders.clear();
    privData->modules.clear();
    //clear internal state
    privData = new ShaderEffectPrivateData;
   

    
    
    for (auto& s : shaders) {
        if (!add_shader_from_file(s.c_str()))
        {
            goto failure;
        }
    }

    if (!build_effect(device))
    {
        goto failure;
    }

    std::cout << "Succesful shader reload" << std::endl;
    //everything working like a charm

    return true;
failure:
    delete privData;
    privData->modules = oldmodules;
    privData = oldPrivData;
    privData->loaded_shaders = shaders;

    return false;
}

VkPipelineBindPoint ShaderEffect::get_bind_point()
{
    if (privData->modules.size() == 1) {
        return VK_PIPELINE_BIND_POINT_COMPUTE;
    }
    else{
        return VK_PIPELINE_BIND_POINT_GRAPHICS;
    }
}

VkShaderStageFlagBits ShaderTypeToVulkanFlag(ShaderType type) {
    switch (type) {

    case ShaderType::Vertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case ShaderType::Fragment:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case ShaderType::Compute:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    default:
        return VK_SHADER_STAGE_ALL;
    }
    return VK_SHADER_STAGE_ALL;
}

VkDescriptorSetLayoutBinding createLayoutBinding(BindInfo bindInfo, VkShaderStageFlags stages) {
	VkDescriptorSetLayoutBinding binding;
	binding.binding = bindInfo.binding;
	binding.descriptorCount = 1;
	binding.stageFlags = stages;
	binding.pImmutableSamplers = nullptr;
    binding.descriptorType = bindInfo.type;
	return binding;
}

VkDescriptorSetLayoutBinding createLayoutBinding(int bindNumber, VkShaderStageFlags stages) {
    VkDescriptorSetLayoutBinding binding;
    binding.binding = bindNumber;
    binding.descriptorCount = 1;
    binding.stageFlags = stages;
    binding.pImmutableSamplers = nullptr;
    return binding;
}



bool ShaderEffect::build_effect(VkDevice device)
{
    privData->device = device;
    glslang::TProgram &Program = privData->Program;

    EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

    if (!Program.link(messages))
    {
        std::cout << "GLSL Linking Failed for: " << std::endl;// << filename << std::endl;
        std::cout << Program.getInfoLog() << std::endl;
        std::cout << Program.getInfoDebugLog() << std::endl;
        return false;
    }

    spv::SpvBuildLogger logger;
    glslang::SpvOptions spvOptions;

    

    for (int i = 0; i < (char)ShaderType::Count; i++) {

        glslang::TIntermediate* shader_stage = Program.getIntermediate((EShLanguage)i);
        if (shader_stage) {
            ShaderModule newModule;
            newModule.type = (ShaderType)i;
            //finalize shader compilation
            glslang::GlslangToSpv(*shader_stage, newModule.SpirV, &logger, &spvOptions);
           


            VkShaderModuleCreateInfo createInfo;
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.pNext = nullptr;
            createInfo.flags = 0;
            createInfo.codeSize = newModule.SpirV.size()*4;
            createInfo.pCode = reinterpret_cast<const uint32_t*>(newModule.SpirV.data());

            VkShaderModule VkModule;

            vkCreateShaderModule(device, &createInfo, nullptr, &VkModule);           
            
            VkPipelineShaderStageCreateInfo shaderCreateInfo;
            shaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderCreateInfo.pNext = nullptr;
            shaderCreateInfo.flags = 0;
           
            shaderCreateInfo.module = VkModule;
            shaderCreateInfo.pName = "main";
            shaderCreateInfo.pSpecializationInfo = nullptr;
            shaderCreateInfo.stage = ShaderTypeToVulkanFlag(newModule.type);


            privData->ShaderStages.push_back(shaderCreateInfo);

            privData->modules.push_back(std::move(newModule));
        }       
    }

    for (ShaderModule &ShaderMod : privData->modules)
    {        
        //read spirv
        spirv_cross::Compiler comp(ShaderMod.SpirV);

        //reflect resources
        spirv_cross::ShaderResources res = comp.get_shader_resources();

        using namespace spirv_cross;
        for (const Resource& resource : res.uniform_buffers)
        {
            const SPIRType& type = comp.get_type(resource.base_type_id);
            size_t size = comp.get_declared_struct_size(type);
            unsigned set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
            unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);

            VkDescriptorSetLayoutBinding vkbind = createLayoutBinding(binding, ShaderTypeToVulkanFlag(ShaderMod.type));
            vkbind.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

            add_binding(&privData->bindingSets[set], vkbind);

            //privData->bindingSets[set].descriptorBindings.push_back(vkbind);

            BindInfo newinfo;
            newinfo.set = set;
            newinfo.binding = binding;
            newinfo.range = (int)size;;
            newinfo.type = vkbind.descriptorType;

            privData->reflectionData.DataBindings[comp.get_name(resource.id)] = newinfo;
        }

        for (const Resource& resource : res.storage_buffers)
        {
            const SPIRType& type = comp.get_type(resource.base_type_id);
            size_t size = comp.get_declared_struct_size(type);
            unsigned set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
            unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);


            VkDescriptorSetLayoutBinding vkbind = createLayoutBinding(binding, ShaderTypeToVulkanFlag(ShaderMod.type));
            vkbind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;           

            add_binding(&privData->bindingSets[set], vkbind);

            BindInfo newinfo;
            newinfo.set = set;
            newinfo.binding = binding;
            newinfo.range = (int)size;
            newinfo.type = vkbind.descriptorType;

            privData->reflectionData.DataBindings[comp.get_name(resource.id)] = newinfo;
        }

        for (const Resource& resource : res.storage_images) {
			BindInfo newinfo;
			newinfo.set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
			newinfo.binding = comp.get_decoration(resource.id, spv::DecorationBinding);
			newinfo.range = 0;
			newinfo.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

			VkDescriptorSetLayoutBinding vkbind = createLayoutBinding(newinfo, ShaderTypeToVulkanFlag(ShaderMod.type));		

			add_binding(&privData->bindingSets[newinfo.set], vkbind);

			privData->reflectionData.DataBindings[comp.get_name(resource.id)] = newinfo;
        }
        for (const Resource& resource : res.sampled_images)
        {
            const SPIRType& type = comp.get_type(resource.base_type_id);

            const SPIRType& array_type = comp.get_type(resource.type_id);

            bool is_bindless_array = false;
            if (array_type.array.size() > 0) {
                if (array_type.array.front() == 0) {
                    is_bindless_array = true;
               }
            }

			BindInfo newinfo;
			newinfo.set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
			newinfo.binding = comp.get_decoration(resource.id, spv::DecorationBinding);
			newinfo.range = 0;
            newinfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

            VkDescriptorBindingFlags flasgs = 0;
            if (is_bindless_array)
            {
                newinfo.array_len = 0;
                flasgs = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
                
            }

			//if (type.image.dim == spv::Dim::DimCube)
			//{
			//    newinfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			//}
			//else {
			//    newinfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			//}			

			VkDescriptorSetLayoutBinding vkbind = createLayoutBinding(newinfo, ShaderTypeToVulkanFlag(ShaderMod.type));

            if (is_bindless_array)
            {
                vkbind.descriptorCount = 4096;
            }

			add_binding(&privData->bindingSets[newinfo.set], vkbind, flasgs);

			privData->reflectionData.DataBindings[comp.get_name(resource.id)] = newinfo;
        }

		//for (const Resource& resource : res.acceleration_structures)
		//{
		//	BindInfo newinfo;
		//	newinfo.set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
		//	newinfo.binding = comp.get_decoration(resource.id, spv::DecorationBinding);
		//	newinfo.range = 0;
		//	newinfo.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        //
		//	VkDescriptorSetLayoutBinding vkbind = createLayoutBinding(newinfo, ShaderTypeToVulkanFlag(ShaderMod.type));
        //
		//	add_binding(&privData->bindingSets[newinfo.set], vkbind);            		
        //
        //    privData->reflectionData.DataBindings[comp.get_name(resource.id)] = newinfo;
		//}

        VkShaderStageFlags stage_flags{};

        for (const ShaderModule& mod : privData->modules)
        {     
            stage_flags |= ShaderTypeToVulkanFlag(mod.type);
        }

        for (const Resource& resource : res.push_constant_buffers)
        {
            const SPIRType& type = comp.get_type(resource.base_type_id);
            uint32_t last = uint32_t(type.member_types.size() - 1);
            size_t size = comp.get_declared_struct_size(type);
            size_t offset =comp.type_struct_member_offset(type, 0);
            VkPushConstantRange pushConstantRange;

            pushConstantRange.size = size - offset;
            pushConstantRange.offset = offset;
            pushConstantRange.stageFlags = ShaderTypeToVulkanFlag(ShaderMod.type);

            //offset += size;

            BindInfoPushConstants newinfo;
            newinfo.size = size;

            privData->pushConstantRanges.push_back(pushConstantRange);
            privData->reflectionData.PushConstants.push_back(newinfo);
        }        
    }
    
    return true;
}



VkPipelineLayout ShaderEffect::build_pipeline_layout(VkDevice device)
{    
    if (!privData->bPipelineLayoutsBuilt) {
		std::array< VkDescriptorSetLayout, 4> descriptorSetLayouts = build_descriptor_layouts(device);

		VkPipelineLayoutCreateInfo pipelineLayoutInfo;
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pNext = nullptr;
		pipelineLayoutInfo.setLayoutCount = 4;
		pipelineLayoutInfo.pSetLayouts = &descriptorSetLayouts[0];
		pipelineLayoutInfo.pushConstantRangeCount = privData->pushConstantRanges.size();
		pipelineLayoutInfo.pPushConstantRanges = privData->pushConstantRanges.data();
        pipelineLayoutInfo.flags = 0;
        
		vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &privData->builtPipelineLayout);

        privData->bPipelineLayoutsBuilt = true;
    }
    

    return privData->builtPipelineLayout;
}

void ShaderEffect::set_manual_push_constants(VkPushConstantRange* ranges, int count)
{
    privData->pushConstantRanges.clear();
    for (int i = 0; i < count; i++) {
        privData->pushConstantRanges.push_back(ranges[i]);
    }

}

std::array<VkDescriptorSetLayout, 4> ShaderEffect::build_descriptor_layouts(VkDevice device)
{
    if (!privData->bSetLayoutsBuilt) {
		std::array< VkDescriptorSetLayout, 4> descriptorSetLayouts;

		for (int i = 0; i < 4; i++) {

			VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags{};
			binding_flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
			binding_flags.bindingCount = privData->bindingSets[i].descriptorFlags.size();
			binding_flags.pBindingFlags = privData->bindingSets[i].descriptorFlags.data();

			VkDescriptorSetLayoutCreateInfo layoutInfo;

			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.flags = 0;
            layoutInfo.pNext = &binding_flags;//nullptr;
			layoutInfo.pBindings = privData->bindingSets[i].descriptorBindings.data();
			layoutInfo.bindingCount = privData->bindingSets[i].descriptorBindings.size();

			VkDescriptorSetLayout descriptorSetLayout;

			auto result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);

			if (result == VK_SUCCESS) {
				descriptorSetLayouts[i] = descriptorSetLayout;
			}
		}

		privData->builtSetLayouts = descriptorSetLayouts;
        privData->bSetLayoutsBuilt = true;
    }

    return privData->builtSetLayouts;
}

std::vector<VkPipelineShaderStageCreateInfo> ShaderEffect::get_stage_infos()
{
    return privData->ShaderStages;
}

std::vector<std::string> ShaderEffect::get_loaded_shaders()
{
    return privData->loaded_shaders;
}

BindReflection* ShaderEffect::get_reflection()
{
    return &privData->reflectionData;
}


ShaderEffect::ShaderEffect()
{
    privData = new ShaderEffectPrivateData;
    privData->bPipelineLayoutsBuilt = false;
    privData->bSetLayoutsBuilt = false;
}

ShaderEffect::~ShaderEffect()
{
    for (auto shader : privData->Shaders)
    {
        delete shader;
    }
    for (auto shaderinfo : privData->ShaderStages)
    {
        vkDestroyShaderModule(privData->device, shaderinfo.module, nullptr);
    }
    delete privData;
}
