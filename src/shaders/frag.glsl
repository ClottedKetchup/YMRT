#version 450 core

layout(location = 0) in vec2 tc;

layout(binding = 0) uniform sampler2D screen;

layout(location = 0) out vec4 color;

void main()
{
	color = texture(screen, tc);
//	color = vec4(1.0, 0.0, 0.0, 1.0);
}