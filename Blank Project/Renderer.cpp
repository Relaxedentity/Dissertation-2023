#include "Renderer.h"
#include "../nclgl/Light.h"
#include "../nclgl/Camera.h"
#include "../nclgl/HeightMap.h"
#include <algorithm >
#include <ctime>
#include "../nclgl/MeshMaterial.h"
#include "../nclgl/MeshAnimation.h"

const int LIGHT_NUM = 60;
#define SHADOWSIZE 2048
const int POST_PASSES = 5;

Renderer::Renderer(Window& parent) : OGLRenderer(parent) {
	quad = Mesh::GenerateQuad();

	sphere = Mesh::LoadFromMeshFile("Sphere.msh");
	cube = Mesh::LoadFromMeshFile("Cube.msh");

	heightMap = new HeightMap(TEXTUREDIR "noise.png");

	waterTex = SOIL_load_OGL_texture(
		TEXTUREDIR "water.TGA", SOIL_LOAD_AUTO,
		SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);

	earthTex = SOIL_load_OGL_texture(
		TEXTUREDIR "Barren Reds.JPG", SOIL_LOAD_AUTO,
		SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);

	earthBump = SOIL_load_OGL_texture(
		TEXTUREDIR "Barren RedsDOT3.JPG", SOIL_LOAD_AUTO,
		SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);

	cubeMap = SOIL_load_OGL_cubemap(
		TEXTUREDIR "rusted_west.jpg", TEXTUREDIR "rusted_east.jpg",
		TEXTUREDIR "rusted_up.jpg", TEXTUREDIR "rusted_down.jpg",
		TEXTUREDIR "rusted_south.jpg", TEXTUREDIR "rusted_north.jpg",
		SOIL_LOAD_RGB, SOIL_CREATE_NEW_ID, 0);

	if (!earthTex || !earthBump || !cubeMap || !waterTex) {
		return;
	}
	SetTextureRepeating(earthTex, true);
	SetTextureRepeating(earthBump, true);
	SetTextureRepeating(waterTex, true);

	reflectShader = new Shader("reflectVertex.glsl", "reflectFragment.glsl");
	skyboxShader = new Shader("skyboxVertex.glsl", "skyboxFragment.glsl");
	lightShader = new Shader("PerPixelVertex.glsl", "PerPixelFragment.glsl");
	sceneShader = new Shader("TexturedVertex.glsl", "TexturedFragment.glsl");

	if (!reflectShader->LoadSuccess() ||
		!skyboxShader->LoadSuccess() ||
		!lightShader->LoadSuccess() ||
		!sceneShader->LoadSuccess()) {
		return;
	}

	Vector3 heightmapSize = heightMap->GetHeightmapSize();

	camera = new Camera(-45.0f, 0.0f, heightmapSize * Vector3(0.5f, 5.0f, 0.5f));
	light = new Light(heightmapSize * Vector3(0.5f, 1.5f, 0.5f), Vector4(1, 1, 1, 1), heightmapSize.x);

	projMatrix = Matrix4::Perspective(1.0f, 15000.0f, (float)width / (float)height, 45.0f);

	//modified cubemap tex
	glGenTextures(1, &cubeMapBufferTex);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapBufferTex);
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height,
	//	0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	for (int i = 0; i < 6; ++i) {
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, width, width, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	}
	//depth tex
	glGenTextures(1, &bufferDepthTex);
	glBindTexture(GL_TEXTURE_2D, bufferDepthTex);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height,
		0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);



	//frame buffer generation
	glGenFramebuffers(1, &cubeMapFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, cubeMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
		GL_TEXTURE_2D, bufferDepthTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
		GL_TEXTURE_2D, bufferDepthTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_CUBE_MAP_POSITIVE_X, cubeMapBufferTex, 0);
	//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	//	GL_TEXTURE_CUBE_MAP, cubeMapBufferTex, 0);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
		GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "complete" << std::endl;
	}

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
		GL_FRAMEBUFFER_COMPLETE || !bufferDepthTex || !cubeMapBufferTex) {
		return;
	}
	
	//depth buffer generation
	//glGenRenderbuffers(1, &depthbuffer);
	//glBindRenderbuffer(GL_RENDERBUFFER, depthbuffer);
	//glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
	//glBindRenderbuffer(GL_RENDERBUFFER, 0);

	//glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, cubeMapFBO);


	

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	waterRotate = 0.0f;
	waterCycle = 0.0f;
	init = true;

}

