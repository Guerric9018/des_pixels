#version 330 core

layout(location = 2) in vec4 inst_endpoints;

uniform float scale;

void main()
{
	vec2 pos = inst_endpoints.xy;
	if ((gl_VertexID & 1) == 1) {
		pos = inst_endpoints.zw;
	}
	pos = pos * scale;
	pos.y = 1.0 - pos.y;
	gl_Position = vec4(pos - vec2(0.5, 0.5), 0.0, 1.0);
}


