#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_ARB_shader_image_load_store : enable
#extension GL_NV_fragment_shader_barycentric : enable

// Force early depth/stencil test
layout(early_fragment_tests) in;

// In
layout(location = 0) in vec2 texCoords;
layout(location = 1) in vec3 worldPos;

// Descriptors
layout(binding = 1) uniform sampler2D texSampler;

// Out (To Gbuffer)
layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;

void main() 
{
	// position
	outPosition = vec4(worldPos, 1.0);

	// normals (none yet, just output white)
	outNormal = vec4(1.0, 1.0, 1.0, 1.0);

	// colour
    outAlbedo = texture(texSampler, texCoords);
}