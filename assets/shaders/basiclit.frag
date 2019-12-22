#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(set = 0,binding = 6) uniform sampler2D tex1;
layout(set = 0,binding = 7) uniform sampler2D tex2;
layout(set = 0,binding = 8) uniform sampler2D tex3;
layout(set = 0,binding = 9) uniform sampler2D tex4;
layout(set = 0,binding = 10) uniform sampler2D tex5;
layout(set = 0,binding = 11) uniform sampler2D tex6;
layout(set = 0,binding = 12) uniform sampler2D tex7;
layout(set = 0,binding = 13) uniform sampler2D tex8;

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


	vec4 color = texture(tex1, fragTexCoord);
	float metal =  texture(tex3, fragTexCoord).b;
	float rough =  texture(tex3, fragTexCoord).g;

	
	float diffuse = clamp(dot(fragNormal,light_dir),0.1f,1.0f);
	float spec = pow(max(dot(fragNormal, halfwayDir), 0.0),  mix(8,1.f, rough));

	float fog_dist = clamp( length (fragPos - eyePos)/5000 ,0, 1.f);

	vec4 dialectric = (color)*diffuse + vec4(0.1f) * spec;
	vec4 metallic =  (color)*(spec*diffuse);

    outColor = mix(dialectric,metallic, metal);
}