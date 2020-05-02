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



layout(push_constant) uniform PushConsts {
	int object_id;
};

#if 0
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
#endif
void main() {

    vec4 inPosition = get_position(object_id,gl_VertexIndex);

	mat4 objectMatrix = MainObjectBuffer.objects[object_id].model;

	gl_Position = ubo.proj * ubo.view * objectMatrix * inPosition;  

}