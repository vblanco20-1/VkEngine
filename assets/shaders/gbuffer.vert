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
	mat4 inv_model;
    mat4 inv_view;
    mat4 inv_proj;
	vec4 eye;
} ubo;

layout(std140,set = 0, binding = 2) readonly buffer Pos 
{
   PerObject objects[ ];
} MainObjectBuffer;


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
layout(location = 4) out vec4 clipPos;
layout(location = 5) out vec4 lastclipPos;

void main() {

	mat4 objectMatrix = MainObjectBuffer.objects[object_id].model;
	vec4 pointPos  = ubo.proj * ubo.view * objectMatrix * vec4(inPosition, 1.0);

	gl_Position =pointPos;

	clipPos = pointPos;
	lastclipPos = ubo.inv_model * objectMatrix * vec4(inPosition, 1.0);

    fragColor = inColor;
    fragTexCoord = inTexCoord;
#if 1 // WORLD SPACE
	fragNormal = normalize(objectMatrix * vec4(inNormal, 0.0)).xyz;
	fragPos = (objectMatrix * vec4(inPosition, 1.0)).xyz;
#else
	fragPos = pointPos.xyz;//vec3(ubo.view * objectMatrix * vec4(inPosition, 1.0));
	fragNormal = vec3(ubo.view * objectMatrix * vec4(inNormal, 0.0));
#endif
}