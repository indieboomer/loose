#version 450
layout(location=0) in vec3 viewNormal;
layout(location=0) out vec4 albedo;
layout(location=1) out vec4 normal;
layout(push_constant) uniform Object {mat4 model;vec4 color;} o;
void main(){albedo=o.color;normal=vec4(normalize(viewNormal),1);}
