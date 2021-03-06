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
layout (input_attachment_index = 1, set = 0, binding = 9) uniform subpassInput inputTessCoords1;
layout (input_attachment_index = 1, set = 0, binding = 10) uniform subpassInput inputTessCoords2;
layout (input_attachment_index = 1, set = 0, binding = 11) uniform subpassInput inputTessCoords3;
layout(set = 0, binding = 2) uniform MVPUniformBufferObject 
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

vec2 Interpolate2DLinear(vec2 v0, vec2 v1, vec2 v2, vec3 tessCoord)
{
   	return vec2(tessCoord.x) * v0 + vec2(tessCoord.y) * v1 + vec2(tessCoord.z) * v2;
}

vec3 Interpolate3DLinear(vec3 v0, vec3 v1, vec3 v2, vec3 tessCoord)
{
   	return vec3(tessCoord.x) * v0 + vec3(tessCoord.y) * v1 + vec3(tessCoord.z) * v2;
}

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
Vertex[3] LoadPatchControlPoints(uint drawID, uint primID)
{
	Vertex[3] controlPoints;

	// Index of the first vertex of this draw call's geometry
	uint startIndex = drawID; // There's only one draw call at the moment, so drawID should always be 0

	// Get position in the Index Buffer of each vertex in this triangle (eg 31, 32, 33)
	uint triVert1IndexBufferPosition = (primID * 3 + 0) + startIndex;
	uint triVert2IndexBufferPosition = (primID * 3 + 1) + startIndex;
	uint triVert3IndexBufferPosition = (primID * 3 + 2) + startIndex;	

	// Now get vertex index from index buffer (eg, 17, 41, 32)
	uint triVert0Index = indexBuffer[triVert1IndexBufferPosition].val;
	uint triVert1Index = indexBuffer[triVert2IndexBufferPosition].val;
	uint triVert2Index = indexBuffer[triVert3IndexBufferPosition].val;

	// Load vertex data of the 3 control points
	controlPoints[0] = vertexBuffer[triVert0Index];
	controlPoints[1] = vertexBuffer[triVert1Index];
	controlPoints[2] = vertexBuffer[triVert2Index];

	return controlPoints;
}

// Re-evaluates the evaluation stage for the tessellated primitive this fragment belongs to.
// Takes input patch control points and interpolates to tessellated vertices with stored 
// tessellation coordinates
Vertex[3] EvaluateTessellatedPrimitive(Vertex[3] patchControlPoints, vec4 tessCoords_v1XYZ_v2X, vec4 tessCoords_v2YZ_v3XY, float tessCoords_v3Z)
{
	Vertex[3] vertices;

	// Extract tessellation (barycentric) coordinates for the three vertices
	vec3 tessCoord0 = tessCoords_v1XYZ_v2X.xyz;
	vec3 tessCoord1 = vec3(tessCoords_v1XYZ_v2X.w, tessCoords_v2YZ_v3XY.xy);
	vec3 tessCoord2 = vec3(tessCoords_v2YZ_v3XY.zw, tessCoords_v3Z);
	
	// Interpolate positions
	vertices[0].posXYZnormX.xyz = Interpolate3DLinear(patchControlPoints[0].posXYZnormX.xyz, patchControlPoints[1].posXYZnormX.xyz, patchControlPoints[2].posXYZnormX.xyz, tessCoord0);
	vertices[1].posXYZnormX.xyz = Interpolate3DLinear(patchControlPoints[0].posXYZnormX.xyz, patchControlPoints[1].posXYZnormX.xyz, patchControlPoints[2].posXYZnormX.xyz, tessCoord1);
	vertices[2].posXYZnormX.xyz = Interpolate3DLinear(patchControlPoints[0].posXYZnormX.xyz, patchControlPoints[1].posXYZnormX.xyz, patchControlPoints[2].posXYZnormX.xyz, tessCoord2);

	// Not bothering with normals until lighting is implemented
	vertices[0].posXYZnormX.w = 0.0; vertices[0].normYZtexXY.xy = vec2(0.0, 1.0);
	vertices[1].posXYZnormX.w = 0.0; vertices[1].normYZtexXY.xy = vec2(0.0, 1.0);
	vertices[2].posXYZnormX.w = 0.0; vertices[2].normYZtexXY.xy = vec2(0.0, 1.0);

	// Interpolate UV coordinates
	vertices[0].normYZtexXY.zw = Interpolate2DLinear(patchControlPoints[0].normYZtexXY.zw, patchControlPoints[1].normYZtexXY.zw, patchControlPoints[2].normYZtexXY.zw, tessCoord0);
	vertices[1].normYZtexXY.zw = Interpolate2DLinear(patchControlPoints[0].normYZtexXY.zw, patchControlPoints[1].normYZtexXY.zw, patchControlPoints[2].normYZtexXY.zw, tessCoord1);
	vertices[2].normYZtexXY.zw = Interpolate2DLinear(patchControlPoints[0].normYZtexXY.zw, patchControlPoints[1].normYZtexXY.zw, patchControlPoints[2].normYZtexXY.zw, tessCoord2);

	return vertices;
}

