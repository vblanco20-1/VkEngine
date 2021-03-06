
layout(set = 0, binding = 4) uniform sampler2D blueNoise;
layout(set = 0, binding = 5) uniform sampler2D ssaoMap;
layout(set = 0, binding = 6) uniform samplerCube ambientCubemap;
layout(set = 0, binding = 7) uniform sampler2D shadowMap;
layout(set = 0, binding = 8) uniform samplerCube reflectionCubemap;
layout(set = 0, binding = 9) uniform sampler2D samplerBRDFLUT;
layout(set = 0, binding = 10) uniform sampler2D all_textures[];

layout(set = 2, binding = 6) uniform sampler2D tex1;
layout(set = 2, binding = 7) uniform sampler2D tex2;
layout(set = 2, binding = 8) uniform sampler2D tex3;
layout(set = 2, binding = 9) uniform sampler2D tex4;
layout(set = 2, binding = 10) uniform sampler2D tex5;
layout(set = 2, binding = 11) uniform sampler2D tex6;
layout(set = 2, binding = 12) uniform sampler2D tex7;
layout(set = 2, binding = 13) uniform sampler2D tex8;