#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(set=0,binding=0) uniform Scene {mat4 view;mat4 projection;mat4 invView;mat4 invProjection;mat4 lightVP;vec4 light;vec4 eye;vec4 options;vec4 viewport;} s;
layout(push_constant) uniform Object {mat4 model;vec4 color;} o;
layout(location=0) out vec3 viewNormal;
void main(){gl_Position=s.projection*s.view*o.model*vec4(position,1);viewNormal=mat3(s.view)*transpose(inverse(mat3(o.model)))*normal;}
