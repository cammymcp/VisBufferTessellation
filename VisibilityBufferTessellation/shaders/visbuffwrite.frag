#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
//#extension GL_NV_fragment_shader_barycentric : enable

#define PRIMITIVE_ID_BITS 23

// Force early depth/stencil test
layout(early_fragment_tests) in;

// In
layout(location = 0) in vec2 texCoords;
layout(location = 1) in flat uint drawID;

// Out (To Visibility Buffer)
layout(location = 0) out vec4 visBuff;
layout(location = 1) out vec4 uvDerivs;

// Engel's packing function
uint calculateOutputVBID(bool opaque, uint drawID, uint primitiveID)
{
	uint drawID_primID = ((drawID << 23) & 0x7F800000) | (primitiveID & 0x007FFFFF);
	return (opaque) ? drawID_primID : (1 << 31) | drawID_primID;
}

void main() 
{
	// Fill visibility buffer
	//idAndBarys.x = calculateOutputVBID(true, drawID, gl_PrimitiveID);
	//vec3 barycentricCoords = gl_BaryCoordNV;
	//idAndBarys.y = packUnorm2x16(barycentricCoords.xy);

	visBuff = unpackUnorm4x8(calculateOutputVBID(true, drawID, gl_PrimitiveID));

	// Fill UV derivatives buffer
	uvDerivs.xy = dFdx(texCoords);
	uvDerivs.zw = dFdy(texCoords);
}