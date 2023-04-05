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
	 void DrawSphere();
	 void RenderDynamicCubeMap(int face);
	 void DrawCube();
	 void FillBuffers();
	 Shader* lightShader;
	 Shader* reflectShader;
	 Shader* skyboxShader;
	 Shader* sceneShader;

	 HeightMap* heightMap;
	 Mesh* quad;

	 Light* light;
	 Camera* camera;

	 GLuint cubeMap;
	 GLuint waterTex;
	 GLuint earthTex;
	 GLuint earthBump;
	 GLuint cubeMapBufferTex;
	 GLuint bufferDepthTex;

	 float waterRotate;
	 float waterCycle;

	 Mesh* sphere;
	 Mesh* cube;

	 GLuint cubeMapFBO;
	 GLuint depthbuffer;

 };