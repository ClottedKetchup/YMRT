#version 450 core

const vec2 position[4] = vec2[4]( vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0) );
const vec2 texcoord[4] = vec2[4]( vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0) );

layout(location = 0) out vec2 tc;

void main()
{
	gl_Position = vec4(position[gl_VertexID], 0.0, 1.0);
	tc = texcoord[gl_VertexID];
}