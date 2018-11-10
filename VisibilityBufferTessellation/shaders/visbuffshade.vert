#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// Descriptors
layout(binding = 0) uniform ModelUniformBufferObject 
{
    mat4 model;
    mat4 view;
    mat4 proj;
} modelubo;
layout(binding = 1) uniform QuadUniformBufferObject 
{
    mat4 model;
    mat4 view;
    mat4 proj;
} quadubo;

// Out
layout(location = 0) out vec2 outTexCoord;
out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
    outTexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outTexCoord * 2.0f - 1.0f, 0.0f, 1.0f);
}