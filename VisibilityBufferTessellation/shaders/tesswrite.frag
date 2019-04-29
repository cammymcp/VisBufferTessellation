#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#define PRIMITIVE_ID_BITS 23

// Force early depth/stencil test
layout(early_fragment_tests) in;

// In
layout (location = 0) flat in int primitiveID;
layout (location = 1) flat in uvec3 inTessCoords;
// Out 
layout(location = 0) out vec4 outColour;
layout(location = 1) out vec4 visBuff;
layout(location = 2) out vec4 tessCoordsBuff1;
layout(location = 3) out vec4 tessCoordsBuff2;
layout(location = 4) out float tessCoordsBuff3;

// Engel's packing function (without alpha bit)
uint calculateOutputVBID(uint drawID, uint primitiveID)
{
	uint drawID_primID = ((drawID << 23) & 0x7F800000) | (primitiveID & 0x007FFFFF);
	return drawID_primID;
}

void main() 
{
	// Write to colour attachments to avoid undefined behaviour
	outColour = vec4(0.0); 

	// Fill visibility buffer
	visBuff = unpackUnorm4x8(calculateOutputVBID(0, primitiveID + 1)); // Offset primitive ID so that the first primitive in each draw call is not lost due to being 0

	// Recover tess coords from geometry shader
	vec3 tessCoord0 = unpackUnorm4x8(inTessCoords.x).xyz;
	vec3 tessCoord1 = unpackUnorm4x8(inTessCoords.y).xyz;
	vec3 tessCoord2 = unpackUnorm4x8(inTessCoords.z).xyz;

	// Store tess coords in buffers 
	tessCoordsBuff1 = vec4(tessCoord0, tessCoord1.x);
	tessCoordsBuff2 = vec4(tessCoord1.yz, tessCoord2.xy);
	tessCoordsBuff3 = tessCoord2.z;
}