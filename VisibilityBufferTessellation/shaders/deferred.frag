#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// In
layout(location = 0) in vec2 texCoords;

// Descriptors
layout (binding = 1) uniform sampler2D positionSampler;
layout (binding = 2) uniform sampler2D normalSampler;
layout (binding = 3) uniform sampler2D colourSampler;

// Out
layout(location = 0) out vec4 outColour;

void main() 
{
	// Get G-Buffer values	
	vec3 fragPos = texture(positionSampler, texCoords).rgb;
	vec3 normal = texture(normalSampler, texCoords).rgb;
	vec4 albedo = texture(colourSampler, texCoords);

	// Not doing any lighting calculations at the moment, so won't be using position or normal here.

	// Fragment colour
    outColour = albedo;
}