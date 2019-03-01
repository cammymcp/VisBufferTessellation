#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(binding = 0) uniform UniformBufferObject
{
	uint tessellationFactor;
	uint showVisibilityBuffer;
	uint showTessCoordsBuffer;
	uint showInterpolatedTexCoords;
	uint wireframe;
} settings;

layout (vertices = 3) out;

void main()
{
	if (gl_InvocationID == 0)
	{
		if (settings.tessellationFactor > 0)
		{
			gl_TessLevelOuter[0] = settings.tessellationFactor;
			gl_TessLevelOuter[1] = settings.tessellationFactor;
			gl_TessLevelOuter[2] = settings.tessellationFactor;
			gl_TessLevelInner[0] = settings.tessellationFactor;
		}
		else
		{
			// Tessellation factor can be set to zero by example
			// to demonstrate a simple passthrough
			gl_TessLevelInner[0] = 1.0;
			gl_TessLevelOuter[0] = 1.0;
			gl_TessLevelOuter[1] = 1.0;
			gl_TessLevelOuter[2] = 1.0;
		}
	}

	// Pass world position through
	gl_out[gl_InvocationID].gl_Position =  gl_in[gl_InvocationID].gl_Position;
} 
