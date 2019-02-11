#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// Descriptors
layout(binding = 1) uniform UniformBufferObject 
{
    mat4 mvp;
    mat4 proj;
} ubo;

layout(binding = 2) uniform sampler2D heightmap;

layout(quads, equal_spacing, cw) in;

layout (location = 0) in vec3 inNormal[];
layout (location = 1) in vec2 inTexCoords[];
 
layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outTexCoords;

void main()
{
	// Interpolate UV coordinates
	vec2 uv1 = mix(inTexCoords[0], inTexCoords[1], gl_TessCoord.x);
	vec2 uv2 = mix(inTexCoords[3], inTexCoords[2], gl_TessCoord.x);
	outTexCoords = mix(uv1, uv2, gl_TessCoord.y);

	// Interpolate normals
	vec3 n1 = mix(inNormal[0], inNormal[1], gl_TessCoord.x);
	vec3 n2 = mix(inNormal[3], inNormal[2], gl_TessCoord.x);
	outNormal = mix(n1, n2, gl_TessCoord.y);

	// Interpolate positions
	vec4 pos1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
	vec4 pos2 = mix(gl_in[3].gl_Position, gl_in[2].gl_Position, gl_TessCoord.x);
	vec4 pos = mix(pos1, pos2, gl_TessCoord.y);

	// Displace height
	//pos.y -= textureLod(heightmap, outTexCoords, 0.0).r * /*ubo.displacementFactor*/ 8;

	// Perspective projection
	gl_Position = ubo.mvp * pos;
}