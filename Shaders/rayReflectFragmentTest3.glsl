#version 330 core

uniform samplerCube cubeTex;
uniform sampler2D diffuseTex;
uniform mat4 projMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelMatrix;
uniform mat4 cubeModelMatrix;
uniform mat4 cubeModelMatrix2;
uniform mat4 cubeModelMatrix3;
uniform mat4 cubeModelMatrix4;
uniform vec3 cameraPos;
uniform sampler2D heightMapTex;
uniform vec4 lightColour;
uniform vec3 lightPos;
uniform float lightRadius;
in vec3 position;
int objectType;
mat4 temp;
in Vertex {
	vec3 ray_origin;
	vec3 ray_direction;
    vec3 worldPos;
    vec3 normal;
} IN;

out vec4 fragColour;

int DetermineCubeFace(vec3 p, mat4 mm) {
    vec3 faceNormals[6];
    faceNormals[0] = vec3(1.0, 0.0, 0.0);   // Right face
    faceNormals[1] = vec3(-1.0, 0.0, 0.0);  // Left face
    faceNormals[2] = vec3(0.0, 1.0, 0.0);   // Top face
    faceNormals[3] = vec3(0.0, -1.0, 0.0);  // Bottom face
    faceNormals[4] = vec3(0.0, 0.0, 1.0);   // Front face
    faceNormals[5] = vec3(0.0, 0.0, -1.0);  // Back face

    vec3 direction = p - (mm*vec4(vec3(0.0),1.0)).xyz;
    float maxDot = -1.0;
    int faceIndex = -1;

    for (int i = 0; i < 6; i++) {
        float dotProduct = dot(normalize(direction), faceNormals[i]);
        if (dotProduct > maxDot) {
            maxDot = dotProduct;
            faceIndex = i;
        }
    }

    return faceIndex;
}

vec2 getCubeUVFromPoint(vec3 worldPosition, mat4 modelMatrix, vec2 textureDimensions) {
    int temp = DetermineCubeFace(worldPosition,modelMatrix );
    vec4 localPosition = inverse(modelMatrix) * vec4(worldPosition, 1.0);
    vec2 texCoords;
    if(temp == 0 || temp == 1){
    texCoords = localPosition.yz / textureDimensions;
    }
    if(temp == 2 || temp == 3){
    texCoords = localPosition.xy / textureDimensions;
    }
    if(temp == 4 || temp == 5){
    texCoords = localPosition.xz / textureDimensions;
    }
    return texCoords;
}

float SphereSDF(vec3 p, vec3 center, float radius)
{
    vec4 spherePos = vec4(center, 1.0) * modelMatrix;
    vec3 sphereCenter = spherePos.xyz;
    float dist = length(p - sphereCenter) - radius;
    return dist;
}
float CubeSDF(vec3 p, vec3 b, mat4 cubeMM) {
    vec3 q = abs((inverse(cubeMM) * vec4(p, 1.0)).xyz) - b;
    return length(max(q,0.0));
}

float HeightmapSDF(vec3 p) {
    vec2 heightmapSize = textureSize(heightMapTex, 0)*16.0f;
    vec2 texCoord = vec2(p.x / heightmapSize.x, p.z / heightmapSize.y);
    float height = texture(heightMapTex, texCoord).r*255.0f;
    float distance = p.y - height;
    
    return distance;
}

float SceneSDF(vec3 p) {
    float cube = CubeSDF(p, vec3(0.5), cubeModelMatrix)*200;
    float cube2 = CubeSDF(p, vec3(0.5), cubeModelMatrix2)*200;
    float cube3 = CubeSDF(p, vec3(0.5), cubeModelMatrix3)*200;
    float cube4 = CubeSDF(p, vec3(0.5), cubeModelMatrix4)*200;
    float heightmap = HeightmapSDF(p);
    if(cube <heightmap && cube < cube2 && cube < cube3 && cube < cube4){
        objectType = 2;
    	temp = cubeModelMatrix;
        return cube;
    }
    if(heightmap <cube && heightmap < cube2 && heightmap < cube3 && heightmap < cube4){
        objectType = 3;
        return heightmap;
    }
    if(cube2 <heightmap && cube2 < cube && cube2 < cube3 && cube2 < cube4){
	objectType = 2;
	temp = cubeModelMatrix2;
        return cube2;
    }
    if(cube3 <heightmap && cube3 < cube && cube3 < cube2 && cube3 < cube4){
	objectType = 2;
	temp = cubeModelMatrix3;
        return cube3;
    }
    if(cube4 <heightmap && cube4 < cube && cube4 < cube2 && cube4 < cube3){
	objectType = 2;
	temp = cubeModelMatrix4;
        return cube4;
    }
}


vec3 CalcCubeNormal(vec3 p) {
    float eps;

    float d = SceneSDF(p);
    if(objectType == 2){
        eps = 0.001;
    }
    else if(objectType == 3){
        eps = 0.00001;
    }

    vec3 normal;
    normal.x = d - SceneSDF(vec3(p.x - eps, p.y, p.z));
    normal.y = d - SceneSDF(vec3(p.x, p.y - eps, p.z));
    normal.z = d - SceneSDF(vec3(p.x, p.y, p.z - eps));

    normal = normalize(normal);

    return normal;
    
}


