#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// In
layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec2 texCoords;

// Descriptors
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

// Out
layout(location = 0) out vec2 outTexCoord;
layout(location = 1) flat out vec4 vertexPos; // Position of this vertex, not to be interpolated for barycentric coord calculations
layout(location = 2)	  out vec4 fragPos; // Temporarily filled with the vertex position. WILL be interpolated to fragment position
out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
	// Screen Position
	vec4 vertScreenPos = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    gl_Position = vertScreenPos;
	vertexPos = vertScreenPos;
	fragPos = vertScreenPos; // Temporary
	
	// Tex
	outTexCoord = texCoords;
	outTexCoord.t = 1.0 - outTexCoord.t; // Flip y component of texcoords to match Vulkan's UV coord system
}