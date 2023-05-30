#version 330 core

uniform mat4 modelMatrix;
uniform mat4 cubeModelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projMatrix;
uniform mat4 textureMatrix;
uniform vec3 cameraPos;

in vec3 position;
in vec3 normal;
in vec2 texCoord;

out Vertex {
	vec3 ray_origin;
	vec3 ray_direction;
	vec3 worldPos;
	vec3 normal;
} OUT;

void main(void) {
	mat3 normalMatrix = transpose(inverse(mat3(modelMatrix )));
	OUT.normal = normalize(normalMatrix * normalize(normal));
	gl_Position = projMatrix * viewMatrix * modelMatrix * vec4(position, 1.0);
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
	OUT.worldPos = worldPos.xyz;
    OUT.ray_origin = cameraPos + normalize(vec3(viewMatrix * vec4(0.0, 0.0, -1.0, 0.0))) * 0.1;
    OUT.ray_direction = normalize(cameraPos - worldPos.xyz );
}
