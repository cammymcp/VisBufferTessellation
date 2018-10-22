#version 450
#extension GL_ARB_separate_shader_objects : enable

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
out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    outColour = inColour;
	outTexCoord = texCoords;
}