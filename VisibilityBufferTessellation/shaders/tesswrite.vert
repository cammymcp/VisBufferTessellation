#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_ARB_shader_draw_parameters : enable

// In
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoords;

// Out
layout(location = 0) out vec2 outTexCoords;
out gl_PerVertex
{
	vec4 gl_Position;
};

// Simple passthrough, only position is required. 
void main() 
{
	gl_Position = vec4(inPosition, 1.0);
	outTexCoords = inTexCoords;
}