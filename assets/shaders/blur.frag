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

layout(binding=0) uniform sampler2D texSource;
layout(binding=1) uniform sampler2D texLinearDepth;

layout (location = 0) in vec2 texCoord;

layout(location=0,index=0) out float out_Color;


//-------------------------------------------------------------------------

vec4 BlurFunction(vec2 uv, float r, vec4 center_c, float center_d, inout float w_total)
{
  vec4  c = texture( texSource, uv );
  float d = texture( texLinearDepth, uv).w / 50000.f;
  
  const float BlurSigma = float(KERNEL_RADIUS) * 0.5;
  const float BlurFalloff = 1.0 / (2.0*BlurSigma*BlurSigma);
  
  float ddiff = (d - center_d) * g_Sharpness * 10;
  float w = exp2(-r*r*BlurFalloff - ddiff*ddiff);
  w_total += w;

  return c*w;
}

void main()
{
  vec4  center_c = texture( texSource, texCoord );
  float center_d = texture( texLinearDepth, texCoord).w / 50000.f;
  
  vec4  c_total = center_c;
  float w_total = 1.0;
  
  for (float r = 1; r <= KERNEL_RADIUS; ++r)
  {
    vec2 uv = texCoord + (g_InvResolutionDirection * r);
    c_total += BlurFunction(uv, r, center_c, center_d, w_total);  
  }

  for (float r = 1; r <= KERNEL_RADIUS; ++r)
  {
     vec2 uv = texCoord + (g_InvResolutionDirection * r * -1);
    c_total += BlurFunction(uv, r, center_c, center_d, w_total);  
   }

  out_Color = (c_total/w_total).r;
}
#endif