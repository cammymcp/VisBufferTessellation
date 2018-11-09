#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
//#extension GL_NV_fragment_shader_barycentric : enable

#define PRIMITIVE_ID_BITS     23

// Force early depth/stencil test
layout(early_fragment_tests) in;

// In
layout(location = 0) in vec2 texCoords;
layout(location = 1) in flat uint drawID;

// Descriptors
layout(binding = 1) uniform sampler2D texSampler;

// Out (To Visibility Buffer)
layout(location = 0) out uint visBuff;
layout(location = 1) out uvec2 uvDerivs;

// Engel's packing function minus the alpha bit
uint calculateOutputVBID(uint drawID, uint primitiveID)
{
	uint drawID_primID = ((drawID << PRIMITIVE_ID_BITS) & 0x7F800000) | (primitiveID & 0x007FFFFF);
	return drawID_primID;
}

void main() 
{
	// Fill visibility buffer
	//idAndBarys.x = calculateOutputVBID(true, drawID, gl_PrimitiveID);
	//vec3 barycentricCoords = gl_BaryCoordNV;
	//idAndBarys.y = packUnorm2x16(barycentricCoords.xy);

	visBuff = calculateOutputVBID(drawID, gl_PrimitiveID);

	// Fill UV derivatives buffer
	uvDerivs.x = packSnorm2x16(dFdx(texCoords));
	uvDerivs.y = packSnorm2x16(dFdy(texCoords));
}