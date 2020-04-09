#version 450

layout (set = 0,binding = 0) uniform sampler2D samplerPositionDepth;
layout (set = 0,binding = 1) uniform sampler2D samplerNormal;
layout (set = 0,binding = 2) uniform sampler2D ssaoNoise;

//const int SSAO_KERNEL_SIZE = 16;
const float SSAO_RADIUS = 50.f;
layout (constant_id = 0) const int SSAO_KERNEL_SIZE = 64;
//layout (constant_id = 1) const float SSAO_RADIUS = 0.5;

layout (set = 0,binding = 3) uniform UBOSSAOKernel
{
	vec4 samples[SSAO_KERNEL_SIZE];
} uboSSAOKernel;

layout(set = 0, binding = 4) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
	mat4 inv_model;
    mat4 inv_view;
    mat4 inv_proj;
	vec4 eye;
} ubo;

layout (location = 0) in vec2 inUV;

layout (location = 0) out float outFragColor;
//layout (location = 0) out vec4 outFragColor;

float rand2D(in vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float getSampleDepth(in vec2 uv){
	return texture(samplerPositionDepth, uv).r;
}

void main() 
{
	
	vec3 fpos = vec3(gl_FragCoord.x,gl_FragCoord.y,getSampleDepth(inUV) );
	// Get G-Buffer values
	//vec3 fragPos = texture(samplerPositionDepth, inUV).rgb;
	vec3 fragPos = (ubo.inv_proj * vec4(fpos,1.f)).xyz; //texture(samplerPositionDepth, inUV).rgb;

	vec3 normal = normalize(texture(samplerNormal, inUV).rgb * 2.0 - 1.0);

	// Get a random vector using a noise lookup
	ivec2 texDim = textureSize(samplerPositionDepth, 0); 
	ivec2 noiseDim = textureSize(ssaoNoise, 0);
	const vec2 noiseUV = vec2(float(texDim.x)/float(noiseDim.x), float(texDim.y)/(noiseDim.y)) * inUV;

    vec3 randomVec = texture(ssaoNoise, noiseUV).xyz * 2.0 - 1.0;
	
	// Create TBN matrix
	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(tangent, normal);

	mat3 TBN = mat3(tangent, bitangent, normal);

	// Calculate occlusion value
	float occlusion = 0.0f;
	// remove banding
	const float bias = 5.f;

	for(int i = 0; i < SSAO_KERNEL_SIZE; i++)
	{		
		vec3 samplePos = TBN*uboSSAOKernel.samples[i].xyz; 
		samplePos = fragPos + samplePos * SSAO_RADIUS; 
		
		// project
		vec4 offset = vec4(samplePos, 1.0f);
		offset = ubo.projection * offset; 
		offset.xyz /= offset.w; 
		offset.xyz = offset.xyz * 0.5f + 0.5f; 
		
		float sampleDepth = -getSampleDepth(offset.xy); 

#define RANGE_CHECK 1
#ifdef RANGE_CHECK
		// Range check
		float rangeCheck = smoothstep(0.0f, 1.0f, SSAO_RADIUS / abs(fragPos.z - sampleDepth));
		occlusion += (sampleDepth >= samplePos.z + bias ? 1.0f : 0.0f) * rangeCheck;           
#else
		occlusion += (sampleDepth >= samplePos.z + bias ? 1.0f : 0.0f);  
#endif
	}
	occlusion *= 2;
	occlusion = clamp( 1.0 - (occlusion / float(SSAO_KERNEL_SIZE)) ,0.f,1.f );

	if(abs(fragPos.z) > 2000){
		outFragColor = 1.f;
	}
	else{
		outFragColor =  occlusion;
	}	
}

