#version 450
#extension GL_ARB_separate_shader_objects : enable
 #extension GL_GOOGLE_include_directive : enable


layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 posDepth;
layout(location = 1) out vec4 normal;

const float NEAR_PLANE = 5.f; //todo: specialization const
const float FAR_PLANE =  5000.0f; //todo: specialization const 

float linearDepth(float depth)
{
	float z = depth * 2.0f - 1.0f; 
	//float DistPlanes = FAR_PLANE - NEAR_PLANE;
	//return NEAR_PLANE + DistPlanes * depth;
	return (2.0f * NEAR_PLANE * FAR_PLANE) / (FAR_PLANE + NEAR_PLANE - z * (FAR_PLANE - NEAR_PLANE));
}

void main() 
{	
	
	posDepth.xyz = fragPos;
	posDepth.w = linearDepth(gl_FragCoord.z);

	normal =vec4(normalize(fragNormal) * 0.5 + 0.5, 1.0);
	
}