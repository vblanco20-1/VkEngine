#version 450
#extension GL_ARB_separate_shader_objects : enable
 #extension GL_GOOGLE_include_directive : enable

#include "texture_samplers.inl"

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
	vec4 eye;
} ubo;

layout(set = 1, binding = 0) uniform SceneParameters {
	vec4 fog_a; //xyz color, w power
    vec4 fog_b; //x min, y far, zw unused
    vec4 ambient;//xyz color, w power
} sceneParams;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;
layout(location = 4) in vec3 eyePos;


layout(location = 0) out vec4 outColor;

void main() {
	vec3 light_dir = normalize(vec3(1.0f,-1.0f,1.0f));


	vec3 viewDir = normalize(eyePos - fragPos);
	vec3 halfwayDir = normalize(light_dir + viewDir);

	vec4 emmisive = texture(tex4, fragTexCoord);

	vec4 color = texture(tex1, fragTexCoord);
	float metal =  texture(tex3, fragTexCoord).b;
	float rough =  texture(tex3, fragTexCoord).g;

	
	vec4 cubemap = texture(ambientCubemap, reflect(-viewDir,fragNormal), 0);


	float diffuse = clamp(dot(fragNormal,light_dir),0.1f,1.0f);
	float spec = pow(max(dot(fragNormal, halfwayDir), 0.0),  mix(8,1.f, rough));

	float fog_dist = clamp( length (fragPos - eyePos)/5000 ,0, 1.f);

	vec4 dialectric = (color)*diffuse + vec4(0.1f) * spec;
	vec4 metallic =  (color)*(spec*diffuse);

	vec4 final_color = emmisive+  mix(dialectric,metallic, metal) + vec4(sceneParams.ambient.xyz,0) * sceneParams.ambient.w;

    outColor = mix(final_color,cubemap * color,metal * 0.3);
}