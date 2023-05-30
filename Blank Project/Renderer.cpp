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

	heightMapTex = SOIL_load_OGL_texture(
		TEXTUREDIR "noise.png", SOIL_LOAD_AUTO,
		SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);
	glTexParameteri(heightMapTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(heightMapTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

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

	rayReflectShader = new Shader("rayReflectVertex.glsl", "rayReflectFragment.glsl");
	rayReflectShaderTest = new Shader("rayReflectVertexTest.glsl", "rayReflectFragmentTest.glsl");
	rayReflectShaderTest2 = new Shader("rayReflectVertexTest2.glsl", "rayReflectFragmentTest2.glsl");
	rayReflectShaderTest3 = new Shader("rayReflectVertexTest3.glsl", "rayReflectFragmentTest3.glsl");
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

	hsize = heightMap->GetHeightmapSize();
	std::cout << hsize << std::endl;

	camera = new Camera(-45.0f, 0.0f, hsize * Vector3(0.5f, 5.0f, 0.6f));
	light = new Light(hsize * Vector3(0.5f, 1.5f, 0.5f), Vector4(1, 1, 1, 1), hsize.x);

	projMatrix = Matrix4::Perspective(1.0f, 15000.0f, (float)width / (float)height, 45.0f);

	//modified cubemap tex
	for (int i = 0; i < 2; i++) {
		glGenTextures(1, &cubeMapBufferTex[i]);
		glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapBufferTex[i]);
		glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		for (int i = 0; i < 6; i++) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB8, 1024, 1024, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
		}
		//depth tex
		glGenTextures(1, &bufferDepthTex[i]);
		glBindTexture(GL_TEXTURE_2D, bufferDepthTex[i]);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, 1024, 1024,
			0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	}
	glGenTextures(1, &rayCastTex);
	glBindTexture(GL_TEXTURE_2D, rayCastTex);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height,
		0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	glGenTextures(1, &rayDepthTex);
	glBindTexture(GL_TEXTURE_2D, rayDepthTex);
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
		GL_TEXTURE_2D, bufferDepthTex[0], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
		GL_TEXTURE_2D, bufferDepthTex[0], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_CUBE_MAP_POSITIVE_X, cubeMapBufferTex[0], 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
		GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "complete" << std::endl;
	}
	glGenFramebuffers(1, &rayCastFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, rayCastFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
		GL_TEXTURE_2D, rayDepthTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
		GL_TEXTURE_2D, rayDepthTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, rayCastTex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
		GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "complete" << std::endl;
	}

	//if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
	//	GL_FRAMEBUFFER_COMPLETE || !bufferDepthTex || !cubeMapBufferTex[0]) {
	//	return;
	//}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	waterRotate = 0.0f;
	waterCycle = 0.0f;
	movement = 0.0f;
	texbuffer = 0;
	renderMode = true;
	reverse = false;
	init = true;

}

Renderer ::~Renderer(void) {
	delete camera;
	delete heightMap;
	delete quad;
	delete reflectShader;
	delete rayReflectShader;
	delete sceneShader;
	delete skyboxShader;
	delete lightShader;
	delete light;
}

void Renderer::UpdateScene(float dt) {
	camera->UpdateCamera(dt);
	viewMatrix = camera->BuildViewMatrix();
	waterRotate += dt * 2.0f;
	waterCycle += dt * 0.25f;
	if (Window::GetKeyboard()->KeyTriggered(KEYBOARD_Q)) {
		renderMode = !renderMode;
	}
	
	if (!reverse) {
		movement += dt * 0.5f;
		if (movement > 3.0f) {
			reverse = true;
		}
	}
	else {
		movement -= dt * 0.5f;
		if (movement < 0.0f) {
			reverse = false;
		}
	}

}

