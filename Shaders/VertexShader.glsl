#version 420 // GLSL 4.20
// an ultra simple glsl vertex shader

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
layout(location = 0) in vec3 local_pos;
layout(location = 1) in vec3 uvw;
layout(location = 2) in vec3 norms;

//Out World Info
out vec3 worldNorm;
out vec3 worldPos;

void main()
{
	vec4 tempNorm = vec4(norms, 0);//Create a temp value to store the normals in
	worldNorm = (tempNorm * worldMatrix).xyz; //Put the normals into world space, and pass it out to worldNorm

	vec4 tempPos = vec4(local_pos, 1); //Create a temp value to store the positions in
	worldPos = (tempPos * worldMatrix).xyz; //Put the positions into world space, and pass it out to worldPos

	gl_Position = tempPos * worldMatrix * viewMatrix * projectionMatrix; //Set gl_Position into projection space
}