#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_KHR_vulkan_glsl : enable

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

struct BarycentricCoordinates
{
	vec3 dbDx;
	vec3 dbDy;
};

struct GradientInterpolationResults
{
	vec2 interp;
	vec2 dx;
	vec2 dy;
};

// Interpolate 2D attributes using the partial derivatives and generates dx and dy for texture sampling.
GradientInterpolationResults interpolateAttributeWithGradient(mat3x2 attributes, vec3 db_dx, vec3 db_dy, vec2 d)
{
	vec3 attr0 = vec3(attributes[0].x, attributes[1].x, attributes[2].x);
	vec3 attr1 = vec3(attributes[0].y, attributes[1].y, attributes[2].y);
	vec2 attribute_x = vec2(dot(db_dx,attr0), dot(db_dx,attr1));
	vec2 attribute_y = vec2(dot(db_dy,attr0), dot(db_dy,attr1));
	vec2 attribute_s = attributes[0];
	
	GradientInterpolationResults result;
	result.dx = attribute_x;
	result.dy = attribute_y;
	result.interp = (attribute_s + d.x * attribute_x + d.y * attribute_y);
	return result;
}

// Engel's barycentric coord partial derivs function. 
// Computes the partial derivatives of a triangle from the projected screen space vertices
BarycentricCoordinates ComputeBarycentricCoordinates(vec2 v[3])
{
	BarycentricCoordinates baryCoords;
	float d = 1.0 / determinant(mat2(v[2] - v[1], v[0] - v[1]));
	baryCoords.dbDx = vec3(v[1].y - v[2].y, v[2].y - v[0].y, v[0].y - v[1].y) * d;
	baryCoords.dbDy = vec3(v[2].x - v[1].x, v[0].x - v[2].x, v[1].x - v[0].x) * d;
	return baryCoords;
}

vec3 InterpolateAttribute(mat3 attributes, vec3 dbDx, vec3 dbDy, vec2 delta)
{
	vec3 attribute_x = attributes * dbDx;
	vec3 attribute_y = attributes * dbDy;
	vec3 attribute_s = attributes[0];
	
	return (attribute_s + delta.x * attribute_x + delta.y * attribute_y);
}

vec2 InterpolateAttribute(mat3x2 attributes, vec3 dbDx, vec3 dbDy, vec2 delta)
{
	vec3 attr0 = vec3(attributes[0].x, attributes[1].x, attributes[2].x);
	vec3 attr1 = vec3(attributes[0].y, attributes[1].y, attributes[2].y);
	vec2 attribute_x = vec2(dot(dbDx,attr0), dot(dbDx,attr1));
	vec2 attribute_y = vec2(dot(dbDy,attr0), dot(dbDy,attr1));
	vec2 attribute_s = attributes[0];
	
	return (attribute_s + delta.x * attribute_x + delta.y * attribute_y);
}

float InterpolateAttribute(vec3 attributes, vec3 dbDx, vec3 dbDy, vec2 delta)
{
	float attribute_x = dot(attributes, dbDx);
	float attribute_y = dot(attributes, dbDy);
	float attribute_s = attributes[0];
	
	return (attribute_s + delta.x * attribute_x + delta.y * attribute_y);
}

// In
layout(location = 0) in vec2 texCoords;
layout(location = 1) in vec2 inScreenPos;

// Descriptors
layout(set = 0, binding = 0) uniform UniformBufferObject 
{
    mat4 model;
    mat4 view;
	mat4 invView;
    mat4 proj;
} ubo;
layout (set = 0, binding = 2) uniform sampler2D textureSampler;
layout (set = 0, binding = 3) uniform sampler2D inputVisibility;
layout (set = 0, binding = 4) uniform sampler2D inputUVDerivs;
layout (set = 0, binding = 5) uniform sampler2D inputDepth;
layout (std430, set = 0, binding = 6) readonly buffer IndxBuff
{
	Index indexBuffer[];
};
layout (std430, set = 0, binding = 7) readonly buffer VertBuff
{
	Vertex vertexBuffer[];
};

// Out
layout(location = 0) out vec4 outColour;
layout(location = 1) out vec4 debug;

