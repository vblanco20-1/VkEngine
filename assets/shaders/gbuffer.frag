#version 450
#extension GL_ARB_separate_shader_objects : enable
 #extension GL_GOOGLE_include_directive : enable


layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragPos;
layout(location = 2) in vec4 clipPos;
layout(location = 3) in vec4 lastclipPos;

layout(location = 0) out vec4 posDepth;
layout(location = 1) out vec4 normal;
layout(location = 2) out vec4 motionVectors;

const float NEAR_PLANE = 0.1f; //todo: specialization const
const float FAR_PLANE =  50000.0f; //todo: specialization const 

float linearize_depth(float d)
{
    return NEAR_PLANE * FAR_PLANE / (FAR_PLANE + d * (NEAR_PLANE - FAR_PLANE));
}
float linearDepth(float depth)
{

	
	float z = (depth * 2.0f - 1.0f); 
	
	return (2.0f * NEAR_PLANE * FAR_PLANE) / (FAR_PLANE + NEAR_PLANE - z * (FAR_PLANE - NEAR_PLANE));

	//float DistPlanes = FAR_PLANE - NEAR_PLANE;
	//return NEAR_PLANE + DistPlanes * depth;
}

void main() 
{	
	
	posDepth.xyz = fragPos;
	//posDepth.w = linearDepth(gl_FragCoord.z);//gl_FragCoord.z;
	//posDepth.w = linearize_depth(1-gl_FragCoord.z);
	posDepth.w = gl_FragCoord.z;
	
	
	float colorScale =1.f;
    // Make sure to account for perspective division
    vec3 motion = ((clipPos / clipPos.w) - (lastclipPos / lastclipPos.w)).xyz;
    // Adjust delta clip space to color space
   // vec3 motionColor = ((colorScale * motion) + 2.0) / 4.0;
    //motionVectors = vec4(motionColor, 1.0);
	motionVectors = vec4(motion, 1.0);
	//linearDepth(1-gl_FragCoord.z);
	//posDepth.x = linearDepth(1-gl_FragCoord.z);

	normal =vec4(normalize(fragNormal) * 0.5 + 0.5, 1.0);	
}