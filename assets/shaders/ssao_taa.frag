#version 450

layout(push_constant) uniform PushConsts{
	mat4  matA; 
  mat4  matB;
  mat4  matC;
  mat4  matD;
};

layout(binding=0) uniform sampler2D _RG_ssao_mid;
layout(binding=1) uniform sampler2D _RG_gbuf_pos;
layout(binding=2) uniform sampler2D _RG_ssao_accumulate;
layout(binding=3) uniform sampler2D _RG_motion_vectors;


layout (location = 0) in vec2 texCoord;

layout(location=0,index=0) out float out_Color;

vec4 uv_to_clipspace(vec2 uv, float depth){
  float x = (2*uv.x)-1;
  float y = (2*(uv.y))-1;

  //clip space point
  return vec4(x,y, depth,1);
}

void main()
{
  vec2 current_uv = texCoord;

  vec4 pixelMovement = texture(_RG_motion_vectors, current_uv) /2.f;
  vec2 oldPixelUv = current_uv - ((pixelMovement.xy));// * 2.0) - 1.0);


  //inverse viewproj matrix for this frame
  mat4 inv_newview = matB;
  //viewproj matrix of last frame
  mat4 old_viewproj = matA;

  float depth = texture(_RG_gbuf_pos, current_uv).w;

  vec2 cp = current_uv;
  //clip space point
  vec4 p = uv_to_clipspace(cp,depth);

  //transform to world space
  vec4 worldspace = (inv_newview *p); //vec4(texture(_RG_gbuf_pos, current_uv).xyz,1);

  //transform back to clip space with old matrix
  vec4 p2 = (old_viewproj* (worldspace));

  //find UV from clip-space
  vec2 old_uv = (p2.xy / p2.w)* 0.5 + 0.5;
  //old_uv = oldPixelUv;
  //sample new
  float new = texture(_RG_ssao_mid, current_uv).r;

 #if 0
  out_Color = (abs(pixelMovement.x));
 #else
  if(old_uv.x < 0 || old_uv.x > 1 || old_uv.y < 0 || old_uv.y > 1){
    out_Color = (new);
  }
  else
  {
      //old_uv.y = 1- old_uv.y;
  //sample old
  float orig = texture(_RG_ssao_accumulate, old_uv).r;
  float orig_d = texture(_RG_ssao_accumulate, old_uv).g;

 
  float alpha = 0.05;

  vec4 p_old = uv_to_clipspace(old_uv,orig_d);

  vec3 worldspace_old = vec3(matC * p_old);

 
  if(false)//distance(worldspace_old,worldspace) > 0.1)
  {
    alpha = 1.0;
    out_Color =  (alpha * new) + (1-alpha) * orig;;
  }
  else{
    out_Color =  (alpha * new) + (1-alpha) * orig;;
  }
  } 
 #endif
}