Renderer ::~Renderer(void) {
	delete camera;
	delete heightMap;
	delete quad;
	delete reflectShader;
	delete skyboxShader;
	delete lightShader;
	delete light;
}

void Renderer::UpdateScene(float dt) {
	camera->UpdateCamera(dt);
	viewMatrix = camera->BuildViewMatrix();
	waterRotate += dt * 2.0f;
	waterCycle += dt * 0.25f;
}

void Renderer::RenderScene() {
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	FillBuffers();
	viewMatrix = camera->BuildViewMatrix();
	projMatrix = Matrix4::Perspective(1.0f, 15000.0f, (float)width / (float)height, 45.0f);
	DrawSkybox();
	DrawHeightmap();
	DrawCube();
	DrawSphere();
	//DrawWater();
}

void Renderer::DrawSkybox() {
	glDepthMask(GL_FALSE);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);
	BindShader(skyboxShader);
	UpdateShaderMatrices();

	quad->Draw();
	
	glDepthMask(GL_TRUE);
}

void Renderer::DrawHeightmap() {
	BindShader(lightShader);
	SetShaderLight(*light);
	glUniform3fv(glGetUniformLocation(lightShader->GetProgram(),
		"cameraPos"), 1, (float*)&camera->GetPosition());

	glUniform1i(glGetUniformLocation(lightShader->GetProgram(), "diffuseTex"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, earthTex);

	glUniform1i(glGetUniformLocation(lightShader->GetProgram(), "bumpTex"), 1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, earthBump);

	modelMatrix.ToIdentity();
	textureMatrix.ToIdentity();

	UpdateShaderMatrices();

	heightMap->Draw();
}

void Renderer::DrawWater() {
	BindShader(reflectShader);

	glUniform3fv(glGetUniformLocation(reflectShader->GetProgram(), "cameraPos"), 1, (float*)&camera->GetPosition());
	glUniform1i(glGetUniformLocation(reflectShader->GetProgram(), "diffuseTex"), 0);
	glUniform1i(glGetUniformLocation(reflectShader->GetProgram(), "cubeTex"), 2);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, waterTex);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);

	Vector3 hSize = heightMap->GetHeightmapSize();

	modelMatrix =
		Matrix4::Translation(hSize * 0.5f) *
		Matrix4::Scale(hSize * 0.5f) *
		Matrix4::Rotation(90, Vector3(1, 0, 0));

	textureMatrix =
		Matrix4::Translation(Vector3(waterCycle, 0.0f, waterCycle)) *
		Matrix4::Scale(Vector3(10, 10, 10)) *
		Matrix4::Rotation(waterRotate, Vector3(0, 0, 1));

	UpdateShaderMatrices();
	quad->Draw();
}

