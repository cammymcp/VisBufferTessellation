#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_ARB_shader_draw_parameters : enable

// In
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColour;
layout(location = 2) in vec2 texCoords;

// Out
layout(location = 0) flat out uint drawID;
out gl_PerVertex
{
	vec4 gl_Position;
};

// Descriptors
layout(binding = 0) uniform UniformBufferObject 
{
    mat4 mvp;
    mat4 proj;
} ubo;

void main() 
{
	// Screen Position
	vec4 vertScreenPos = ubo.mvp * vec4(inPosition, 1.0);
    gl_Position = vertScreenPos;

	// DrawID
	drawID = gl_DrawIDARB;
}