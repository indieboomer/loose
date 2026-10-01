#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outputColor;
layout(set=0,binding=0) uniform Scene {mat4 view;mat4 projection;mat4 invView;mat4 invProjection;mat4 lightVP;vec4 light;vec4 eye;vec4 options;vec4 viewport;} s;
layout(set=0,binding=1) uniform sampler2D albedoTex;
layout(set=0,binding=2) uniform sampler2D normalTex;
layout(set=0,binding=3) uniform sampler2D depthTex;
layout(set=0,binding=4) uniform sampler2D shadowTex;
vec3 positionAt(vec2 p){float d=texture(depthTex,p).r;vec4 h=s.invProjection*vec4(p*2-1,d,1);return h.xyz/h.w;}
float aoAt(vec2 p,int count){
    float d=texture(depthTex,p).r;if(d>=.99999)return 1;
    vec3 pos=positionAt(p),n=normalize(texture(normalTex,p).xyz);
    vec3 seed=abs(n.z)<.85?vec3(0,0,1):vec3(0,1,0);
    vec3 t=normalize(cross(seed,n)),b=cross(n,t);
    float occ=0,weight=0;
    for(int i=0;i<count;i++){
        float f=(float(i)+.5)/float(count),a=float(i)*2.39996323;
        float z=.20+.75*f,r=sqrt(1-z*z);
        vec3 dir=t*(cos(a)*r)+b*(sin(a)*r)+n*z;
        vec3 samplePos=pos+dir*(.12+.43*f);
        vec4 h=s.projection*vec4(samplePos,1);vec2 q=h.xy/h.w*.5+.5;
        if(h.w<=0||any(lessThan(q,vec2(.001)))||any(greaterThan(q,vec2(.999))))continue;
        float sd=texture(depthTex,q).r;if(sd>=.99999)continue;
        vec3 actual=positionAt(q);float w=1-smoothstep(.15,.65,abs(pos.z-actual.z));
        occ+=(actual.z>samplePos.z+.028?1:0)*w;weight+=1;
    }
    return clamp(1-.78*occ/max(weight,1),.3,1);
}
float ambientOcclusion(){
    float center=aoAt(uv,16);vec3 p=positionAt(uv),n=normalize(texture(normalTex,uv).xyz);
    float sum=center*2,weight=2;
    vec2 offsets[4]=vec2[](vec2(2,0),vec2(-2,0),vec2(0,2),vec2(0,-2));
    for(int i=0;i<4;i++){
        vec2 q=uv+offsets[i]*s.viewport.zw;
        if(any(lessThan(q,vec2(0)))||any(greaterThan(q,vec2(1))))continue;
        vec3 nn=normalize(texture(normalTex,q).xyz);float dz=abs(positionAt(q).z-p.z);
        float w=pow(max(dot(n,nn),0),16)*exp(-dz*30);
        sum+=aoAt(q,8)*w;weight+=w;
    }
    return sum/weight;
}
float shadow(vec3 world,float ndl){
    if(s.options.x<.5)return 1;
    vec4 h=s.lightVP*vec4(world,1);vec3 p=h.xyz/h.w;p.xy=p.xy*.5+.5;
    if(h.w<=0||p.z<0||p.z>1||any(lessThan(p.xy,vec2(0)))||any(greaterThan(p.xy,vec2(1))))return 1;
    float visibility=0;float bias=max(.00018,.00065*(1-ndl));
    vec2 step=1.0/vec2(textureSize(shadowTex,0));
    for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)visibility+=(p.z-bias<=texture(shadowTex,p.xy+vec2(x,y)*step).r?1:0);
    return visibility/9;
}
void main(){
    float d=texture(depthTex,uv).r;if(d>=.99999){outputColor=vec4(.008,.009,.012,1);return;}
    vec3 p=positionAt(uv),n=normalize(texture(normalTex,uv).xyz);
    vec3 world=(s.invView*vec4(p,1)).xyz,wn=normalize(mat3(s.invView)*n);
    float ao=(s.options.y>.5||s.options.z==1)?ambientOcclusion():1;
    if(s.options.z==1){outputColor=vec4(vec3(ao),1);return;}
    if(s.options.z==2){outputColor=vec4(vec3(1-exp(-length(p)*.12)),1);return;}
    if(s.options.z==3){outputColor=vec4(n*.5+.5,1);return;}
    vec3 lightVector=s.light.xyz-world;float dist=length(lightVector);vec3 l=lightVector/dist;
    float ndl=max(dot(wn,l),0),cone=smoothstep(.34,.55,dot(-l,vec3(0,-1,0)));
    float direct=3.5*ndl*cone/(1+.085*dist*dist)*shadow(world,ndl);
    vec3 albedo=texture(albedoTex,uv).rgb;
    vec3 viewDir=normalize(s.eye.xyz-world),halfDir=normalize(l+viewDir);
    float spec=.04*pow(max(dot(wn,halfDir),0),24)*ndl*cone*shadow(world,ndl);
    vec3 color=albedo*(vec3(.36,.39,.43)*ao+vec3(1,.94,.84)*direct)+spec;
    color*=s.options.w;
    color=color/(1+color*.55); // Restrained fixed filmic shoulder; swapchain performs sRGB conversion.
    outputColor=vec4(color,1);
}
