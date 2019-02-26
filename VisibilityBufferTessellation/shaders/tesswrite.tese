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

layout(triangles, equal_spacing, cw) in;

// Input patch attributes
layout (location = 0) in vec3 inNormal[];
layout (location = 1) in vec2 inTexCoords[];
 
layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outTexCoords;
layout (location = 2) out vec3 outTessCoords;

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
	// Interpolate UV coordinates
	outTexCoords = interpolate2D(inTexCoords[0], inTexCoords[1], inTexCoords[2]);

	// Interpolate normals
	outNormal = interpolate3D(inNormal[0], inNormal[1], inNormal[2]);
	outNormal = normalize(outNormal);

	// Interpolate positions
	vec3 pos = interpolate3D(gl_in[0].gl_Position.xyz, gl_in[1].gl_Position.xyz, gl_in[2].gl_Position.xyz);

	// Displace height
	//pos.y -= textureLod(heightmap, outTexCoords, 0.0).r * /*ubo.displacementFactor*/ 8;

	// Perspective projection
	gl_Position = ubo.mvp * vec4(pos, 1.0);
	outTessCoords = gl_TessCoord;
}