void main() 
{
	// Unpack triangle ID and draw ID from visibility buffer
	vec4 visibilityRaw = texelFetch(inputVisibility, ivec2(gl_FragCoord.xy), 0);
	uint alphaBitDrawIdTriId = packUnorm4x8(visibilityRaw);		
	debug = vec4(0.0f, 0.0f, 0.0f, 0.0f);

	// If this pixel doesn't contain triangle data, return early
	if (length(visibilityRaw) != 0)
	{
		uint drawID = (alphaBitDrawIdTriId >> 23) & 0x000000FF; // Draw ID the number of draw call to which the triangle belongs
		uint triangleID = (alphaBitDrawIdTriId & 0x007FFFFF); // Triangle ID is the offset of the triangle within the draw call. i.e. it is relative to drawID
		uint alphaBit = (alphaBitDrawIdTriId >> 31); // Not using this at the moment	
		
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
		Vertex vert0 = vertexBuffer[triVert0Index];
		vec3 vert0Pos = vec3(vertexBuffer[triVert0Index].posXYZcolX.x, vertexBuffer[triVert0Index].posXYZcolX.y, vertexBuffer[triVert0Index].posXYZcolX.z);
		vec3 vert1Pos = vec3(vertexBuffer[triVert1Index].posXYZcolX.x, vertexBuffer[triVert1Index].posXYZcolX.y, vertexBuffer[triVert1Index].posXYZcolX.z);
		vec3 vert2Pos = vec3(vertexBuffer[triVert2Index].posXYZcolX.x, vertexBuffer[triVert2Index].posXYZcolX.y, vertexBuffer[triVert2Index].posXYZcolX.z);

		// Transform positions to clip space
		vec4 clipPos0 = ubo.model * ubo.view * ubo.proj * vec4(vert0Pos, 1.0f);
		vec4 clipPos1 = ubo.model * ubo.view * ubo.proj * vec4(vert1Pos, 1.0f);
		vec4 clipPos2 = ubo.model * ubo.view * ubo.proj * vec4(vert2Pos, 1.0f);

		// Pre-calculate 1 over w components
		vec3 oneOverW = 1.0f / vec3(clipPos0.w, clipPos1.w, clipPos2.w);

		// Calculate 2D screen positions
		clipPos0 *= oneOverW[0];
		clipPos1 *= oneOverW[1];
		clipPos2 *= oneOverW[2];
		vec2 screenPositions[3] = { clipPos0.xy, clipPos1.xy, clipPos2.xy };

		// Get barycentric coordinates for attribute interpolation
		BarycentricCoordinates baryCoords = ComputeBarycentricCoordinates(screenPositions);

		// Get delta vector that describes current screen point relative to vertex 0
		vec2 delta = inScreenPos + -screenPositions[0];

		// Construct w component of screen point from interpolating 1/w
		float w = 1.0f / InterpolateAttribute(oneOverW, baryCoords.dbDx, baryCoords.dbDy, delta);

		// Construct Z component at this screen point using necessary components in projection matrix
		float z = w * ubo.proj[2][2] + ubo.proj[3][2];

		// Now calculate the world position of this point using projected position and inverse view matrix 
		vec4 projectedPosition = vec4(inScreenPos * w, z, w);
		vec3 position = (ubo.invView * projectedPosition).xyz;
		debug = vec4(position.x, position.y, position.z, 1.0f);

		// Get tex partial derivatives from buffer
		//uvec2 texDerivsRaw = uvec2(texelFetch(inputUVDerivs, ivec2(gl_FragCoord.xy), 0).xy);
		//vec2 texDx = unpackSnorm2x16(texDerivsRaw.x);
		//vec2 texDy = unpackSnorm2x16(texDerivsRaw.y);

		// Interpolate texture coordinates
		mat3x2 triTexCoords =
		{
			vec2 (vertexBuffer[triVert0Index].colYZtexXY.z, vertexBuffer[triVert0Index].colYZtexXY.w),
			vec2 (vertexBuffer[triVert1Index].colYZtexXY.z, vertexBuffer[triVert1Index].colYZtexXY.w),
			vec2 (vertexBuffer[triVert2Index].colYZtexXY.z, vertexBuffer[triVert2Index].colYZtexXY.w)
		};
		GradientInterpolationResults fragTexCoords = interpolateAttributeWithGradient(triTexCoords, baryCoords.dbDx, baryCoords.dbDy, delta);

		// Get fragment colour from texture
		vec4 textureDiffuseColour = textureGrad(textureSampler, fragTexCoords.interp * w, fragTexCoords.dx * w, fragTexCoords.dy * w);

		// Final Fragment colour
		outColour = textureDiffuseColour;
	}
	else
	{
		outColour = vec4(0.2f, 0.2f, 0.2f, 1.0f);
	}

}