#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference : enable

#include "object_buffer.inl"

struct PointLight{
	vec4 pos_r; //xyz pos, w radius
	vec4 col_power; //xyz col, w power
};

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
	mat4 inv_model;
    mat4 inv_view;
    mat4 inv_proj;
	vec4 eye;
} ubo;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragPos;
layout(location = 2) out vec4 clipPos;
layout(location = 3) out vec4 lastclipPos;

void main() {


	vec4 inPosition = get_position(gl_InstanceIndex,gl_VertexIndex);	
	vec3 inNormal= get_normal(gl_InstanceIndex,gl_VertexIndex);

	mat4 objectMatrix = MainObjectBuffer.objects[gl_InstanceIndex].model;
	vec4 pointPos  = ubo.proj * ubo.view * objectMatrix * inPosition;

	gl_Position = pointPos;

	clipPos = pointPos;
	lastclipPos = ubo.inv_model * objectMatrix * inPosition;
   
#if 1 // WORLD SPACE
	fragNormal = normalize(objectMatrix * vec4(inNormal, 0.0)).xyz;
	fragPos = (objectMatrix * inPosition).xyz;
#else
	fragPos = pointPos.xyz;//vec3(ubo.view * objectMatrix * vec4(inPosition, 1.0));
	fragNormal = vec3(ubo.view * objectMatrix * vec4(inNormal, 0.0));
#endif
}