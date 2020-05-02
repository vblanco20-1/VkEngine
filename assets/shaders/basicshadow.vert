#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference : enable


#include "object_buffer.inl"


layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
	mat4 inv_model;
    mat4 inv_view;
    mat4 inv_proj;
	vec4 eye;
} ubo;

layout(std430,set = 0, binding = 3) readonly buffer Instances 
{
   int indices[ ];
} InstanceIDBuffer;

void main() {

    int object_idx = (InstanceIDBuffer.indices[gl_InstanceIndex]);

    vec4 inPosition = get_position(object_idx,gl_VertexIndex);

	mat4 objectMatrix = MainObjectBuffer.objects[object_idx].model;

	gl_Position = ubo.proj * ubo.view * objectMatrix * inPosition;
}