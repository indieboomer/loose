#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outputColor;
layout(set=0,binding=0) uniform Scene {mat4 view;mat4 projection;mat4 invView;mat4 invProjection;mat4 lightVP;vec4 light;vec4 eye;vec4 options;vec4 viewport;} s;
layout(set=0,binding=5) uniform sampler2D litTex;
float luma(vec3 c){return dot(c,vec3(.299,.587,.114));}
void main(){
    vec3 center=texture(litTex,uv).rgb;
    if(s.options.z>0){outputColor=vec4(center,1);return;}
    vec2 texel=s.viewport.zw;
    vec3 nw=texture(litTex,uv+vec2(-1,-1)*texel).rgb;
    vec3 ne=texture(litTex,uv+vec2(1,-1)*texel).rgb;
    vec3 sw=texture(litTex,uv+vec2(-1,1)*texel).rgb;
    vec3 se=texture(litTex,uv+vec2(1,1)*texel).rgb;
    float m=luma(center),a=luma(nw),b=luma(ne),c=luma(sw),d=luma(se);
    float lo=min(m,min(min(a,b),min(c,d))),hi=max(m,max(max(a,b),max(c,d)));
    if(hi-lo<max(.022,hi*.12)){outputColor=vec4(center,1);return;}
    vec2 direction=vec2(-(a+b-c-d),a+c-b-d);
    float reduction=max((a+b+c+d)*.03125,.0078125);
    float scale=1/(min(abs(direction.x),abs(direction.y))+reduction);
    direction=clamp(direction*scale,vec2(-6),vec2(6))*texel;
    vec3 pair=.5*(texture(litTex,uv+direction*(-1.0/6)).rgb+texture(litTex,uv+direction*(1.0/6)).rgb);
    vec3 wide=.5*pair+.25*(texture(litTex,uv-direction*.5).rgb+texture(litTex,uv+direction*.5).rgb);
    float value=luma(wide);outputColor=vec4(value<lo||value>hi?pair:wide,1);
}
