#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// Out
layout(location = 0) out vec2 outScreenPos;
out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
	// Create a full screen triangle based on vertex index.
    vec4 position;
	position.x = (gl_VertexIndex == 2) ?  3.0f : -1.0f;
	position.y = (gl_VertexIndex == 0) ? -3.0f :  1.0f;
	position.zw = vec2(0.0f, 1.0f);
	
	outScreenPos = position.xy;
	gl_Position = position;
}