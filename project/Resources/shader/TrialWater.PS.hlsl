#include "Object3d.hlsli"
ConstantBuffer<Material> mat : register(b0);
ConstantBuffer<DirectionalLight> sun : register(b1);
ConstantBuffer<Camera> camera : register(b2);

// XY is the signed transport signal, not a velocity in metres/second; Z is an artistic phase.
// This pass blends over the real opaque bed; it does not sample the framebuffer or refract it.
float4 main(VertexShaderOutput input) : SV_Target0 {
    float4 sampleData=mat.surfaceFront;
    float depth=saturate(sampleData.w), activity=saturate(length(sampleData.xy));
    float2 p=input.worldPosition.xz;
    float2 dir=activity>.0001 ? sampleData.xy/max(activity,.0001) : float2(1,0);
    float2 side=float2(-dir.y,dir.x);
    float phase=sampleData.z, travel=dot(p,dir)*4.8-phase, across=dot(p,side)*18;
    float a=dot(p,float2(2.2,3.1))+.7*sin(p.y*4.1)+phase;
    float b=dot(p,float2(-4.3,2.5))-.5*sin(p.x*3.3)-phase;
    float wake=across+.8*sin(travel)+.6*sin(travel*2+across*.2);
    float2 slope=.06*float2(2.2,3.1)*cos(a)+.035*float2(-4.3,2.5)*cos(b)
                +activity*(.21*side*cos(wake)+.13*dir*cos(travel));
    float3 n=normalize(float3(-slope.x,1,-slope.y));
    float3 v=normalize(camera.worldPosition-input.worldPosition);
    float3 l=normalize(-sun.direction+float3(.0001,0,0));
    float grazing=pow(1-saturate(dot(n,v)),3);
    float crest=.65*sin(a)+.35*sin(b);
    float streak=smoothstep(.42,.95,cos(wake))*smoothstep(-.35,.8,sin(travel+across*.08))
                *(1-saturate(fwidth(wake)*.28))*sqrt(activity);
    float3 tint=lerp(float3(.10,.64,.55),float3(.025,.22,.27),depth);
    float glint=pow(saturate(dot(n,normalize(l+v))),38);
    float3 col=tint*(.91+.25*crest)+float3(.60,.83,.79)*(glint*.62+streak*.59);
    col=lerp(col,float3(.41,.68,.76),.045+.54*grazing);
    return float4(col,saturate(.38+.55*depth+.10*grazing+.16*streak));
}
