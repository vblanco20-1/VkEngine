

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
        unsigned member_count = type.member_types.size();

        auto struct_name = comp.get_name(resource.base_type_id);

        unsigned set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
        unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);

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
