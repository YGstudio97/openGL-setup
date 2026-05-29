// 3D Rotating Cube with OpenGL + GLFW + GLAD
// Save as: main.cpp
// Compile: g++ main.cpp -o cube -lglfw -lGL -ldl
// (or on macOS: g++ main.cpp -o cube -lglfw -framework OpenGL -ldl)

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>

// ---------------------------------------------------------------------------
// Vertex Shader – passes position, color, and normal to fragment shader
// ---------------------------------------------------------------------------
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;

out vec3 ourColor;
out vec3 Normal;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    FragPos     = vec3(model * vec4(aPos, 1.0));
    Normal      = mat3(transpose(inverse(model))) * aNormal;
    ourColor    = aColor;
}
)";

// ---------------------------------------------------------------------------
// Fragment Shader – simple diffuse + ambient lighting
// ---------------------------------------------------------------------------
const char* fragmentShaderSource = R"(
#version 330 core
in vec3 ourColor;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;

void main()
{
    // Ambient
    float ambientStrength = 0.35;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse
    vec3 norm     = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff    = max(dot(norm, lightDir), 0.0);
    vec3 diffuse  = diff * lightColor;

    vec3 result = (ambient + diffuse) * ourColor;
    FragColor   = vec4(result, 1.0);
}
)";

// ---------------------------------------------------------------------------
// Minimal mat4 helpers (column-major, OpenGL convention)
// ---------------------------------------------------------------------------
struct Mat4 {
    float m[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    float& operator[](int i) { return m[i]; }
    const float& operator[](int i) const { return m[i]; }
};

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row) {
            r.m[col*4+row] = 0;
            for (int k = 0; k < 4; ++k)
                r.m[col*4+row] += a.m[k*4+row] * b.m[col*4+k];
        }
    return r;
}

