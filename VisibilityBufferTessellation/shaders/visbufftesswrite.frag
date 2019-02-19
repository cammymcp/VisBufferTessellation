#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#define PRIMITIVE_ID_BITS 23

// Force early depth/stencil test
layout(early_fragment_tests) in;

// In
layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inTexCoords;
layout (location = 2) in vec3 inTessCoords;

// Out 
layout(location = 0) out vec4 outColour;
layout(location = 1) out vec4 debug;
layout(location = 2) out vec4 visBuff;
layout(location = 3) out vec4 tessCoordsBuff;

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
	debug = vec4(0.0);

	// Fill visibility buffer (gl_PrimitiveID in a tessellation pipeline refers to the Patch ID)
	visBuff = unpackUnorm4x8(calculateOutputVBID(0, gl_PrimitiveID + 1)); // Offset primitive ID so that the first primitive in each draw call is not lost due to being 0
	tessCoordsBuff = vec4(inTessCoords, 1.0f);
}