#version 420 // GLSL 4.20
// an ultra simple glsl fragment shader

//Out Pixel Info
out vec4 Pixel;

//OBJ_ATTRIBUTES reference data type
struct OBJ_ATTRIBUTES
{
	vec3			Kd; // diffuse reflectivity
	float			d; // dissolve (transparency) 
	vec3			Ks; // specular reflectivity
	float			Ns; // specular exponent
	vec3			Ka; // ambient reflectivity
	float			sharpness; // local reflection map sharpness
	vec3			Tf; // transmission filter
	float			Ni; // optical density (index of refraction)
	vec3			Ke; // emissive reflectivity
	uint			illum; // illumination model
};
//UBO #1 - SceneData
layout(row_major, binding = 1) uniform SceneData
{
	vec4 sunDirection, sunColor;
	mat4 viewMatrix, projectionMatrix;
	mat4 minimapViewMatrix, minimapProjectionMatrix;
	vec4 minimapCameraPos; //Minimap Camera POS
	vec4 cameraPos;
	vec4 sunAmbient;
};
//UBO #2 - ModelData
layout(row_major, binding = 2) uniform ModelData
{
	mat4 worldMatrix;
	OBJ_ATTRIBUTES material;
};

//In Vector Info
in vec3 worldNorm;
in vec3 worldPos;

void main()
{
	//Applying the Directional Light
	float lightRatio = clamp(dot(normalize(-sunDirection.xyz), normalize(worldNorm.xyz)), 0, 1);
	vec3 totalDirect = clamp((lightRatio * sunColor.xyz), 0, 1);
	vec3 totalIndirect = clamp((material.Ka.xyz * sunAmbient.xyz), 0, 1);
	vec3 diffuse = material.Kd;
	//VIEWDIR = NORMALIZE(CAMWORLDPOS – SURFACEPOS)
	vec4 camWorldPos = cameraPos;
	vec3 viewDir = normalize(camWorldPos.xyz - worldPos.xyz);
	//HALFVECTOR = NORMALIZE((-LIGHTDIR) + VIEWDIR)
	vec3 halfVector = normalize((-normalize(sunDirection.xyz)) + viewDir);
	//INTENSITY = MAX(CLAMP(DOT(NORMAL, HALFVECTOR))SPECULARPOWER, 0);
	float intensity = max(pow((clamp(dot(normalize(worldNorm.xyz), halfVector), 0, 1)), material.Ns + 0.00001), 0);
	//REFLECTEDLIGHT = LIGHTCOLOR * SPECULARINTENSITY * INTENSITY;
	vec3 totalReflected = sunColor.xyz * material.Ks * intensity;
	//Put it all together
	//RETURN SATURATE(TOTALDIRECT + TOTALINDIRECT) * DIFFUSE + TOTALREFLECTED + EMISSIVE
	Pixel = vec4(clamp((totalDirect + totalIndirect), 0, 1) * diffuse + totalReflected + material.Ke, material.d);

}