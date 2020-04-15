#version 450

#if 0
layout (binding = 0) uniform sampler2D samplerSSAO;

layout (location = 0) in vec2 inUV;

layout (location = 0) out float outFragColor;

void main() 
{
	const int blurRange = 1;
	int n = 0;
	vec2 texelSize = 1.0 / vec2(textureSize(samplerSSAO, 0));
	float result = 0.0;
	for (int x = -blurRange; x < blurRange; x++) 
	{
		for (int y = -blurRange; y < blurRange; y++) 
		{
			vec2 offset = vec2(float(x), float(y)) * texelSize;
			result += texture(samplerSSAO, inUV + offset).r;
			n++;
		}
	}
	outFragColor = pow(result / (float(n)),2);
}


#else
//const float KERNEL_RADIUS = 10;
  
//const float g_Sharpness = 100.f;
//layout(location=0) uniform float g_Sharpness;
layout(push_constant) uniform PushConsts{
	vec2  g_InvResolutionDirection; // either set x to 1/width or y to 1/height
	float g_Sharpness;
	float KERNEL_RADIUS;
};

const float KERNEL_RAD = 5;

layout(binding=0) uniform sampler2D texSource;
layout(binding=1) uniform sampler2D texLinearDepth;
layout(binding=2) uniform sampler2D texNormals;

layout (location = 0) in vec2 texCoord;

layout(location=0,index=0) out float out_Color;


//-------------------------------------------------------------------------

vec4 BlurFunction(vec2 uv, float r, vec4 center_c, float center_d, inout float w_total)
{
  vec4  c = texture( texSource, uv );
  float d = texture( texLinearDepth, uv).w;// / 50000.f;
  
  const float BlurSigma = ceil(float(KERNEL_RADIUS)) * 0.5;
  const float BlurFalloff = 1.0 / (2.0*BlurSigma*BlurSigma);
  
  float ddiff = (d - center_d) * g_Sharpness * 10000;
  float w = exp2(-r*r*BlurFalloff - ddiff*ddiff);
  w_total += w;

  return c*w;
}

float BlurEdge(vec2 uv, float r, vec4 center_c, float center_d, inout float w_total)
{
  vec4  c = texture( texSource, uv );
  float d = texture( texLinearDepth, uv).w;// / 50000.f;
  
  const float BlurSigma = ceil(float(KERNEL_RADIUS)) * 0.5;
  const float BlurFalloff = 1.0 / (2.0*BlurSigma*BlurSigma);
  
  float ddiff = (d - center_d) * g_Sharpness * 10000; 

  return ddiff;
}

float Difference(float center_d, vec3 center_normal, float p_d, vec3 p_normal){
  
  float normal_factor = pow(max(dot(center_normal, p_normal), 0.0f), 4.0f);
  float depth_factor = abs(center_d - p_d) * g_Sharpness * 10;

 // float weight = pow(max(dot(center_normal, adjacent_normal), 0.0f), 10.0f);
  return  depth_factor + ((normal_factor*1));
}

vec3 GetNormals(vec2 uv){
  return normalize(texture(texNormals, uv).rgb * 2.0 - 1.0);
}

const float NEAR_PLANE = 0.1f; //todo: specialization const
const float FAR_PLANE =  50000.0f; //todo: specialization const 
float linearize_depth(float d)
{
    return NEAR_PLANE * FAR_PLANE / (FAR_PLANE + d * (NEAR_PLANE - FAR_PLANE));
}
float GetDepth(vec2 uv)
{
  return linearize_depth(1-texture( texLinearDepth, uv).w) / 50000.f;
}

float NewBlurFunction(vec2 uv, float r, float center_c,vec3 center_n, float center_d, inout float w_total)
{
  float  c = texture( texSource, uv ).r;
  float d = GetDepth(uv);//texture( texLinearDepth, uv).w;// / 50000.f;
  
  const float BlurSigma = ceil(float(KERNEL_RADIUS)) * 0.5;
  const float BlurFalloff = 1.0 / (2.0*BlurSigma*BlurSigma);
  
  vec3 normal = GetNormals(uv);//vec3(1);
  float ddiff = Difference(center_d,center_n,d,normal);//(d - center_d) * g_Sharpness * 10000;
  float w = exp2(-r*r*BlurFalloff - ddiff*ddiff);
  w_total += w;

  return c*w;
}


void main()
{
  vec4  center_c = texture( texSource, texCoord );
  vec3 center_n = GetNormals(texCoord);
  float center_d =  GetDepth(texCoord);//texture( texLinearDepth, texCoord).w;// / 50000.f;
  float edge=0.f;
  float  c_total = center_c.r;
  float w_total = 1.0;
  
  float rad = ceil(KERNEL_RADIUS);

#if 0
  for (float r = 1; r <= rad; ++r)
  {
    vec2 uv = texCoord + (g_InvResolutionDirection * r);
    c_total += BlurFunction(uv, r, center_c, center_d, w_total);  
    edge += BlurEdge(uv, r, center_c, center_d, w_total);  
  }

  for (float r = 1; r <= rad; ++r)
  {
     vec2 uv = texCoord + (g_InvResolutionDirection * r * -1);
    c_total += BlurFunction(uv, r, center_c, center_d, w_total);  
    edge += BlurEdge(uv, r, center_c, center_d, w_total);  
   }
#endif


  float blurRange = floor(KERNEL_RADIUS);
  if(blurRange != 0){
for (float x = -blurRange; x < blurRange; x++) 
	{
		for (float y = -blurRange; y < blurRange; y++) 
		{
      float r = sqrt(x*x+y*y);
			vec2 uv = texCoord + (g_InvResolutionDirection * vec2(x,y));

			c_total += NewBlurFunction(uv, r, center_c.r,center_n, center_d, w_total);
		}
	}
  }
  

  out_Color = clamp((c_total/w_total),0.f,1.f);
}
#endif