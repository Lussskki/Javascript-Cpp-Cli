#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "tinyobjloader/tiny_obj_loader.h"

#include <iostream>
#include <vector>
#include <cfloat>
#include <algorithm>
#include <map>
#include <tuple>

const unsigned int WIDTH = 800;
const unsigned int HEIGHT = 600;

// Vertex shader
const char* vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos,1.0);
    TexCoord = aTexCoord;
}
)";

// Fragment shader
const char* fragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D texture_diffuse;

void main() {
    FragColor = texture(texture_diffuse, TexCoord);
}
)";

void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error: " << description << std::endl;
}

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Textured OBJ Viewer", nullptr, nullptr);
    if(!window){ glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){ std::cerr<<"Failed GLAD\n"; return -1; }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // --- Load OBJ ---
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    bool ret = tinyobj::LoadObj(&attrib,&shapes,&materials,&err,"assets/Cottage_FREE.obj","assets/");
    if(!err.empty()) std::cerr << "TinyOBJLoader: " << err << std::endl;
    if(!ret){ std::cerr << "Failed to load OBJ!\n"; return -1; }

    // --- Deduplicate vertices and fix UVs ---
    struct Vertex {
        glm::vec3 pos;
        glm::vec2 uv;

        bool operator<(const Vertex& other) const {
            return std::tie(pos.x,pos.y,pos.z,uv.x,uv.y) < std::tie(other.pos.x,other.pos.y,other.pos.z,other.uv.x,other.uv.y);
        }
    };

    std::vector<float> vertices; // x,y,z,u,v
    std::vector<unsigned int> indices;
    std::map<Vertex,unsigned int> uniqueVertices;

    for (const auto& shape : shapes) {
        for (const auto& ind : shape.mesh.indices) {
            glm::vec3 position(
                attrib.vertices[3*ind.vertex_index + 0],
                attrib.vertices[3*ind.vertex_index + 1],
                attrib.vertices[3*ind.vertex_index + 2]
            );

            glm::vec2 uv(0.0f, 0.0f);
            if (ind.texcoord_index >= 0) {
                uv.x = attrib.texcoords[2*ind.texcoord_index + 0];
                uv.y = 1.0f - attrib.texcoords[2*ind.texcoord_index + 1]; // FLIP V
            }

            Vertex vert{position, uv};

            if (uniqueVertices.count(vert) == 0) {
                uniqueVertices[vert] = vertices.size()/5;
                vertices.push_back(position.x);
                vertices.push_back(position.y);
                vertices.push_back(position.z);
                vertices.push_back(uv.x);
                vertices.push_back(uv.y);
            }

            indices.push_back(uniqueVertices[vert]);
        }
    }

    std::cout << "Loaded vertices: " << vertices.size()/5 << ", indices: " << indices.size() << "\n";

    // --- Compute center & scale ---
    glm::vec3 minV(FLT_MAX), maxV(-FLT_MAX);
    for(size_t i=0;i<vertices.size();i+=5){
        minV.x = std::min(minV.x, vertices[i+0]);
        minV.y = std::min(minV.y, vertices[i+1]);
        minV.z = std::min(minV.z, vertices[i+2]);
        maxV.x = std::max(maxV.x, vertices[i+0]);
        maxV.y = std::max(maxV.y, vertices[i+1]);
        maxV.z = std::max(maxV.z, vertices[i+2]);
    }

    glm::vec3 center = (minV + maxV) * 0.5f;
    glm::vec3 diff = maxV - minV;
    float maxDim = std::max({diff.x,diff.y,diff.z});
    float scale = 2.0f / maxDim;

    // --- VAO / VBO / EBO ---
    GLuint VAO,VBO,EBO;
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);
    glGenBuffers(1,&EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),vertices.data(),GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(unsigned int),indices.data(),GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // --- Load texture ---
    int texW, texH, texC;
    unsigned char* data = stbi_load("assets/Cottage_Clean/Cottage_Clean_Base_Color.png",&texW,&texH,&texC,0);
    if(!data){ std::cerr<<"Failed to load texture\n"; return -1; }
    GLuint texture;
    glGenTextures(1,&texture);
    glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,(texC==4?GL_RGBA:GL_RGB),texW,texH,0,(texC==4?GL_RGBA:GL_RGB),GL_UNSIGNED_BYTE,data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

    // --- Compile shaders ---
    auto compileShader = [](GLenum type,const char* src){
        GLuint s = glCreateShader(type);
        glShaderSource(s,1,&src,nullptr);
        glCompileShader(s);
        int success; char info[512];
        glGetShaderiv(s,GL_COMPILE_STATUS,&success);
        if(!success){ glGetShaderInfoLog(s,512,NULL,info); std::cerr<<"Shader compile error: "<<info<<"\n"; }
        return s;
    };
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER,vertexShaderSrc);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER,fragmentShaderSrc);
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram,vertexShader);
    glAttachShader(shaderProgram,fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // --- Model, View, Projection ---
    glm::mat4 modelMat = glm::mat4(1.0f);
    modelMat = glm::scale(modelMat, glm::vec3(scale));
    modelMat = glm::translate(modelMat, -center); // center in X,Y,Z

    float camDist = 0.8f / scale;
    glm::mat4 view = glm::lookAt(glm::vec3(0,0,camDist), glm::vec3(0,0,0), glm::vec3(0,1,0));
    glm::mat4 projection = glm::perspective(glm::radians(45.0f),(float)WIDTH/HEIGHT,0.05f, camDist*10.0f);

    // --- Render loop ---
    while(!glfwWindowShouldClose(window)){
        glClearColor(0.1f,0.1f,0.1f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float angle = (float)glfwGetTime() * glm::radians(15.0f);
        glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0,1,0));
        glm::mat4 modelRot = rotation * modelMat;

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"model"),1,GL_FALSE,glm::value_ptr(modelRot));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"view"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"projection"),1,GL_FALSE,glm::value_ptr(projection));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,texture);
        glUniform1i(glGetUniformLocation(shaderProgram,"texture_diffuse"),0);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES,indices.size(),GL_UNSIGNED_INT,0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1,&VAO);
    glDeleteBuffers(1,&VBO);
    glDeleteBuffers(1,&EBO);
    glDeleteProgram(shaderProgram);
    glDeleteTextures(1,&texture);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}