void main() 
{
	// Unpack triangle ID and draw ID from visibility buffer
	vec4 visibilityRaw = subpassLoad(inputVisibility);
	vec4 tessCoords_v1XYZ_v2X = subpassLoad(inputTessCoords1);
	vec4 tessCoords_v2YZ_v3XY = subpassLoad(inputTessCoords2);
	float tessCoords_v3Z = subpassLoad(inputTessCoords3).x;
	uint DrawIdTriId = packUnorm4x8(visibilityRaw);

	// If this pixel doesn't contain triangle data, return early
	if (visibilityRaw != vec4(0.0))
	{
		// Output debug tess coords
		vec4 tessCoordsColour = vec4(packUnorm4x8(vec4(tessCoords_v1XYZ_v2X.xyz, 0)), packUnorm4x8(vec4(tessCoords_v1XYZ_v2X.w, tessCoords_v2YZ_v3XY.xy, 0)), packUnorm4x8(vec4(tessCoords_v2YZ_v3XY.zw, tessCoords_v3Z, 0)), 1.0);
		tessCoordsColour = normalize(tessCoordsColour);

		uint drawID = (DrawIdTriId >> 23) & 0x000000FF; // Draw ID the number of draw call to which the triangle belongs
		uint triangleID = (DrawIdTriId & 0x007FFFFF) - 1; // Triangle ID is the offset of the triangle within the draw call. i.e. it is relative to drawID
		
		// Load input patch control points using visibility buffer data
		Vertex[3] patchControlPoints = LoadPatchControlPoints(drawID, triangleID);

		// Now interpolate to the generated tessellation primitive using stored tess coords
		Vertex[3] primitiveVertices = EvaluateTessellatedPrimitive(patchControlPoints, tessCoords_v1XYZ_v2X, tessCoords_v2YZ_v3XY, tessCoords_v3Z);
		
		// Get position data of vertices
		vec3 vertPos0 = primitiveVertices[0].posXYZnormX.xyz;
		vec3 vertPos1 = primitiveVertices[1].posXYZnormX.xyz;
		vec3 vertPos2 = primitiveVertices[2].posXYZnormX.xyz;		
	
		// Now displace each vertex by heightmap
		vertPos0.y += texture(heightmap, primitiveVertices[0].normYZtexXY.zw / heightTexScale).r * heightScale;
		vertPos1.y += texture(heightmap, primitiveVertices[1].normYZtexXY.zw / heightTexScale).r * heightScale;
		vertPos2.y += texture(heightmap, primitiveVertices[2].normYZtexXY.zw / heightTexScale).r * heightScale;

		// Get normals for each Vertex
		vec3 vert0Norm = texture(normalmap, primitiveVertices[0].normYZtexXY.zw / heightTexScale).rgb;
		vec3 vert1Norm = texture(normalmap, primitiveVertices[1].normYZtexXY.zw / heightTexScale).rgb;
		vec3 vert2Norm = texture(normalmap, primitiveVertices[2].normYZtexXY.zw / heightTexScale).rgb;

		// Transform positions to clip space
		vec4 clipPos0 = ubo.mvp * vec4(vertPos0, 1);
		vec4 clipPos1 = ubo.mvp * vec4(vertPos1, 1);
		vec4 clipPos2 = ubo.mvp * vec4(vertPos2, 1);

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
			vec2 (primitiveVertices[0].normYZtexXY.z, primitiveVertices[0].normYZtexXY.w),
			vec2 (primitiveVertices[1].normYZtexXY.z, primitiveVertices[1].normYZtexXY.w),
			vec2 (primitiveVertices[2].normYZtexXY.z, primitiveVertices[2].normYZtexXY.w)
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

		// Draw visibility buffer images instead if settings are used.
		if (settings.showVisibilityBuffer == 1)
			outColour = visibilityRaw;
		else if (settings.showTessCoordsBuffer == 1)
			outColour = tessCoordsColour;
		else if (settings.showInterpolatedTexCoords == 1)
			outColour = vec4(normalize(abs(interpTexCoords)), 0.0f, 1.0f);
			//outColour = vec4(interpNorm, 1.0f);
	}
	else
	{
		outColour = vec4(0.35f, 0.55f, 0.7f, 1.0f);
	}

}