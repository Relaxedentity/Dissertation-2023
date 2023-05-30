 #pragma once
 #include "../nclgl/OGLRenderer.h"
 class Camera;
 class Shader;
 class HeightMap;

 class Renderer : public OGLRenderer {
 public:
	 Renderer(Window& parent);
	 ~Renderer(void);
	 void RenderScene() override;
	 void UpdateScene(float dt) override;

 protected:
	 void DrawHeightmap();
	 void DrawWater();
	 void DrawSkybox();
	 void DrawSphere(GLuint &cBufferTex);
	 void DrawRayTracedSphere(GLuint& cBufferTex);
	 void DrawRayTracedSphereTest(GLuint& cBufferTex);
	 void RenderDynamicCubeMap(int face, GLuint &cBufferTex);
	 void FillRayBuffers(GLuint& cBufferTex);
	 void DrawCube(Vector3 translation);
	 void FillBuffers(GLuint& cBufferTex);
	 void SceneQuad();
	 void DrawScene();
	 Shader* lightShader;
	 Shader* reflectShader;
	 Shader* skyboxShader;
	 Shader* sceneShader;
	 Shader* rayReflectShader;
	 Shader* rayReflectShaderTest;
	 Shader* rayReflectShaderTest2;
	 Shader* rayReflectShaderTest3;
	 HeightMap* heightMap;
	 Mesh* quad;

	 Light* light;
	 Camera* camera;

	 GLuint cubeMap;
	 GLuint heightMapTex;
	 GLuint waterTex;
	 GLuint earthTex;
	 GLuint earthBump;
	 GLuint cubeMapBufferTex[2];
	 GLuint rayCastTex;
	 GLuint rayDepthTex;
	 GLuint bufferDepthTex[2];

	 float waterRotate;
	 float waterCycle;

	 Mesh* sphere;
	 Mesh* cube;

	 GLuint cubeMapFBO;
	 GLuint rayCastFBO;

	 int texbuffer;
	 bool renderMode;
	 bool reflectionMode;
	 Vector3 hsize;
	 float movement;
	 bool reverse;
 };