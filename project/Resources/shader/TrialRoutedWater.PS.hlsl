#include "Object3d.hlsli"
// Alternate view of the existing static material CB. No extra upload or SRV per water part.
cbuffer RoutedWater : register(b0) {
    float4 ports : packoffset(c0);       // Signed outward transfers in W,E,-Z,+Z order (U/s).
    float clockS : packoffset(c1.y);
    float4 domain : packoffset(c8);      // Center XZ, inverse pitch (zero for pond), stock fraction.
};
ConstantBuffer<DirectionalLight> sun : register(b1);
ConstantBuffer<Camera> camera : register(b2);
static const float2 face[4]={float2(-1,0),float2(1,0),float2(0,-1),float2(0,1)};
static const float tau=6.2831853;
float2 leftOf(float2 d) {return float2(-d.y,d.x);}

// The longitudinal phase is periodic at *both* cell faces. Transverse sign stays
// left-of-travel through the bend, so a strand entering a turn does not mirror at its exit.
float lane(float2 p,float2 entry,float2 exit,float qi,float qo,out float2 tangent) {
    float u,v;
    if(dot(entry,exit)==0) {
        float2 center=(entry+exit)*.5, ray=p-center, start=-exit;
        float spin=sign(dot(-entry,leftOf(start)));
        float angle=atan2(spin*dot(ray,leftOf(start)),dot(ray,start));
        u=angle/1.57079633;
        float r=max(length(ray),.001);
        v=spin*(.5-r);
        tangent=spin*leftOf(ray)/r;
    } else {
        tangent=exit; u=.5+dot(p,exit); v=dot(p,leftOf(exit));
    }
    float age=u*tau-clockS*tau*.75;
    float amplitude=saturate(lerp(qi,qo,saturate(u))/.45);
    float warp=.018*sin(age*2)+.010*sin(age*3+1.3);
    float track=v-warp;
    float mask=exp2(-75*track*track)*smoothstep(-.07,.01,u)*smoothstep(-.07,.01,1-u);
    float strands=cos(track*76+1.2*sin(age))+ .36*cos(track*122-age);
    float patches=.42+.58*smoothstep(-.65,.75,cos(age*2+track*6));
    return mask*smoothstep(.10,1.16,strands)*patches*amplitude;
}

float4 main(VertexShaderOutput input) : SV_Target0 {
    // Box side walls between adjacent translucent tiles would blend a second time.
    // The waterfall's visible thickness is rendered separately.
    clip(input.normal.y-.05);
    float2 world=input.worldPosition.xz;
    bool pond=domain.z==0;
    float depth=saturate(domain.w);
    float2 p=(world-domain.xy)*domain.z,acc=0;
    float strands=0;
#ifndef MEASURE_WITHOUT_LANES
    if(!pond) {
        float4 intake=max(-ports,0), outflow=max(ports,0);
        float inSum=dot(intake,1),outSum=dot(outflow,1);
        // Multiple inlets share each outlet in proportion to measured volume.
#ifdef ACTIVE_FACES_ONLY
        uint ingoing=dot(float4(intake>0),float4(1,2,4,8));
        uint outgoing=dot(float4(outflow>0),float4(1,2,4,8));
        for(uint ins=ingoing;ins;ins&=ins-1) {uint i=firstbitlow(ins);
#else
        [loop] for(int i=0;i<4;++i) if(intake[i]>0) {
#endif
            if(outSum>0) {
#ifdef ACTIVE_FACES_ONLY
                for(uint outs=outgoing;outs;outs&=outs-1) {uint j=firstbitlow(outs);
#else
                [loop] for(int j=0;j<4;++j) if(outflow[j]>0) {
#endif
                    float2 tangent;
                    float s=lane(p,face[i],face[j],intake[i]*outflow[j]/outSum,outflow[j]*intake[i]/inSum,tangent);
                    acc+=tangent*s; strands+=s;
                }
            } else {
                float2 tangent;float s=lane(p,face[i],-face[i],intake[i],intake[i],tangent)
                    *smoothstep(-.11,.14,dot(p,face[i]));
                acc+=tangent*s;strands+=s;
            }
        }
        if(inSum==0) [loop] for(int j=0;j<4;++j) if(outflow[j]>0) {
            float2 tangent;float s=lane(p,-face[j],face[j],outflow[j],outflow[j],tangent)
                *smoothstep(-.11,.14,dot(p,face[j]));
            acc+=tangent*s;strands+=s;
        }
    }
#endif
    float t=tau*clockS/120;
    float swell=dot(world,float2(1.32,.78))+.82*sin(dot(world,float2(.71,-.57))+t*3)-t*9;
    float crosswave=dot(world,float2(-2.24,3.05))+.69*sin(.71*swell)+t*14;
    float chop=dot(world,float2(5.73,2.39))+.53*sin(crosswave)-t*23;
    float wind=pond?1:.33;
    float2 slope=wind*(float2(.072,.039)*cos(swell)+float2(-.045,.073)*cos(crosswave)
                 +float2(.049,.019)*cos(chop))+acc*.14;
    float3 n=normalize(float3(-slope.x,1,-slope.y));
    float3 view=normalize(camera.worldPosition-input.worldPosition);
    float3 light=normalize(-sun.direction+float3(.0001,0,0));
    float glint=pow(saturate(dot(n,normalize(light+view))),54);
    float fr=pow(1-saturate(dot(n,view)),3);
    float3 col=lerp(float3(.065,.51,.42),float3(.018,.17,.22),depth);
    col*=.98+wind*(.07*sin(swell)+.055*sin(crosswave));
    float lace=smoothstep(.70,.96,.53*sin(crosswave)+.47*sin(chop));
    col+=float3(.43,.70,.65)*(glint*.81+wind*lace*.095+saturate(strands)*.65);
    col=lerp(col,float3(.40,.64,.68),.04+.5*fr);
    return float4(col,saturate(.34+.49*depth+.12*fr+.20*strands));
}
