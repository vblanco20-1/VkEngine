#version 450

layout (set = 0,binding = 0) uniform sampler2D ssao_post;
layout (set = 0,binding = 1) uniform sampler2D gbuffer_position;


layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;


void main() 
{
	vec4 pout = vec4(1.f);
	pout.x = texture(ssao_post, inUV).r;	
	pout.y = texture(gbuffer_position, inUV).w;	
	outFragColor = pout;	
}