Mat4 perspective(float fovY, float aspect, float zNear, float zFar) {
    Mat4 r = {};
    float tanHalf = std::tan(fovY / 2.0f);
    r.m[0]  =  1.0f / (aspect * tanHalf);
    r.m[5]  =  1.0f / tanHalf;
    r.m[10] = -(zFar + zNear) / (zFar - zNear);
    r.m[11] = -1.0f;
    r.m[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
    return r;
}

Mat4 lookAt(float ex, float ey, float ez,
            float cx, float cy, float cz) {
    // forward
    float fx = cx-ex, fy = cy-ey, fz = cz-ez;
    float fl = std::sqrt(fx*fx+fy*fy+fz*fz);
    fx/=fl; fy/=fl; fz/=fl;
    // right = forward x up(0,1,0)
    float rx = fy*0 - fz*1, ry = fz*0 - fx*0, rz = fx*1 - fy*0;
    float rl = std::sqrt(rx*rx+ry*ry+rz*rz);
    rx/=rl; ry/=rl; rz/=rl;
    // up = right x forward
    float ux = ry*fz - rz*fy, uy = rz*fx - rx*fz, uz = rx*fy - ry*fx;

    Mat4 v = {};
    v.m[0]=rx; v.m[4]=ry; v.m[8] =rz;
    v.m[1]=ux; v.m[5]=uy; v.m[9] =uz;
    v.m[2]=-fx;v.m[6]=-fy;v.m[10]=-fz;
    v.m[12]=-(rx*ex+ry*ey+rz*ez);
    v.m[13]=-(ux*ex+uy*ey+uz*ez);
    v.m[14]= (fx*ex+fy*ey+fz*ez);
    v.m[15]=1;
    return v;
}

Mat4 rotateX(float a) {
    Mat4 r;
    r.m[5]  = std::cos(a); r.m[9]  = -std::sin(a);
    r.m[6]  = std::sin(a); r.m[10] =  std::cos(a);
    return r;
}
Mat4 rotateY(float a) {
    Mat4 r;
    r.m[0]  = std::cos(a); r.m[8]  =  std::sin(a);
    r.m[2]  =-std::sin(a); r.m[10] =  std::cos(a);
    return r;
}

// ---------------------------------------------------------------------------
// Cube geometry: 6 faces × 2 triangles × 3 vertices
// Each vertex: position(3) + color(3) + normal(3) = 9 floats
// ---------------------------------------------------------------------------
//  Face colors: +X red, -X cyan, +Y green, -Y magenta, +Z blue, -Z yellow
static const float cubeVertices[] = {
    // +X face (right)  normal = (1,0,0)
     0.5f, -0.5f, -0.5f,  1.0f,0.2f,0.2f,  1,0,0,
     0.5f,  0.5f, -0.5f,  1.0f,0.2f,0.2f,  1,0,0,
     0.5f,  0.5f,  0.5f,  1.0f,0.2f,0.2f,  1,0,0,
     0.5f,  0.5f,  0.5f,  1.0f,0.2f,0.2f,  1,0,0,
     0.5f, -0.5f,  0.5f,  1.0f,0.2f,0.2f,  1,0,0,
     0.5f, -0.5f, -0.5f,  1.0f,0.2f,0.2f,  1,0,0,
    // -X face (left)   normal = (-1,0,0)
    -0.5f, -0.5f,  0.5f,  0.2f,1.0f,1.0f, -1,0,0,
    -0.5f,  0.5f,  0.5f,  0.2f,1.0f,1.0f, -1,0,0,
    -0.5f,  0.5f, -0.5f,  0.2f,1.0f,1.0f, -1,0,0,
    -0.5f,  0.5f, -0.5f,  0.2f,1.0f,1.0f, -1,0,0,
    -0.5f, -0.5f, -0.5f,  0.2f,1.0f,1.0f, -1,0,0,
    -0.5f, -0.5f,  0.5f,  0.2f,1.0f,1.0f, -1,0,0,
    // +Y face (top)    normal = (0,1,0)
    -0.5f,  0.5f, -0.5f,  0.2f,1.0f,0.2f,  0,1,0,
     0.5f,  0.5f, -0.5f,  0.2f,1.0f,0.2f,  0,1,0,
     0.5f,  0.5f,  0.5f,  0.2f,1.0f,0.2f,  0,1,0,
     0.5f,  0.5f,  0.5f,  0.2f,1.0f,0.2f,  0,1,0,
    -0.5f,  0.5f,  0.5f,  0.2f,1.0f,0.2f,  0,1,0,
    -0.5f,  0.5f, -0.5f,  0.2f,1.0f,0.2f,  0,1,0,
    // -Y face (bottom) normal = (0,-1,0)
    -0.5f, -0.5f,  0.5f,  1.0f,0.2f,1.0f,  0,-1,0,
     0.5f, -0.5f,  0.5f,  1.0f,0.2f,1.0f,  0,-1,0,
     0.5f, -0.5f, -0.5f,  1.0f,0.2f,1.0f,  0,-1,0,
     0.5f, -0.5f, -0.5f,  1.0f,0.2f,1.0f,  0,-1,0,
    -0.5f, -0.5f, -0.5f,  1.0f,0.2f,1.0f,  0,-1,0,
    -0.5f, -0.5f,  0.5f,  1.0f,0.2f,1.0f,  0,-1,0,
    // +Z face (front)  normal = (0,0,1)
    -0.5f, -0.5f,  0.5f,  0.2f,0.4f,1.0f,  0,0,1,
     0.5f, -0.5f,  0.5f,  0.2f,0.4f,1.0f,  0,0,1,
     0.5f,  0.5f,  0.5f,  0.2f,0.4f,1.0f,  0,0,1,
     0.5f,  0.5f,  0.5f,  0.2f,0.4f,1.0f,  0,0,1,
    -0.5f,  0.5f,  0.5f,  0.2f,0.4f,1.0f,  0,0,1,
    -0.5f, -0.5f,  0.5f,  0.2f,0.4f,1.0f,  0,0,1,
    // -Z face (back)   normal = (0,0,-1)
     0.5f, -0.5f, -0.5f,  1.0f,1.0f,0.2f,  0,0,-1,
    -0.5f, -0.5f, -0.5f,  1.0f,1.0f,0.2f,  0,0,-1,
    -0.5f,  0.5f, -0.5f,  1.0f,1.0f,0.2f,  0,0,-1,
    -0.5f,  0.5f, -0.5f,  1.0f,1.0f,0.2f,  0,0,-1,
     0.5f,  0.5f, -0.5f,  1.0f,1.0f,0.2f,  0,0,-1,
     0.5f, -0.5f, -0.5f,  1.0f,1.0f,0.2f,  0,0,-1,
};

// ---------------------------------------------------------------------------
// Shader compilation helpers
// ---------------------------------------------------------------------------
unsigned int compileShader(GLenum type, const char* src) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        std::cerr << "Shader compile error:\n" << log << "\n";
    }
    return shader;
}