void Renderer::RenderScene() {
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	if (renderMode) {
		FillBuffers(cubeMapBufferTex[0]);
		viewMatrix = camera->BuildViewMatrix();
		projMatrix = Matrix4::Perspective(1.0f, 15000.0f, (float)width / (float)height, 45.0f);
		DrawSkybox();
		DrawHeightmap();
		Vector3 translation = hsize * 0.5f * Vector3(movement, 7, 1.5);
		DrawCube(translation);
		DrawSphere(cubeMapBufferTex[0]);
	}
	else {
		DrawSkybox();
		DrawHeightmap();
		Vector3 translation = hsize * 0.5f * Vector3(movement, 7, 1.5);
		DrawCube(translation);
		Vector3 translation2 = hsize * 0.5f * Vector3(movement, 7, 0.5);
		DrawCube(translation2);
		Vector3 translation3 = hsize * 0.5f * Vector3(1.0, 8, movement);
		DrawCube(translation3);
		Vector3 translation4 = hsize * 0.5f * Vector3(1.5, 9, movement);
		DrawCube(translation4);
		DrawRayTracedSphereTest(cubeMapBufferTex[1]);
	}
	
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


	modelMatrix =
		Matrix4::Translation(hsize * 0.5f) *
		Matrix4::Scale(hsize * 0.5f) *
		Matrix4::Rotation(90, Vector3(1, 0, 0));

	textureMatrix =
		Matrix4::Translation(Vector3(waterCycle, 0.0f, waterCycle)) *
		Matrix4::Scale(Vector3(10, 10, 10)) *
		Matrix4::Rotation(waterRotate, Vector3(0, 0, 1));

	UpdateShaderMatrices();
	quad->Draw();
}

void Renderer::DrawCube(Vector3 translation) {
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

	modelMatrix =
		Matrix4::Translation(translation) *
		Matrix4::Scale(Vector3(200.0f, 200.0f, 200.0f)) *
		Matrix4::Rotation(90, Vector3(1, 0, 0));
	UpdateShaderMatrices();

	cube->Draw();

}


void Renderer::DrawSphere(GLuint &cBufferTex) {
	BindShader(reflectShader);

	glUniform3fv(glGetUniformLocation(reflectShader->GetProgram(), "cameraPos"), 1, (float*)&camera->GetPosition());
	glUniform1i(glGetUniformLocation(reflectShader->GetProgram(), "diffuseTex"), 0);
	glUniform1i(glGetUniformLocation(reflectShader->GetProgram(), "cubeTex"), 2);
	//glUniformMatrix4fv(glGetUniformLocation(reflectShader->GetProgram(), "viewMatrix"), 1, false, viewMatrix.values);
	//glUniformMatrix4fv(glGetUniformLocation(reflectShader->GetProgram(), "projectionMatrix"), 1, false, projMatrix.values);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, waterTex);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cBufferTex);


	modelMatrix =
		Matrix4::Translation(hsize * 0.5f * Vector3(1,5,1)) *
		Matrix4::Scale(Vector3(200.0f, 200.0f, 200.0f)) *
		Matrix4::Rotation(90, Vector3(1, 0, 0));

	UpdateShaderMatrices();
	sphere->Draw();
}

