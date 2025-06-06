#version 450

precision highp float;
precision highp int;

layout(location = 0) in vec2 texCoord;

layout(std140, binding=1) uniform meshWideBlockPS {
    vec4 blurAmount;
};

layout(set=1,binding=0) uniform sampler2D screenTex;
layout(set=1,binding=1) uniform sampler2D blurTex;

layout (location = 0) out vec4 Out_Color;

void main()
{
    vec4 screen = texture(screenTex, texCoord);
    vec3 blurred = texture(blurTex, texCoord).xyz;
    vec3 mixed = mix(screen.xyz, blurred, vec3(blurAmount.z));
    vec3 glow = ((blurred * blurred) * blurAmount.w);

    Out_Color = vec4(mixed.rgb + glow, screen.a);
//    Out_Color = vec4(glow, screen.a);
}
