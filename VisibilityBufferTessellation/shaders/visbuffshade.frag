#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#define PRIMITIVE_ID_BITS 23

// Structs
struct Vertex
{
	vec4 posXYZcolX;
	vec4 colYZtexXY;
};
struct Index
{
	uint val;
};
struct DerivativesOutput
{
	vec3 dbDx;
	vec3 dbDy;
};

// In
layout(location = 0) in vec2 inScreenPos;

// Out
layout(location = 0) out vec4 outColour;
layout(location = 1) out vec4 debug;

// Descriptors
layout (set = 0, binding = 0) uniform sampler2D textureSampler;
layout (set = 0, binding = 1) uniform sampler2D inputVisibility;
layout(set = 0, binding = 2) uniform UniformBufferObject 
{
    mat4 mvp;
    mat4 proj;
} ubo;
layout (std430, set = 0, binding = 3) readonly buffer IndxBuff
{
	Index indexBuffer[];
};
layout (std430, set = 0, binding = 4) readonly buffer VertBuff
{
	Vertex vertexBuffer[];
};

// Interpolate 2D attributes using the partial derivatives and generates dx and dy for texture sampling.
vec2 Interpolate2DAttributes(mat3x2 attributes, vec3 dbDx, vec3 dbDy, vec2 d)
{
	vec3 attr0 = vec3(attributes[0].x, attributes[1].x, attributes[2].x);
	vec3 attr1 = vec3(attributes[0].y, attributes[1].y, attributes[2].y);
	vec2 attribute_x = vec2(dot(dbDx,attr0), dot(dbDx,attr1));
	vec2 attribute_y = vec2(dot(dbDy,attr0), dot(dbDy,attr1));
	vec2 attribute_s = attributes[0];
	
	vec2 result = (attribute_s + d.x * attribute_x + d.y * attribute_y);
	return result;
}

// Engel's barycentric coord partial derivs function. Follows equation from [Schied][Dachsbacher]
// Computes the partial derivatives of point's barycentric coordinates from the projected screen space vertices
DerivativesOutput ComputePartialDerivatives(vec2 v[3])
{
	DerivativesOutput derivatives;
	float d = 1.0 / determinant(mat2(v[2] - v[1], v[0] - v[1]));
	derivatives.dbDx = vec3(v[1].y - v[2].y, v[2].y - v[0].y, v[0].y - v[1].y) * d;
	derivatives.dbDy = vec3(v[2].x - v[1].x, v[0].x - v[2].x, v[1].x - v[0].x) * d;
	return derivatives;
}

void main() 
{
	// Unpack triangle ID and draw ID from visibility buffer
	vec4 visibilityRaw = texelFetch(inputVisibility, ivec2(gl_FragCoord.xy), 0);
	uint DrawIdTriId = packUnorm4x8(visibilityRaw);		
	debug = vec4(0.0f, 0.0f, 0.0f, 0.0f);

	// If this pixel doesn't contain triangle data, return early
	if (length(visibilityRaw) != 0)
	{
		uint drawID = (DrawIdTriId >> 23) & 0x000000FF; // Draw ID the number of draw call to which the triangle belongs
		uint triangleID = (DrawIdTriId & 0x007FFFFF); // Triangle ID is the offset of the triangle within the draw call. i.e. it is relative to drawID
		
		// Index of the first vertex of this draw call's geometry
		uint startIndex = drawID; // There's only one draw call at the moment, so drawID should always be 0

		// Get position in the Index Buffer of each vertex in this triangle (eg 31, 32, 33)
		uint triVert1IndexBufferPosition = (triangleID * 3 + 0) + startIndex;
		uint triVert2IndexBufferPosition = (triangleID * 3 + 1) + startIndex;
		uint triVert3IndexBufferPosition = (triangleID * 3 + 2) + startIndex;	

		// Now get vertex index from index buffer (eg, 17, 41, 32)
		uint triVert0Index = indexBuffer[triVert1IndexBufferPosition].val;
		uint triVert1Index = indexBuffer[triVert2IndexBufferPosition].val;
		uint triVert2Index = indexBuffer[triVert3IndexBufferPosition].val;

		// Load vertex data of the 3 vertices
		vec3 vert0Pos = vertexBuffer[triVert0Index].posXYZcolX.xyz;
		vec3 vert1Pos = vertexBuffer[triVert1Index].posXYZcolX.xyz;
		vec3 vert2Pos = vertexBuffer[triVert2Index].posXYZcolX.xyz;

		// Transform positions to clip space
		vec4 clipPos0 = ubo.mvp * vec4(vert0Pos, 1);
		vec4 clipPos1 = ubo.mvp * vec4(vert1Pos, 1);
		vec4 clipPos2 = ubo.mvp * vec4(vert2Pos, 1);

		// Pre-calculate 1 over w components
		vec3 oneOverW = 1.0 / vec3(clipPos0.w, clipPos1.w, clipPos2.w);

		// Calculate 2D screen positions
		clipPos0 *= oneOverW[0];
		clipPos1 *= oneOverW[1];
		clipPos2 *= oneOverW[2];
		vec2 screenPositions[3] = { clipPos0.xy, clipPos1.xy, clipPos2.xy };

		// Get barycentric coordinates for attribute interpolation
		DerivativesOutput derivatives = ComputePartialDerivatives(screenPositions);

		// Get delta vector that describes current screen point relative to vertex 0
		vec2 delta = inScreenPos + -screenPositions[0];

		// Interpolate texture coordinates
		mat3x2 triTexCoords =
		{
			vec2 (vertexBuffer[triVert0Index].colYZtexXY.z, vertexBuffer[triVert0Index].colYZtexXY.w),
			vec2 (vertexBuffer[triVert1Index].colYZtexXY.z, vertexBuffer[triVert1Index].colYZtexXY.w),
			vec2 (vertexBuffer[triVert2Index].colYZtexXY.z, vertexBuffer[triVert2Index].colYZtexXY.w)
		};
		vec2 interpTexCoords = Interpolate2DAttributes(triTexCoords, derivatives.dbDx, derivatives.dbDy, delta);
		debug = vec4(interpTexCoords, 0.0f, 1.0f);

		// Get fragment colour from texture
		vec4 textureDiffuseColour = texture(textureSampler, interpTexCoords);

		// Final Fragment colour
		outColour = textureDiffuseColour;
	}
	else
	{
		outColour = vec4(0.2f, 0.2f, 0.2f, 1.0f);
	}

}