void Renderer::DrawCube() {
	BindShader(lightShader);
	SetShaderLight(*light);

	glUniform3fv(glGetUniformLocation(lightShader->GetProgram(),
		"cameraPos"), 1, (float*)&camera->GetPosition());

	glUniform1i(glGetUniformLocation(lightShader->GetProgram(), "diffuseTex"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, earthTex);

	glUniform1i(glGetUniformLocation(lightShader->GetProgram(), "bumpTex"), 1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, earthBump);

	Vector3 hSize = heightMap->GetHeightmapSize();
	modelMatrix =
		Matrix4::Translation(hSize * 0.5f* Vector3(1, 5, 1.5)) *
		Matrix4::Scale(Vector3(200.0f, 200.0f, 200.0f)) *
		Matrix4::Rotation(90, Vector3(1, 0, 0));
	UpdateShaderMatrices();

	cube->Draw();

}


void Renderer::DrawSphere() {
	BindShader(reflectShader);

	glUniform3fv(glGetUniformLocation(reflectShader->GetProgram(), "cameraPos"), 1, (float*)&camera->GetPosition());
	glUniform1i(glGetUniformLocation(reflectShader->GetProgram(), "diffuseTex"), 0);
	glUniform1i(glGetUniformLocation(reflectShader->GetProgram(), "cubeTex"), 2);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, waterTex);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapBufferTex);

	Vector3 hSize = heightMap->GetHeightmapSize();

	modelMatrix =
		Matrix4::Translation(hSize * 0.5f * Vector3(1,5,1)) *
		Matrix4::Scale(Vector3(200.0f, 200.0f, 200.0f)) *
		Matrix4::Rotation(90, Vector3(1, 0, 0));

	//textureMatrix =
	//	Matrix4::Translation(Vector3(waterCycle, 0.0f, waterCycle)) *
	//	Matrix4::Scale(Vector3(10, 10, 10)) *
	//	Matrix4::Rotation(waterRotate, Vector3(0, 0, 1));
	UpdateShaderMatrices();
	sphere->Draw();
}

void Renderer::RenderDynamicCubeMap(int face) {

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + (int)face, cubeMapBufferTex, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	BindShader(sceneShader);
	projMatrix = Matrix4::Perspective(1.0f, 1000.0f, (float)width / (float)width, 90.0f);
	Vector3 hSize = heightMap->GetHeightmapSize();
	Vector3 temp = hSize * 0.5f * Vector3(1, 5, 1);
	//std::cout << GL_TEXTURE_CUBE_MAP_POSITIVE_X + (int)face << std::endl;
	switch (GL_TEXTURE_CUBE_MAP_POSITIVE_X + (int)face)
	{
	case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
		viewMatrix = Matrix4::BuildViewMatrix(temp, temp* Vector3(10,0,0), Vector3(0,-1,0));
		//gluLookAt(0, 0, 0, 1, 0, 0, 0, 1, 0);
		break;

	case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
		viewMatrix = Matrix4::BuildViewMatrix(temp, temp* Vector3(-10,0,0), Vector3(0, -1, 0));
		//gluLookAt(0, 0, 0, -1, 0, 0, 0, 1, 0);
		break;

	case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
		viewMatrix = Matrix4::BuildViewMatrix(temp, temp* Vector3(0,10,0), Vector3(0, 0, 1));
		//gluLookAt(0, 0, 0, 0, 10, 0, 1, 0, 0);
		break;

	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
		viewMatrix = Matrix4::BuildViewMatrix(temp, temp* Vector3(0,-10,0), Vector3(0, 0, -1));
		//gluLookAt(0, 0, 0, 0, -10, 0, 1, 0, 0);
		break;

	case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
		viewMatrix = Matrix4::BuildViewMatrix(temp, temp* Vector3(0,0,10), Vector3(0, -1, 0));
		//gluLookAt(0, 0, 0, 0, 0, 10, 0, 1, 0);
		break;

	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
		viewMatrix = Matrix4::BuildViewMatrix(temp, temp* Vector3(0,0,-10), Vector3(0, -1, 0));
		//gluLookAt(0, 0, 0, 0, 0, -10, 0, 1, 0);
		break;

	default:
		break;
	}
	UpdateShaderMatrices();
	//glTranslatef(-renderPosition.x, -renderPosition.y, -renderPosition.z);
	//glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
}
void Renderer::FillBuffers() {
	glBindFramebuffer(GL_FRAMEBUFFER, cubeMapFBO);
	for (int i = 0; i < 6; i++) {
		RenderDynamicCubeMap(i);
		DrawSkybox();
		DrawHeightmap();
		DrawCube();
	}

	//modelMatrix.ToIdentity();
	//viewMatrix.ToIdentity();
	//UpdateShaderMatrices();

	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
}