vec3 CalculateHMNormal(vec3 p, sampler2D heightmapTex) {
    vec2 heightmapSize = textureSize(heightMapTex, 0)*16.0f;
    float delta = 1.0 / heightmapSize.x;
    vec2 texCoord = vec2(p.x / heightmapSize.x, p.z / heightmapSize.y);
    float heightCenter = texture(heightmapTex, texCoord).r*16.0f;
    float heightLeft = texture(heightmapTex, texCoord - vec2(delta*16.0f, 0.0)).r;
    float heightRight = texture(heightmapTex, texCoord + vec2(delta*16.0f, 0.0)).r;
    float heightDown = texture(heightmapTex, texCoord - vec2(0.0, delta*16.0f)).r;
    float heightUp = texture(heightmapTex, texCoord + vec2(0.0, delta*16.0f)).r;
    
    vec3 normal = vec3(heightLeft - heightRight, 2.0 * delta*255.0f, heightDown - heightUp);
    normal = normalize(normal);
    
    return normal;
}





void main()
{
    int maxSteps = 500;
    const float maxDistance = 1000.0;
    const float epsilon = 0.001;
    const float reflectionStrength = 1.0;
    vec3 p;
    vec3 position = IN.worldPos;
    vec3 reflected = vec3(0.0);
    vec3 color;
    vec2 texSize = textureSize(diffuseTex, 0);
    vec2 heightmapSize = textureSize(heightMapTex, 0)*16.0f;
    vec3 test;
    
    for (int i = 0; i < maxSteps; i++){
        vec3 reflection = reflect(-IN.ray_direction, IN.normal);
        float distance = SceneSDF(IN.worldPos);
        p = position + distance * reflection;
        float surfaceDistance = SceneSDF(p);
        if (surfaceDistance <= epsilon)
        {
            if(objectType == 2){
                vec2 uv = getCubeUVFromPoint(p, temp,texSize);

                vec3 incident = normalize(lightPos - p);
	            vec3 viewDir = reflection;
	            vec3 halfDir = normalize(incident + reflection );
                vec4 diffuse = texture(diffuseTex , uv*200 );
                vec3 tempScale = vec3(200,200,200);
                vec3 tempNormal = CalcCubeNormal(p);
	            float lambert = max(dot(incident , tempNormal), 0.0f);
	            float distance = length(lightPos - p );
	            float attenuation = 1.0 - clamp(distance / lightRadius , 0.0, 1.0);

                float specFactor = clamp(dot(halfDir , tempNormal ) ,0.0 ,1.0);
	            specFactor = pow(specFactor , 60.0 );

                vec3 surface = (diffuse.rgb * lightColour.rgb);
	            color = surface * lambert * attenuation;
	            color += (lightColour.rgb * specFactor )* attenuation *0.33;
	            color += surface * 0.1f; // ambient!
                //reflected = vec3(0.0,0.0,1.0);
                reflected = color* reflectionStrength;
                position += reflection * 0.0001;
                fragColour = vec4(reflected,1.0);
            }
            if(objectType == 3 ){
                if(surfaceDistance<0.0){
                    p += (abs(surfaceDistance)+1.0)* -reflection;
                    surfaceDistance = HeightmapSDF(p);
                }
                if(surfaceDistance < 0.0 ){
                    test = vec3(1.0,0.0,0.0);
                }else{
                    test = vec3(0.0,HeightmapSDF(p),0.0);
                }
                if (p.x >= 0.0 && p.x <= heightmapSize.x && p.z >= 0.0 && p.z <= heightmapSize.y) {

                    vec3 hNormal = CalculateHMNormal(p,heightMapTex);
                    vec3 incident = normalize(lightPos - p );
	                vec3 viewDir = reflection;
	                vec3 halfDir = normalize(incident + reflection );
                    vec3 localPos =  p;
                    vec2 uv = localPos.xz / texSize;
                    vec4 hDiffuse = texture(diffuseTex , uv);

                    float lambert = max(dot(incident , hNormal), 0.0f);
	                float dist = length(lightPos - p );
	                float attenuation = 1.0 - clamp(dist / lightRadius , 0.0, 1.0);

                    float specFactor = clamp(dot(halfDir , hNormal ) ,0.0 ,1.0);
	                specFactor = pow(specFactor , 60.0 );

                    vec3 surface = (hDiffuse.rgb * lightColour.rgb);
                    color = surface * lambert * attenuation;
	                color += (lightColour.rgb * specFactor )* attenuation *0.33;
	                color += surface * 0.2f; // ambient!
                    reflected = color * reflectionStrength;
                    //reflected = hNormal;
                    position += reflection * 0.0001;
                    fragColour = vec4(reflected,1.0);
                }
            }
        }
        else if(surfaceDistance<maxDistance){
	        vec4 reflected = texture(cubeTex ,reflection);
            position += reflection * surfaceDistance;
            fragColour = reflected;
        }
        else if(surfaceDistance>maxDistance){
            vec4 reflected = texture(cubeTex ,reflection);
            fragColour = reflected;
            break;
        }
    }
}    



