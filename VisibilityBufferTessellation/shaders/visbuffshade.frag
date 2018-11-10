#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// In
layout(location = 0) in vec2 texCoords;

// Descriptors
layout (set = 0, binding = 2) uniform sampler2D textureSampler;
layout (input_attachment_index = 0, set = 0, binding = 3) uniform subpassInput inputVisibility;
layout (input_attachment_index = 1, set = 0, binding = 4) uniform subpassInput inputUVDerivs;
layout (input_attachment_index = 2, set = 0, binding = 5) uniform subpassInput inputDepth;

// Out
layout(location = 0) out vec4 outColour;

void main() 
{
	// Get G-Buffer values	
	//vec3 fragPos = texture(positionSampler, texCoords).rgb;
	//vec3 normal = texture(normalSampler, texCoords).rgb;
	//vec4 albedo = texture(colourSampler, texCoords);

	// Not doing any lighting calculations at the moment, so won't be using position or normal here.

	// Fragment colour
    outColour = vec4(1.0f, 1.0f, 1.0f, 1.0f); //albedo;
}