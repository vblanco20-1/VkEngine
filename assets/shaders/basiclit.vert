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

layout(set = 1, binding = 1) uniform UniformBufferObject2 {
    mat4 model;
    mat4 view;
    mat4 proj;
	mat4 inv_model;
    mat4 inv_view;
    mat4 inv_proj;
	vec4 eye;
} shadowUbo;


layout(std140,set = 1, binding = 3) readonly buffer Lights 
{
   PointLight lights[ ];
} MainLights;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragPos;
layout(location = 4) out vec3 eyePos;
layout(location = 5) out vec4 ShadowCoord;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );


void main() {

	vec4 inPosition = get_position(gl_InstanceIndex,gl_VertexIndex);
	vec4 inColor =  get_color(gl_InstanceIndex,gl_VertexIndex);
	vec2 inTexCoord= get_uv0(gl_InstanceIndex,gl_VertexIndex);
	vec3 inNormal= get_normal(gl_InstanceIndex,gl_VertexIndex);
	
	eyePos = (ubo.eye).xyz;

	mat4 objectMatrix = MainObjectBuffer.objects[gl_InstanceIndex].model;

	gl_Position = ubo.proj * ubo.view * objectMatrix * inPosition;
    fragColor = vec3(inColor);
	vec2 texcoord = inTexCoord;
	texcoord.y = 1-texcoord.y;
    fragTexCoord = texcoord;
	fragNormal = normalize(objectMatrix * vec4(inNormal, 0.0)).xyz;
	fragPos = (objectMatrix * inPosition).xyz;

	ShadowCoord = biasMat *shadowUbo.proj * shadowUbo.view * objectMatrix * inPosition;
}