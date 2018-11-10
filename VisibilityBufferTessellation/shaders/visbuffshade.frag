#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
//#extension GL_KHR_vulkan_glsl : enable

#define PRIMITIVE_ID_BITS 23

// Structs
struct Vertex
{
	vec3 pos;
	vec3 col;
	vec2 tex;
};

// In
layout(location = 0) in vec2 texCoords;

// Descriptors
layout(set = 0, binding = 0) uniform UniformBufferObject 
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;
layout (set = 0, binding = 2) uniform sampler2D textureSampler;
layout (set = 0, binding = 3) uniform sampler2D inputVisibility;
layout (set = 0, binding = 4) uniform sampler2D inputUVDerivs;
layout (set = 0, binding = 5) uniform sampler2D inputDepth;
layout (std430, set = 0, binding = 6) buffer IndxBuff
{
	uint indexBuffer[];
};
layout (std430, set = 0, binding = 7) buffer VertBuff
{
	Vertex vertexBuffer[];
};

// Out
layout(location = 0) out vec4 outColour;

void main() 
{
	// Unpack triangle ID and draw ID from visibility buffer
	vec4 visibilityRaw = texelFetch(inputVisibility, ivec2(gl_FragCoord.xy), 0);
	uint alphaBitDrawIdTriId = packUnorm4x8(visibilityRaw);

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
		uint triVert0Index = indexBuffer[triVert1IndexBufferPosition];
		uint triVert1Index = indexBuffer[triVert2IndexBufferPosition];
		uint triVert2Index = indexBuffer[triVert3IndexBufferPosition];

		// Load vertex data of the 3 vertices
		vec3 vert0Pos = vec3(vertexBuffer[triVert0Index].pos.x, vertexBuffer[triVert0Index].pos.y, vertexBuffer[triVert0Index].pos.z);
		vec3 vert1Pos = vec3(vertexBuffer[triVert1Index].pos.x, vertexBuffer[triVert1Index].pos.y, vertexBuffer[triVert1Index].pos.z);
		vec3 vert2Pos = vec3(vertexBuffer[triVert2Index].pos.x, vertexBuffer[triVert2Index].pos.y, vertexBuffer[triVert2Index].pos.z);

		// Transform positions to clip space
		vec4 screenPos0 = ubo.model * ubo.view * ubo.proj * vec4(vert0Pos, 1.0f);
		vec4 screenPos1 = ubo.model * ubo.view * ubo.proj * vec4(vert1Pos, 1.0f);
		vec4 screenPos2 = ubo.model * ubo.view * ubo.proj * vec4(vert2Pos, 1.0f);

		// Final Fragment colour
		outColour = screenPos0;
	}
	else
	{
		outColour = vec4(0.2f, 0.2f, 0.2f, 1.0f);
	}
}