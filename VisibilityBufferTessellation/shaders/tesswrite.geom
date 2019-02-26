#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

layout (location = 0) in vec3 inNormal[];
layout (location = 1) in vec2 inTexCoords[];
layout (location = 2) in vec3 inTessCoords[];

layout (location = 0) flat out uvec3 outTessCoords;
layout (location = 1) flat out int primitiveID;

void main(void)
{	
	// Pack tessellation coordinates into uint vector
	uint tessCoord0 = packUnorm4x8(vec4(inTessCoords[0], 0.0));
	uint tessCoord1 = packUnorm4x8(vec4(inTessCoords[1], 0.0));
	uint tessCoord2 = packUnorm4x8(vec4(inTessCoords[2], 0.0));
	outTessCoords = uvec3(tessCoord0, tessCoord1, tessCoord2);

	// Store patch ID
	primitiveID = gl_PrimitiveIDIn;

	// Positions are already in screen space from evaluation stage
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();
	
	gl_Position = gl_in[1].gl_Position;
	EmitVertex();

	gl_Position = gl_in[2].gl_Position;
	EmitVertex();
	
	EndPrimitive();
}