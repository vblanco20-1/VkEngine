 // forward declaration
layout(buffer_reference) buffer VertexBuffer;

#if 0
struct Vertex{
    vec4 pos;
	vec4 color;
	vec4 texCoord;
	vec4 normal;
};
#else
struct Vertex{
    vec4 pos;
	vec4 normal;
};
#endif



layout(buffer_reference, std430) readonly buffer VertexBuffer {
           Vertex vertices[];
};

struct PerObject{
	mat4 model;
	ivec4 tex1;
	ivec4 tex2;
    VertexBuffer vertex_buffer;
    VertexBuffer vertex_buffer_pad;
};
layout(std430,set = 0, binding = 2) readonly buffer Pos 
{
   PerObject objects[ ];
} MainObjectBuffer;

int get_texture(int textureIndex, int oIdx)
{
	if(textureIndex < 4)
	{
		return MainObjectBuffer.objects[oIdx].tex1[textureIndex];
	}
	else{
		return MainObjectBuffer.objects[oIdx].tex2[textureIndex-4];
	}
}

vec4 get_position(int oIdx,int vIdx){

	vec4 vpos = MainObjectBuffer.objects[oIdx].vertex_buffer.vertices[vIdx].pos;
    vpos.w = 1.f;
	return vpos;
}
vec4 get_color(int oIdx,int vIdx){
#if 0
	vec4 vpos = MainObjectBuffer.objects[oIdx].vertex_buffer.vertices[vIdx].color;
#else
	vec4 vpos = vec4(1);
#endif
	return vpos;
}
vec3 get_normal(int oIdx,int vIdx){

	vec4 vpos = MainObjectBuffer.objects[oIdx].vertex_buffer.vertices[vIdx].normal;
	return vec3(vpos);
}

vec2 get_uv0(int oIdx,int vIdx){
#if 0
	vec4 vpos = MainObjectBuffer.objects[oIdx].vertex_buffer.vertices[vIdx].texCoord;
	return vec2(vpos);
#else
	float uvx = MainObjectBuffer.objects[oIdx].vertex_buffer.vertices[vIdx].pos.w;
	float uvy = MainObjectBuffer.objects[oIdx].vertex_buffer.vertices[vIdx].normal.w;

	return vec2(uvx,uvy);
#endif	
}