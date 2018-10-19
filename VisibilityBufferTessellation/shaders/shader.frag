#version 450
#extension GL_ARB_separate_shader_objects : enable

// In
layout(location = 0) in vec3 colour;
layout(location = 1) in vec2 texCoords;

// Descriptors
layout(binding = 1) uniform sampler2D texSampler;

// Out
layout(location = 0) out vec4 outColour;

void main() 
{
    outColour = texture(texSampler, texCoords);
}