void Renderer::DrawRayTracedSphere(GLuint& cBufferTex) {

	BindShader(rayReflectShaderTest3);
	SetShaderLight(*light);

	modelMatrix =
		Matrix4::Translation(hsize * 0.5f * Vector3(1, 5, 1)) *
		Matrix4::Scale(Vector3(200.0f, 200.0f, 200.0f)) *
		Matrix4::Rotation(90, Vector3(1, 0, 0));

	cubeModelMatrix =
		Matrix4::Translation(hsize * 0.5f * Vector3(movement, 7, 1.5)) *
		Matrix4::Scale(Vector3(200.0f, 200.0f, 200.0f)) *
		Matrix4::Rotation(90, Vector3(1, 0, 0));

	glUniform3fv(glGetUniformLocation(rayReflectShaderTest3->GetProgram(), "cameraPos"), 1, (float*)&camera->GetPosition());
	glUniform1i(glGetUniformLocation(rayReflectShaderTest3->GetProgram(), "diffuseTex"), 1);
	glUniform1i(glGetUniformLocation(rayReflectShaderTest3->GetProgram(), "cubeTex"), 2);
	glUniform1i(glGetUniformLocation(rayReflectShaderTest3->GetProgram(), "heightmapTex"), 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, heightMapTex);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, earthTex);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);





	UpdateRayTraceShaderMatrices();
	sphere->Draw();
	

}
void Renderer::DrawRayTracedSphereTest(GLuint& cBufferTex) {

	BindShader(rayReflectShaderTest3);
	SetShaderLight(*light);

	modelMatrix =
		Matrix4::Translation(hsize * 0.5f * Vector3(1, 5, 1)) *
		Matrix4::Scale(Vector3(200.0f, 200.0f, 200.0f)) *
		Matrix4::Rotation(90, Vector3(1, 0, 0));

	cubeModelMatrix =
		Matrix4::Translation(hsize * 0.5f * Vector3(movement, 7, 1.5)) *
		Matrix4::Scale(Vector3(200.0f, 200.0f, 200.0f)) *
		Matrix4::Rotation(90, Vector3(1, 0, 0));

	cubeModelMatrix2 =
		Matrix4::Translation(hsize * 0.5f * Vector3(movement, 7, 0.5)) *
		Matrix4::Scale(Vector3(200.0f, 200.0f, 200.0f)) *
		Matrix4::Rotation(90, Vector3(1, 0, 0));
	
	cubeModelMatrix3 =
		Matrix4::Translation(hsize * 0.5f * Vector3(1.0, 8, movement)) *
		Matrix4::Scale(Vector3(200.0f, 200.0f, 200.0f)) *
		Matrix4::Rotation(90, Vector3(1, 0, 0));
	
	cubeModelMatrix4 =
		Matrix4::Translation(hsize * 0.5f * Vector3(1.5, 9, movement)) *
		Matrix4::Scale(Vector3(200.0f, 200.0f, 200.0f)) *
		Matrix4::Rotation(90, Vector3(1, 0, 0));


	glUniform3fv(glGetUniformLocation(rayReflectShaderTest3->GetProgram(), "cameraPos"), 1, (float*)&camera->GetPosition());
	glUniform1i(glGetUniformLocation(rayReflectShaderTest3->GetProgram(), "diffuseTex"), 1);
	glUniform1i(glGetUniformLocation(rayReflectShaderTest3->GetProgram(), "cubeTex"), 2);
	glUniform1i(glGetUniformLocation(rayReflectShaderTest3->GetProgram(), "heightmapTex"), 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, heightMapTex);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, earthTex);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);



	UpdateRayTraceShaderMatrices();
	sphere->Draw();


}
void Renderer::DrawScene() {
	glBindFramebuffer(GL_FRAMEBUFFER, rayCastFBO);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	viewMatrix = camera->BuildViewMatrix();
	projMatrix = Matrix4::Perspective(1.0f, 15000.0f, (float)width / (float)height, 45.0f);
	DrawSkybox();
	DrawHeightmap();
	Vector3 translation = hsize * 0.5f * Vector3(movement, 7, 1.5);
	DrawCube(translation);
	DrawCube(Vector3(0,0,0));
	DrawRayTracedSphere(cubeMapBufferTex[0]);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::SceneQuad() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	BindShader(sceneShader);
	modelMatrix.ToIdentity();
	viewMatrix.ToIdentity();
	projMatrix.ToIdentity();
	UpdateShaderMatrices();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rayCastTex);
	glUniform1i(glGetUniformLocation(sceneShader->GetProgram(), "diffuseTex"), 0);
	quad->Draw();
}

