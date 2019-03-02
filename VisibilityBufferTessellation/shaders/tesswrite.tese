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

// In
layout(triangles, equal_spacing, cw) in;
layout(location = 0) in vec2 inTexCoords[];

// Out
layout (location = 0) out vec3 outTessCoords;

vec2 interpolate2D(vec2 v0, vec2 v1, vec2 v2)
{
   	return vec2(gl_TessCoord.x) * v0 + vec2(gl_TessCoord.y) * v1 + vec2(gl_TessCoord.z) * v2;
}

vec3 interpolate3D(vec3 v0, vec3 v1, vec3 v2)
{
   	return vec3(gl_TessCoord.x) * v0 + vec3(gl_TessCoord.y) * v1 + vec3(gl_TessCoord.z) * v2;
}

void main()
{
	// Interpolate positions
	vec3 pos = interpolate3D(gl_in[0].gl_Position.xyz, gl_in[1].gl_Position.xyz, gl_in[2].gl_Position.xyz);

	// Displace height
	vec2 tex = interpolate2D(inTexCoords[0], inTexCoords[1], inTexCoords[2]);
	pos.y += textureLod(heightmap, tex / 5.0, 0.0).r * 8;

	// Perspective projection
	gl_Position = ubo.mvp * vec4(pos, 1.0);
	outTessCoords = gl_TessCoord;
}