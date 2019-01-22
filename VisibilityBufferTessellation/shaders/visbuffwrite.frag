#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#define PRIMITIVE_ID_BITS 23

// Force early depth/stencil test
layout(early_fragment_tests) in;

// In
layout(location = 0) in flat uint drawID;

// Out (To Visibility Buffer)
layout(location = 0) out vec4 visBuff;

// Engel's packing function (without alpha bit)
uint calculateOutputVBID(uint drawID, uint primitiveID)
{
	uint drawID_primID = ((drawID << 23) & 0x7F800000) | (primitiveID & 0x007FFFFF);
	return drawID_primID;
}

void main() 
{
	// Fill visibility buffer
	visBuff = unpackUnorm4x8(calculateOutputVBID(drawID, gl_PrimitiveID + 1)); // Offset primitive ID so that the first primitive in each draw call is not lost due to being 0
}