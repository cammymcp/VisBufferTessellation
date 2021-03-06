#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// Structs
struct Vertex
{
	vec4 posXYZnormX;
	vec4 normYZtexXY;
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

// Constants
const float heightTexScale = 8.0f;
const float heightScale = 5.0f;

// In
layout(location = 0) in vec2 inScreenPos;

// Out
layout(location = 0) out vec4 outColour;

// Descriptors
layout (set = 0, binding = 0) uniform sampler2D textureSampler;
layout (input_attachment_index = 0, set = 0, binding = 1) uniform subpassInput inputVisibility;
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
layout(set = 0, binding = 5) uniform SettingsUniformBufferObject
{
	uint tessellationFactor;
	uint showVisibilityBuffer;
	uint showTessCoordsBuffer;
	uint showInterpolatedTexCoords;
	uint wireframe;
} settings;
layout(set = 0, binding = 6) uniform sampler2D heightmap;
layout(set = 0, binding = 7) uniform sampler2D normalmap;
layout(set = 0, binding = 8) uniform DirectionalLightUniformBufferObject
{
	vec4 direction;
	vec4 ambient;
	vec4 diffuse;
} light;

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

// Interpolate vertex attributes at point 'd' using the partial derivatives
vec3 Interpolate3DAttributes(mat3 attributes, vec3 dbDx, vec3 dbDy, vec2 d)
{
	vec3 attribute_x = attributes * dbDx;
	vec3 attribute_y = attributes * dbDy;
	vec3 attribute_s = attributes[0];
	
	return (attribute_s + d.x * attribute_x + d.y * attribute_y);
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

// Takes draw call ID and primitive ID and returns the three patch control points
Vertex[3] LoadTriangleVertices(uint drawID, uint primID)
{
	Vertex[3] vertices;

	// Index of the first vertex of this draw call's geometry
	uint startIndex = drawID; // There's only one draw call at the moment, so drawID should always be 0. This would work out the offset of the first vertex in the nth draw call if using MultiDraw commands. 

	// Get position in the Index Buffer of each vertex in this triangle (eg 31, 32, 33)
	uint triVert1IndexBufferPosition = (primID * 3 + 0) + startIndex;
	uint triVert2IndexBufferPosition = (primID * 3 + 1) + startIndex;
	uint triVert3IndexBufferPosition = (primID * 3 + 2) + startIndex;	

	// Now get vertex index from index buffer (eg, 17, 41, 32)
	uint triVert0Index = indexBuffer[triVert1IndexBufferPosition].val;
	uint triVert1Index = indexBuffer[triVert2IndexBufferPosition].val;
	uint triVert2Index = indexBuffer[triVert3IndexBufferPosition].val;

	// Load vertex data of the 3 control points
	vertices[0] = vertexBuffer[triVert0Index];
	vertices[1] = vertexBuffer[triVert1Index];
	vertices[2] = vertexBuffer[triVert2Index];

	return vertices;
}

void main() 
{
	// Unpack triangle ID and draw ID from visibility buffer
	vec4 visibilityRaw = subpassLoad(inputVisibility);
	uint DrawIdTriId = packUnorm4x8(visibilityRaw);	

	// If this pixel doesn't contain triangle data, return early
	if (visibilityRaw != vec4(0.0))
	{
		uint drawID = (DrawIdTriId >> 23) & 0x000000FF; // Draw ID the index of the draw call to which the triangle belongs
		uint triangleID = (DrawIdTriId & 0x007FFFFF) - 1; // Triangle ID is the offset of the triangle within the draw call. i.e. it is relative to drawID		
		
		// Load triangle vertices using visibility buffer data
		Vertex[3] vertices = LoadTriangleVertices(drawID, triangleID);

		// Load position data of the 3 vertices
		vec3 vert0Pos = vertices[0].posXYZnormX.xyz;
		vec3 vert1Pos = vertices[1].posXYZnormX.xyz;
		vec3 vert2Pos = vertices[2].posXYZnormX.xyz;		

		// Now displace each vertex by heightmap
		vert0Pos.y += texture(heightmap, vertices[0].normYZtexXY.zw / heightTexScale).r * heightScale; 
		vert1Pos.y += texture(heightmap, vertices[1].normYZtexXY.zw / heightTexScale).r * heightScale;
		vert2Pos.y += texture(heightmap, vertices[2].normYZtexXY.zw / heightTexScale).r * heightScale;

		// Get normals for each Vertex
		vec3 vert0Norm = texture(normalmap, vertices[0].normYZtexXY.zw / heightTexScale).rgb;
		vec3 vert1Norm = texture(normalmap, vertices[1].normYZtexXY.zw / heightTexScale).rgb;
		vec3 vert2Norm = texture(normalmap, vertices[2].normYZtexXY.zw / heightTexScale).rgb;

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
			vec2 (vertices[0].normYZtexXY.zw),
			vec2 (vertices[1].normYZtexXY.zw),
			vec2 (vertices[2].normYZtexXY.zw)
		};
		vec2 interpTexCoords = Interpolate2DAttributes(triTexCoords, derivatives.dbDx, derivatives.dbDy, delta);

		// Interpolate normal
		mat3 triNormals =
		{
			vert0Norm,
			vert1Norm,
			vert2Norm
		};
		vec3 interpNorm = Interpolate3DAttributes(triNormals, derivatives.dbDx, derivatives.dbDy, delta);

		// Get fragment colour from texture
		vec4 textureDiffuseColour = texture(textureSampler, interpTexCoords);

		// Calculate directional light colour contribution
		vec4 lightColour = light.ambient;
		float lightIntensity = clamp(dot(-interpNorm, light.direction.xyz), 0.0f, 1.0f);
		lightColour += light.diffuse * lightIntensity;
		lightColour = clamp(lightColour, 0.0f, 1.0f);

		// Final Fragment colour
		outColour =  clamp(textureDiffuseColour * lightColour, 0.0f, 1.0f);

		// Draw visibility buffer instead if setting is used.
		if (settings.showVisibilityBuffer == 1)
			outColour = visibilityRaw;
		else if (settings.showInterpolatedTexCoords == 1)
			outColour = vec4(normalize(abs(interpTexCoords)), 0.0f, 1.0f);
			//outColour = vec4(interpNorm, 1.0f);
	}
	else
	{
		outColour = vec4(0.35f, 0.55f, 0.7f, 1.0f);
	}
}