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

layout(set = 1, binding = 1) uniform UniformBufferObject2 {
    mat4 model;
    mat4 view;
    mat4 proj;
	vec4 eye;
} shadowUbo;

layout(set = 1, binding = 0) uniform SceneParameters {
	vec4 fog_a; //xyz color, w power
    vec4 fog_b; //x min, y far, zw unused
    vec4 ambient;//xyz color, w power
	vec4 viewport;
} sceneParams;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;
layout(location = 4) in vec3 eyePos;
layout(location = 5) in vec4 inShadowCoord;

layout(location = 0) out vec4 outColor;
#define ambientshadow 0.1
float shadowSample(){
   return texture( shadowMap, (inShadowCoord / inShadowCoord.w).st).r;
}

float textureProj(vec4 shadowCoord, vec2 off)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
	{
		float dist = texture( shadowMap, shadowCoord.st + off ).r;
		if ( shadowCoord.w > 0.0 && dist < shadowCoord.z ) 
		{
			shadow = ambientshadow;
		}
	}
	return shadow;
}
vec3 screenBluenoise()
{
	// Get a random vector using a noise lookup
	ivec2 noiseDim = textureSize(blueNoise, 0);
	const vec2 noiseUV = gl_FragCoord.xy / noiseDim;  

    vec3 randomVec = texture(blueNoise, noiseUV).xyz * 2.0 - 1.0;
	return randomVec;
}

float filterPCF(vec4 sc)
{
	ivec2 texDim = textureSize(shadowMap, 0);
	float scale = 0.5;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 3;

    vec3 rand = (screenBluenoise());
	
	sc.x += dx * rand.z;
    sc.y += dx * rand.y;
	vec2 dirA = rand.xy;
	vec2 dirB = vec2(-dirA.y,dirA.x);
	
	dirA *= dx;
	dirB *= dy;
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y));
			//shadowFactor += textureProj(sc, dirA*x + dirB*y);
			count++;
		}	
	}
	//return shadowFactor += textureProj(sc, vec2(0));
	return shadowFactor / count;
}

const float PI = 3.14159265359;
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}



vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
} 

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}
vec3 convert_cubemap_coords(vec3 N){

	float swap = N.y;
	N.y = N.z * -1.f;
	N.z = swap;	
	return N;
}
vec3 prefilteredReflection(vec3 R, float roughness)
{
	vec3 dir = convert_cubemap_coords(R);
	const float MAX_REFLECTION_LOD = 9.0; // todo: param/const
	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	vec3 a = textureLod(reflectionCubemap, dir, lodf).rgb;
	vec3 b = textureLod(reflectionCubemap, dir, lodc).rgb;
	return mix(a, b, lod - lodf);
}


vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}



void main() {

	vec2 screenspace = vec2(0.f);
	screenspace.y = gl_FragCoord.y / sceneParams.viewport.w;//900.f;
	screenspace.x = gl_FragCoord.x / sceneParams.viewport.z;//1700.f;

	float tx_ssao = texture(ssaoMap,screenspace).r;
	float ssao = mix(0.0,1,tx_ssao); //clamp( pow(texture(ssaoMap,screenspace).r , 2.f),0.3f,1.f);
	
	vec3 light_dir = normalize(vec3(100.0f, 600.0f, 800.0f)) * 1.f;

    float ao = 1.f;
	vec3 albedo = clamp(texture(tex1, fragTexCoord).rgb,vec3(0.01),vec3(1.f));

	float metallic = 1;
	float roughness = 0.9;
	
	vec3 tex3 = texture(tex3, fragTexCoord).xyz;
	//if(tex3 != vec3(1)){
 	 metallic = tex3.b;
	 roughness =  tex3.g;
	//}

	vec4 emmisive = texture(tex4, fragTexCoord);

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(eyePos - fragPos);
    vec3 R = reflect(-V, N); 


	vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);


	// reflectance equation
    vec3 Lo = vec3(0.0);

	float shadow = mix(0.f,1.f , filterPCF(inShadowCoord / inShadowCoord.w));

	vec3 norm = N;
	//norm.x = N.y;
	//norm.y = N.x;
	//norm.z = -N.z;


	//light
	vec3 L = light_dir;
    vec3 H = normalize(V + L);
	{
		float distance    = length(light_dir);
		float attenuation = shadow;// / (distance * distance);
		vec3 lightradiance     = vec3(1.f,.7f,.7f) * attenuation * 4.f;        
		
		// cook-torrance brdf
		float NDF = DistributionGGX(N, H, roughness);        
		float G   = GeometrySmith(N, V, L, roughness);      
		vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       
		
		vec3 kS = F;
		vec3 kD = vec3(1.0) - kS;
		kD *= 1.0 - metallic;	  
		
		vec3 numerator    = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
		vec3 specular     = numerator / max(denominator, 0.001);  
		    
		// add to outgoing radiance Lo
		float NdotL = max(dot(N, L), 0.0);                
		Lo += (kD * albedo / PI + specular) * lightradiance * NdotL; 
	}

   

    vec2 brdf = texture(samplerBRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
	vec3 reflection = prefilteredReflection(R, roughness).rgb;	
	vec3 irradiance = texture(ambientCubemap,convert_cubemap_coords(N)).rgb * 4;

	// Diffuse based on irradiance
	vec3 diffuse = irradiance * albedo;	

	vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);

	// Specular reflectance
	vec3 specular = reflection * (F * brdf.x + brdf.y);

	// Ambient part
	vec3 kD = 1.0 - F;
	kD *= 1.0 - metallic;	  
	vec3 ambient = (kD * diffuse + specular) * pow(ssao,4);
	
	vec3 color = ambient+ Lo;//vec3(pow(ssao,4));//
	
	// Tone mapping
	color = Uncharted2Tonemap(color * sceneParams.ambient.w);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));	
	// Gamma correction
	color = pow(color, vec3(1.0f / 1));

	//vec4 tex_test  = vec4(texture(tex3, fragTexCoord));//.rgba;
	//float tex_test  = texture(tex3, fragTexCoord);
	//outColor = tex_test;
	outColor = vec4(ssao);

	//outColor = vec4(color, 1.0);
	
	
	//outColor=	vec4(prefilteredReflection(N, sceneParams.ambient.w).rgb, 1.0);
}