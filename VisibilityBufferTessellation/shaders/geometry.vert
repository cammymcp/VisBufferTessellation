#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// In
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColour;
layout(location = 2) in vec2 texCoords;

// Descriptors
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

// Out
layout(location = 0) out vec3 outColour;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outWorldPos;
out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
	// Screen Position
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);

	// World space vertex position
	outWorldPos = vec3(ubo.model * vec4(inPosition, 1.0));
	// GL to Vulkan coord space
	outWorldPos.y = -outWorldPos.y;

	// Colour
    outColour = inColour;
	
	// Tex
	outTexCoord = texCoords;
	outTexCoord.t = 1.0 - outTexCoord.t; // Might not be needed
}