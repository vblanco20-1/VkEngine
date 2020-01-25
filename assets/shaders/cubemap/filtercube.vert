#version 450

//layout (location = 0) in vec3 inPos;

layout(push_constant) uniform PushConsts {
	layout (offset = 0) mat4 mvp;
	
} pushConsts;

layout (location = 0) out vec3 outUVW;

out gl_PerVertex {
	vec4 gl_Position;
};

vec3 cube(){
	int tri = gl_VertexIndex / 3;
    int idx = gl_VertexIndex % 3;
    int face = tri / 2;
    int top = tri % 2;

    int dir = face % 3;
    int pos = face / 3;

    int nz = dir >> 1;
    int ny = dir & 1;
    int nx = 1 ^ (ny | nz);

    vec3 d = vec3(nx, ny, nz);
    float flip = 1 - 2 * pos;

    vec3 n = flip * d;
    vec3 u = -d.yzx;
    vec3 v = flip * d.zxy;

    float mirror = -1 + 2 * top;
    return  n + mirror*(1-2*(idx&1))*u + mirror*(1-2*(idx>>1))*v;
}

void main() 
{
	vec3 inPos = cube();
	outUVW = inPos;
	gl_Position = pushConsts.mvp * vec4(inPos.xyz, 1.0);
}