unsigned int createProgram(const char* vs, const char* fs) {
    unsigned int v = compileShader(GL_VERTEX_SHADER,   vs);
    unsigned int f = compileShader(GL_FRAGMENT_SHADER, fs);
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    int success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(prog, 512, NULL, log);
        std::cerr << "Program link error:\n" << log << "\n";
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return prog;
}

// ---------------------------------------------------------------------------
// Globals for interactive rotation
// ---------------------------------------------------------------------------
float rotX = 0.3f;
float rotY = 0.5f;
bool  autoSpin = true;

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        const float step = 0.05f;
        if (key == GLFW_KEY_ESCAPE)   glfwSetWindowShouldClose(window, true);
        if (key == GLFW_KEY_UP)       rotX -= step;
        if (key == GLFW_KEY_DOWN)     rotX += step;
        if (key == GLFW_KEY_LEFT)     rotY -= step;
        if (key == GLFW_KEY_RIGHT)    rotY += step;
        if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
            autoSpin = !autoSpin;
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    // GLFW init
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4); // MSAA

    GLFWwindow* window = glfwCreateWindow(800, 600, "3D Cube — Arrow keys rotate | Space toggles spin", NULL, NULL);
    if (!window) { std::cerr << "Window creation failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);

    // GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD init failed\n"; return -1;
    }

    glViewport(0, 0, 800, 600);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    // Build shader program
    unsigned int shaderProgram = createProgram(vertexShaderSource, fragmentShaderSource);

    // Upload cube geometry
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    const int stride = 9 * sizeof(float);
    // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    // color
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // normal
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    // Uniform locations
    int uModel      = glGetUniformLocation(shaderProgram, "model");
    int uView       = glGetUniformLocation(shaderProgram, "view");
    int uProjection = glGetUniformLocation(shaderProgram, "projection");
    int uLightPos   = glGetUniformLocation(shaderProgram, "lightPos");
    int uLightColor = glGetUniformLocation(shaderProgram, "lightColor");

    // Fixed projection & view
    Mat4 proj = perspective(0.785398f /*45°*/, 800.0f / 600.0f, 0.1f, 100.0f);
    Mat4 view = lookAt(1.8f, 1.4f, 2.5f,  0, 0, 0);

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        float t = (float)glfwGetTime();
        if (autoSpin) {
            rotY = t * 0.7f;
            rotX = t * 0.3f;
        }

        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Model = Ry * Rx
        Mat4 model = multiply(rotateY(rotY), rotateX(rotX));

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(uModel,      1, GL_FALSE, model.m);
        glUniformMatrix4fv(uView,       1, GL_FALSE, view.m);
        glUniformMatrix4fv(uProjection, 1, GL_FALSE, proj.m);
        glUniform3f(uLightPos,   2.0f, 3.0f, 4.0f);
        glUniform3f(uLightColor, 1.0f, 1.0f, 1.0f);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 36); // 6 faces × 6 verts
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}