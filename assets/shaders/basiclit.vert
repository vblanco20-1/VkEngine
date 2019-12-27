#version 450
#extension GL_ARB_separate_shader_objects : enable

struct PerObject{
	mat4 model;
};

struct PointLight{
	vec4 pos_r; //xyz pos, w radius
	vec4 col_power; //xyz col, w power
};

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

layout(std140,set = 0, binding = 2) readonly buffer Pos 
{
   PerObject objects[ ];
} MainObjectBuffer;



layout(std140,set = 1, binding = 3) readonly buffer Lights 
{
   PointLight lights[ ];
} MainLights;


layout(push_constant) uniform PushConsts {
	int object_id;
};


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

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

	
	eyePos = (ubo.eye).xyz;

	mat4 objectMatrix = MainObjectBuffer.objects[object_id].model;

	gl_Position = ubo.proj * ubo.view * objectMatrix * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
	fragNormal = normalize(objectMatrix * vec4(inNormal, 0.0)).xyz;
	fragPos = (objectMatrix * vec4(inPosition, 1.0)).xyz;

	ShadowCoord = biasMat *shadowUbo.proj * shadowUbo.view * objectMatrix * vec4(inPosition, 1.0);

}