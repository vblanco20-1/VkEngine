 // forward declaration
layout(buffer_reference) buffer VertexBuffer;

struct Vertex{
    vec4 pos;
	vec4 color;
	vec4 texCoord;
	vec4 normal;
};

        // complete reference type definition
//layout(buffer_reference, std430, buffer_reference_align = 16) buffer VertexBuffer {
layout(buffer_reference, std430) readonly buffer VertexBuffer {
           Vertex vertices[];
};


struct PerObject{
	mat4 model;
    VertexBuffer vertex_buffer;
    VertexBuffer vertex_buffer_pad1;
};
layout(std430,set = 0, binding = 2) readonly buffer Pos 
{
   PerObject objects[ ];
} MainObjectBuffer;


vec4 get_position(int oIdx,int vIdx){

	vec4 vpos = MainObjectBuffer.objects[oIdx].vertex_buffer.vertices[vIdx].pos;
    vpos.w = 1.f;
	return vpos;
}
vec4 get_color(int oIdx,int vIdx){

	vec4 vpos = MainObjectBuffer.objects[oIdx].vertex_buffer.vertices[vIdx].color;
	return vpos;
}
vec3 get_normal(int oIdx,int vIdx){

	vec4 vpos = MainObjectBuffer.objects[oIdx].vertex_buffer.vertices[vIdx].normal;
	return vec3(vpos);
}

vec2 get_uv0(int oIdx,int vIdx){

	vec4 vpos = MainObjectBuffer.objects[oIdx].vertex_buffer.vertices[vIdx].texCoord;
	return vec2(vpos);
}