void Renderer::RenderDynamicCubeMap(int face, GLuint &cBufferTex) {

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + (int)face, cBufferTex, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	BindShader(sceneShader);
	projMatrix = Matrix4::Perspective(1.0f, 5000.0f, 1, 90.0f);
	Vector3 hSize = heightMap->GetHeightmapSize();
	Vector3 temp = hSize * 0.5f * Vector3(1, 5, 1);
	glViewport(0, 0, 1024, 1024);
	//std::cout << GL_TEXTURE_CUBE_MAP_POSITIVE_X + (int)face << std::endl;
	switch (GL_TEXTURE_CUBE_MAP_POSITIVE_X + (int)face)
	{
	case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
		glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
		//viewMatrix = Matrix4::BuildViewMatrix(temp, temp* Vector3(10,0,0), Vector3(0,-1,0));
		viewMatrix = Matrix4::Rotation(-180, Vector3(1, 0, 0)) *
			Matrix4::Rotation(-90, Vector3(0, 1, 0)) *
			Matrix4::Translation(-temp);
		break;

	case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
		glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
		//viewMatrix = Matrix4::BuildViewMatrix(temp, temp* Vector3(-10,0,0), Vector3(0, -1, 0));
		viewMatrix = Matrix4::Rotation(-180, Vector3(1, 0, 0)) *
			Matrix4::Rotation(90, Vector3(0, 1, 0)) *
			Matrix4::Translation(-temp);
		break;

	case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
		glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
		//viewMatrix = Matrix4::BuildViewMatrix(temp, temp* Vector3(0,10,0), Vector3(0, 0, 1));
		viewMatrix = Matrix4::Rotation(-90, Vector3(1, 0, 0)) *
			Matrix4::Rotation(0, Vector3(0, 1, 0)) *
			Matrix4::Translation(-temp);
		break;

	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
		glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
		viewMatrix = Matrix4::BuildViewMatrix(temp, temp* Vector3(0,-10,0), Vector3(0, 0, -1));
		viewMatrix = Matrix4::Rotation(90, Vector3(1, 0, 0)) *
			Matrix4::Rotation(0, Vector3(0, 1, 0)) *
			Matrix4::Translation(-temp);
		break;

	case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
		glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
		//viewMatrix = Matrix4::BuildViewMatrix(temp, temp* Vector3(0,0,10), Vector3(0, -1, 0));
		viewMatrix = Matrix4::Rotation(-180, Vector3(1, 0, 0)) *
			Matrix4::Rotation(0, Vector3(0, 1, 0)) *
			Matrix4::Translation(-temp);
		break;

	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
		glClearColor(0.0f, 1.0f, 1.0f, 1.0f);
		//viewMatrix = Matrix4::BuildViewMatrix(temp, temp* Vector3(0,0,-10), Vector3(0, -1, 0));
		viewMatrix = Matrix4::Rotation(-180, Vector3(1, 0, 0)) *
			Matrix4::Rotation(-180, Vector3(0, 1, 0)) *
			Matrix4::Translation(-temp);
		break;

	default:
		break;
	}
	UpdateShaderMatrices();
	
	//glTranslatef(-renderPosition.x, -renderPosition.y, -renderPosition.z);
	//glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
}
void Renderer::FillRayBuffers(GLuint& cBufferTex) {
	glBindFramebuffer(GL_FRAMEBUFFER, rayCastFBO);
	for (int i = 0; i < 6; i++) {
		RenderDynamicCubeMap(i, cBufferTex);
		DrawSkybox();
		DrawHeightmap();
		Vector3 translation = hsize * 0.5f * Vector3(movement, 7, 1.5);
		DrawCube(translation);
	}
	glViewport(0, 0, width, height);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::FillBuffers(GLuint& cBufferTex) {
	glBindFramebuffer(GL_FRAMEBUFFER, cubeMapFBO);
	
	for (int i = 0; i < 6; i++) {
		RenderDynamicCubeMap(i, cBufferTex);
		DrawSkybox();
		DrawHeightmap();
		Vector3 translation = hsize * 0.5f * Vector3(movement, 7, 1.5);
		DrawCube(translation);
	}

	//modelMatrix.ToIdentity();
	//viewMatrix.ToIdentity();
	//UpdateShaderMatrices();

	glViewport(0, 0, width, height);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
}
