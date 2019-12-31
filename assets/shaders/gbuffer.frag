#version 450
#extension GL_ARB_separate_shader_objects : enable
 #extension GL_GOOGLE_include_directive : enable


layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 posDepth;
layout(location = 1) out vec4 normal;

void main() 
{	
	posDepth.xyz = fragPos;
	posDepth.z = 0.f;

	normal.xyz = fragNormal;
	normal.z = 1.f;
}