#include "terrain.h"
#include "glad/glad.h"

#include "ui.h"
#include "structs.h"
#include "vendor/stb_image.h"
#include <algorithm>
#include <cmath>
#include <array>
#include <fstream>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <string>
#include <exception>

/*
 * Constants.
 */
constexpr auto clear_color = glm::vec4(0.45f, 0.55f, 0.60f, 1.0f);

/*
 * Globals. For convenience.
 */
static std::unordered_map<std::string, int> g_uniform_locations;
static unsigned int g_program = 0;
static unsigned int tex_plaster = 0;
static unsigned int tex_wood = 0;
static unsigned int tex_roof = 0;
static unsigned int tex_water = 0;
static unsigned int tex_grass = 0;
static unsigned int tex_stone = 0;
// Per-building hand-painted material textures (the original assets remain unchanged).
static unsigned int tex_upper_plaster=0, tex_upper_wood=0, tex_upper_roof=0;
static unsigned int tex_smithy_plaster=0, tex_smithy_wood=0, tex_smithy_roof=0;
static unsigned int tex_storehouse_plaster=0, tex_storehouse_wood=0, tex_storehouse_roof=0;
static unsigned int tex_tower_stone=0, tex_tower_roof=0;
static unsigned int tex_merchant_plaster=0, tex_merchant_wood=0, tex_merchant_roof=0;
static unsigned int tex_grass_tuft = 0;
static unsigned int tex_bush = 0;
static unsigned int g_grass_tuft_vao = 0;
static unsigned int g_grass_tuft_vertex_count = 0;
static unsigned int g_bush_vao = 0;
static unsigned int g_bush_vertex_count = 0;
static unsigned int g_roof_vao = 0;
static unsigned int g_roof_vertex_count = 0;
static unsigned int g_gable_vao = 0;
static unsigned int g_gable_vertex_count = 0;
static unsigned int g_cube_vao = 0;
static unsigned int g_paths_vao = 0;
static unsigned int g_paths_vertex_count = 0;
static glm::mat4 g_house_transform(1.0f);
static glm::mat4 g_model = glm::mat4(
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
);
static glm::vec3 g_light_pos = glm::vec3(6.0f, 7.0f, 5.0f);
static glm::vec3 g_light_color = glm::vec3(1.0f, 0.96f, 0.90f);
// Orbit camera settings
static glm::vec3 camera_target(0.0f, 1.0f, 0.0f);

static float camera_distance = 23.0f;
static float camera_yaw = 37.0f;
static float camera_pitch = 18.0f;

static double last_mouse_x = 0.0;
static double last_mouse_y = 0.0;

static bool first_mouse = true;

/*
 * Forward declarations.
 */
static void set_model(unsigned int);
static void draw_bridge();
static void draw_imported_watchtower();
static void draw_imported_blacksmith(const glm::vec3& position, float yaw_degrees);
static void draw_rocks();
static void draw_grass_tufts();
static void draw_bushes();
static void draw_forest();
static void set_view(unsigned int);
static void set_projection(unsigned int);
static void set_light_pos(unsigned int);
static void set_light_color(unsigned int);

/*
 * Checks for OpenGL errors.
 */
static unsigned int gl_print_error(void)
{
    unsigned int error = glGetError();
    if (error == 0)
        std::cout << "No GL errors." << std::endl;
    else
        std::cerr << "GL Error: " << error << std::endl;
    return error;
}

/*
 * Window error event callback.
 */
static void glfw_error_callback(int error, const char* description)
{
    std::cerr << "GLFW Error: " << error << " " << description << std::endl;
}

static void scroll_callback(
    GLFWwindow* window,
    double xoffset,
    double yoffset)
{
    camera_distance -= (float)yoffset * 0.6f;

    camera_distance = std::clamp(
        camera_distance, 2.0f, 40.0f
    );
}

static void update_camera(GLFWwindow* window)
{
    double mouse_x, mouse_y;
    glfwGetCursorPos(window, &mouse_x, &mouse_y);

    if (first_mouse)
    {
        last_mouse_x = mouse_x;
        last_mouse_y = mouse_y;
        first_mouse = false;
    }

    float dx = (float)(mouse_x - last_mouse_x);
    float dy = (float)(mouse_y - last_mouse_y);

    last_mouse_x = mouse_x;
    last_mouse_y = mouse_y;

    // Rotate camera with left mouse button
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)
        == GLFW_PRESS)
    {
        camera_yaw -= dx * 0.3f;
        camera_pitch += dy * 0.3f;

        camera_pitch = std::clamp(
            camera_pitch, -85.0f, 85.0f
        );
    }

    float yaw = glm::radians(camera_yaw);
    float pitch = glm::radians(camera_pitch);

    glm::vec3 direction(
        cos(pitch) * sin(yaw),
        sin(pitch),
        cos(pitch) * cos(yaw)
    );

    // Pan camera with right mouse button
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)
        == GLFW_PRESS)
    {
        glm::vec3 right = glm::normalize(
            glm::cross(
                glm::vec3(0.0f, 1.0f, 0.0f),
                direction
            )
        );

        glm::vec3 up = glm::normalize(
            glm::cross(direction, right)
        );

        float speed = camera_distance * 0.0015f;

        camera_target += right * dx * speed;
        camera_target += up * dy * speed;
    }

    // WASD movement relative to camera direction
    float move_speed = 0.08f;

    float yaw_rad = glm::radians(camera_yaw);

    glm::vec3 forward(
        -sin(yaw_rad),
        0.0f,
        -cos(yaw_rad)
    );

    glm::vec3 right(
        cos(yaw_rad),
        0.0f,
        -sin(yaw_rad)
    );

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera_target += forward * move_speed;

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera_target -= forward * move_speed;

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera_target -= right * move_speed;

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera_target += right * move_speed;


    cg::camera.center = camera_target;
    cg::camera.eye =
        camera_target + direction * camera_distance;
    cg::camera.up = glm::vec3(0.0f, 1.0f, 0.0f);

    set_view(g_program);
}


/*
 * Window keypress event callback.
 */
static void key_callback(GLFWwindow* window,
    int key,
    int scancode,
    int action,
    int mode)
{
    switch (key)
    {
    case GLFW_KEY_ESCAPE:
        /* Close on escape. */
        glfwSetWindowShouldClose(window, true);
        break;
    case GLFW_KEY_X:
        /* For debugging. */
        gl_print_error();
        break;
    case GLFW_KEY_R:
        if (action == GLFW_PRESS)
        {
            camera_target = glm::vec3(0.0f, 1.0f, 0.0f);
            camera_distance = 23.0f;
            camera_yaw = 37.0f;
            camera_pitch = 18.0f;
        }
        break;
    case GLFW_KEY_D:
        break;
    case GLFW_KEY_W:
        break;
    case GLFW_KEY_S:
        break;
    default:
        break;
    }
}

/*
 * Window resize event callback.
 */
static void size_callback(GLFWwindow* window, int width, int height)
{
    if (width == 0 || height == 0)
        return;

    glViewport(0, 0, width, height);
    cg::perspective.aspect = static_cast<float>(width) / height;
    set_projection(g_program);
};

/*
 * Create window.
 */
GLFWwindow* init_window(void)
{
    glfwSetErrorCallback(glfw_error_callback);

    if (glfwInit() == GLFW_FALSE)
        return nullptr;

    /*
     * Required for Apple.
     */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, cg::version.gl_major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, cg::version.gl_minor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    /*
     * Multisampling.
     */
    glfwWindowHint(GLFW_SAMPLES, 4);

    /*
     * Create the graphics context and make it current.
     */
    GLFWwindow* const window = glfwCreateWindow(cg::window.window_width,
        cg::window.window_height,
        cg::window.window_title,
        nullptr,
        nullptr);

    if (window == nullptr)
        return nullptr;

    glfwMakeContextCurrent(window);

    /*
     * Enable VSync.
     */
    glfwSwapInterval(1);

    /*
     * Set event callbacks.
     */
    glfwSetKeyCallback(window, key_callback);
    glfwSetWindowSizeCallback(window, size_callback);
    glfwSetScrollCallback(window, scroll_callback);

    /*
     * Load OpenGL with GLAD.
     * Crashes if GL functions are called without this.
     */
    gladLoadGL();

    return window;
}

/*
 * Clear color and depth buffers.
 */
static void clear(void)
{
    glClearColor(clear_color.x * clear_color.w,
        clear_color.y * clear_color.w,
        clear_color.z * clear_color.w,
        clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/*
 * Destroy window.
 */
static void cleanup_window(GLFWwindow* window)
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

/*
 * Read shader from file.
 */
static std::optional<std::string> read_shader(const std::string& path)
{
    std::string result;

    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (in.good() == false)
        return std::nullopt;

    in.seekg(0, std::ios::end);
    size_t size = in.tellg();

    if (size == -1)
        return std::nullopt;

    result.resize(size);
    in.seekg(0, std::ios::beg);
    in.read(&result[0], size);

    return result;
}

/*
 * Compile shader.
 * Copy the shader source string here just in case.
 * c_str will point to garbage if a string reference goes out of scope.
 */
static unsigned int compile_shader(const std::string shader_source,
    unsigned int type)
{
    unsigned int shader = glCreateShader(type);

    const char* c_str = shader_source.c_str();
    glShaderSource(shader, 1, &c_str, nullptr);

    glCompileShader(shader);

    /*
     * Compile error checking.
     */
    int is_compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &is_compiled);
    if (is_compiled == 0)
    {
        std::string log;
        int length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        log.resize(length);
        glGetShaderInfoLog(shader, length, nullptr, &log[0]);

        std::string type_s = type == GL_VERTEX_SHADER ? "vertex" : "fragment";
        std::cerr << "Failed to compile " << type_s << " shader." << std::endl;
        std::cerr << log << std::endl;

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

/*
 * Compile and link shader program.
 */
static unsigned int create_shader(const std::string& vertex_source,
    const std::string& fragment_source)
{
    unsigned int program = glCreateProgram();
    unsigned int vertex_shader = compile_shader(vertex_source, GL_VERTEX_SHADER);
    if (vertex_shader == 0)
        return 0;
    unsigned int fragment_shader = compile_shader(fragment_source, GL_FRAGMENT_SHADER);
    if (fragment_shader == 0)
        return 0;

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glValidateProgram(program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    glDetachShader(program, vertex_shader);
    glDetachShader(program, fragment_shader);

    return program;
}

int get_uniform_location(unsigned int program, const std::string& location)
{
    if (g_uniform_locations.find(location) != g_uniform_locations.end())
        return g_uniform_locations[location];

    int uniform = glGetUniformLocation(program, location.c_str());
    if (uniform == -1)
        std::cout << "Warning: Uniform " << location <<
        " does not exist. This uniform will not be set." << std::endl;

    g_uniform_locations[location] = uniform;
    return uniform;
}

static void set_vec3(unsigned int program,
    const glm::vec3& vector,
    const std::string& location)
{
    int uniform = get_uniform_location(program, location);
    if (uniform == -1)
        return;
    glUniform3fv(uniform, 1, glm::value_ptr(vector));
}

static void set_matrix(unsigned int program,
    const glm::mat4& matrix,
    const std::string& location)
{
    int uniform = get_uniform_location(program, location);
    if (uniform == -1)
        return;
    glUniformMatrix4fv(uniform, 1, false, glm::value_ptr(matrix));
}

/*
 * Set model matrix uniform.
 */
static void set_model(unsigned int program)
{
    set_matrix(program, g_model, "u_model");
}

/*
 * Set view matrix uniform.
 */
static void set_view(unsigned int program)
{
    glm::mat4 view = glm::lookAt(cg::camera.eye,
        cg::camera.center,
        cg::camera.up);

    set_matrix(program, view, "u_view");
    set_vec3(program, cg::camera.eye, "u_view_pos");
}

/*
 * Set projection matrix uniform.
 */
static void set_projection(unsigned int program)
{
    glm::mat4 projection = glm::perspective(cg::perspective.fov,
        cg::perspective.aspect,
        cg::perspective.z_near,
        cg::perspective.z_far);

    set_matrix(program, projection, "u_projection");
}

/*
 * Set light position.
 */
static void set_light_pos(unsigned int program)
{
    set_vec3(program, g_light_pos, "u_light_pos");
}

/*
 * Set light color.
 */
static void set_light_color(unsigned int program)
{
    set_vec3(program, g_light_color, "u_light_color");
}

/*
 * Vertex buffer object.
 * Uploads draw data to GPU memory.
 */
static unsigned int init_vbo(void)
{
    /*
     * Cube.
     */
    std::array<float, 312> vertices =
    {
        /* Position            Normal                Texture Coords */
        /* Front face */
        -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   1.0f, 1.0f,

         0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,   0.0f, 0.0f,

        /* Back face */
        -0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   1.0f, 1.0f,

         0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,   0.0f, 0.0f,

        /* Left face */
        -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,   1.0f, 1.0f,

        -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,   1.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,   0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   0.0f, 0.0f,

        /* Right face */
         0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  0.0f,   0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   1.0f,  0.0f,  0.0f,   1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,   1.0f, 1.0f,

         0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,   1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  0.0f,   0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  0.0f,   0.0f, 0.0f,

         /* Top face */
         -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,   0.0f, 0.0f,
          0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,   1.0f, 0.0f,
          0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,   1.0f, 1.0f,

          0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,   1.0f, 1.0f,
         -0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,   0.0f, 1.0f,
         -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,   0.0f, 0.0f,

         /* Bottom face */
         -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,   0.0f, 0.0f,
          0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,   1.0f, 0.0f,
          0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,   1.0f, 1.0f,

          0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,   1.0f, 1.0f,
         -0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,   0.0f, 1.0f,
         -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,   0.0f, 0.0f,

         -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,   0.0f, 0.0f,
          0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,   1.0f, 0.0f,
          0.5f, 1.5f,  0.5f,   0.0f, -1.0f,  0.0f,   1.0f, 1.0f,
    };

    unsigned int vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices[0], GL_STATIC_DRAW);

    return vbo;
}

/*
 * Vertex array object.
 * Specifies the format of the draw data.
 */
static unsigned int init_vao(void)
{
    unsigned int vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    /*
     * Position attribute.
     */
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    /*
     * Normal attribute.
     */
    glVertexAttribPointer(1, 3, GL_FLOAT, false, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    /*
     * Texture coordinate attribute.
     */
    glVertexAttribPointer(2, 2, GL_FLOAT, false, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    return vao;
}

// Unified roof parameters: every roof vertex comes from this definition.
struct RoofDimensions
{
    float half_length = 1.51f;  // X: overhang past side walls
    float half_span = 1.42f;    // Z: half-width, including eaves
    float ridge_y = 2.82f;     // top of ridge
    float eave_y = 1.89f;      // top of eaves
    float thickness = 0.12f;  // vertical thickness of roof
    float wall_half_span = 1.10f;
    float wall_top_y = 1.825f;
    float wall_half_length = 1.25f;
};

static const RoofDimensions roof;

// Each vertex: position XYZ, normal XYZ, UV.
static void add_triangle(std::vector<float>& mesh,
    glm::vec3 a, glm::vec3 b, glm::vec3 c,
    glm::vec2 uv_a, glm::vec2 uv_b, glm::vec2 uv_c,
    glm::vec3 outward)
{
    glm::vec3 calculated = glm::cross(b - a, c - a);
    if (glm::dot(calculated, outward) < 0.0f)
    {
        std::swap(b, c);
        std::swap(uv_b, uv_c);
    }

    glm::vec3 normal = glm::normalize(outward);
    auto vertex = [&](glm::vec3 p, glm::vec2 uv)
        {
            mesh.insert(mesh.end(), {
                p.x, p.y, p.z,
                normal.x, normal.y, normal.z,
                uv.x, uv.y
                });
        };
    vertex(a, uv_a);
    vertex(b, uv_b);
    vertex(c, uv_c);
}

static void add_quad(std::vector<float>& mesh,
    glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
    glm::vec3 outward, float u_repeat = 1.0f, float v_repeat = 1.0f)
{
    add_triangle(mesh, a, b, c,
        { 0.0f, 0.0f }, { 0.0f, v_repeat }, { u_repeat, v_repeat }, outward);
    add_triangle(mesh, a, c, d,
        { 0.0f, 0.0f }, { u_repeat, v_repeat }, { u_repeat, 0.0f }, outward);
}

static unsigned int upload_mesh(const std::vector<float>& vertices,
    unsigned int& vertex_count)
{
    unsigned int vao = 0;
    unsigned int vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
        8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
        8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
        8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    vertex_count = static_cast<unsigned int>(vertices.size() / 8);
    return vao;
}

// Creates both tiled slopes, their undersides and thickness (no cubes).
// Gables use the very same dimensions, so they cannot protrude through the roof.
static void init_roof()
{
    std::vector<float> roof_mesh;
    std::vector<float> gable_mesh;
    const float x0 = -roof.half_length;
    const float x1 = roof.half_length;
    const float t = roof.thickness;

    for (float side : {-1.0f, 1.0f})
    {
        float ez = side * roof.half_span;
        glm::vec3 ridge0(x0, roof.ridge_y, 0.0f);
        glm::vec3 ridge1(x1, roof.ridge_y, 0.0f);
        glm::vec3 eave0(x0, roof.eave_y, ez);
        glm::vec3 eave1(x1, roof.eave_y, ez);
        glm::vec3 below(0.0f, t, 0.0f);
        glm::vec3 outward(0.0f, roof.half_span,
            side * (roof.ridge_y - roof.eave_y));

        // Visible tiled surface, slope and UV both shared at the ridge.
        add_quad(roof_mesh, ridge0, eave0, eave1, ridge1, outward);
        // Underneath the tiles.
        add_quad(roof_mesh, ridge0 - below, eave0 - below,
            eave1 - below, ridge1 - below, -outward);
        // Eave edge and two end edges: roof is a closed solid.
        add_quad(roof_mesh, eave0, eave0 - below,
            eave1 - below, eave1, glm::vec3(0, 0, side));
        add_quad(roof_mesh, ridge0, eave0,
            eave0 - below, ridge0 - below, glm::vec3(-1, 0, 0));
        add_quad(roof_mesh, ridge1, eave1,
            eave1 - below, ridge1 - below, glm::vec3(1, 0, 0));
    }

    // The two triangular end walls: bottom is at the actual wall height;
    // upper edges exactly follow the underside of the roof.
    const float edge_y = roof.ridge_y -
        (roof.ridge_y - roof.eave_y) *
        (roof.wall_half_span / roof.half_span) - roof.thickness;
    const float peak_y = roof.ridge_y - roof.thickness;

    for (float side : {-1.0f, 1.0f})
    {
        const float x = side * (roof.wall_half_length + 0.002f);
        glm::vec3 outward(side, 0, 0);
        glm::vec3 bottom_left(x, roof.wall_top_y, -roof.wall_half_span);
        glm::vec3 bottom_right(x, roof.wall_top_y, roof.wall_half_span);
        glm::vec3 upper_left(x, edge_y, -roof.wall_half_span);
        glm::vec3 upper_right(x, edge_y, roof.wall_half_span);
        glm::vec3 peak(x, peak_y, 0.0f);

        // Lower trapezoid and upper triangle share vertices, no open sides.
        add_quad(gable_mesh, bottom_left, bottom_right,
            upper_right, upper_left, outward);
        add_triangle(gable_mesh, upper_left, upper_right, peak,
            { 0,0 }, { 1,0 }, { 0.5f,1 }, outward);
    }

    g_roof_vao = upload_mesh(roof_mesh, g_roof_vertex_count);
    g_gable_vao = upload_mesh(gable_mesh, g_gable_vertex_count);
}

/*
 * Load and bind texture image.
 */
static unsigned int init_texture(const std::string& path)
{
    int texture_width = 0;
    int texture_height = 0;
    int texture_bpp = 0;

    stbi_set_flip_vertically_on_load(1);

    unsigned char* texture_data = stbi_load(path.c_str(),
        &texture_width,
        &texture_height,
        &texture_bpp,
        4);

    if (texture_data == nullptr)
    {
        std::cerr << "Failed to load texture." << std::endl;
        return 0;
    }

    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    /*
     * Texture wrapping and filtering.
     * Required.
     */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        texture_width,
        texture_height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        texture_data);

    glGenerateMipmap(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);


    stbi_image_free(texture_data);
    return texture;
}

/*
 * Shader setup.
 */
static unsigned int init_program(const std::string& vertex_path,
    const std::string& fragment_path)
{
    const auto vertex_source = read_shader(vertex_path);
    const auto fragment_source = read_shader(fragment_path);
    if (vertex_source.has_value() == false ||
        fragment_source.has_value() == false)
    {
        std::cerr << "Failed to read shaders." << std::endl;
        return 0;
    }

    unsigned int program = create_shader(vertex_source.value(),
        fragment_source.value());

    return program;
}

/*
 * Init scene.
 */
static void init(void)
{
    /*
     * Enable z-buffer and multisampling.
     */
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    /*
     * Blending. For transparency.
     */
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    unsigned int vbo = init_vbo();
    unsigned int vao = init_vao();
    g_cube_vao = vao;
    init_roof();
    glBindVertexArray(g_cube_vao);
    tex_water = init_texture("resources/textures/water.png");
    tex_grass_tuft = init_texture("resources/textures/grass_tuft.png");
    tex_bush = init_texture("resources/textures/bush.png");
    tex_plaster = init_texture("resources/textures/plaster.png");
    tex_wood = init_texture("resources/textures/wood.png");
    tex_roof = init_texture("resources/textures/roof.png");
    tex_grass = init_texture("resources/textures/grass.png");
    tex_stone = init_texture("resources/textures/stone.png");
    tex_upper_plaster = init_texture("resources/textures/plaster_upper.png");
    tex_upper_wood = init_texture("resources/textures/wood_upper.png");
    tex_upper_roof = init_texture("resources/textures/roof_upper.png");
    tex_smithy_plaster = init_texture("resources/textures/plaster_smithy.png");
    tex_smithy_wood = init_texture("resources/textures/wood_smithy.png");
    tex_smithy_roof = init_texture("resources/textures/roof_smithy.png");
    tex_storehouse_plaster = init_texture("resources/textures/plaster_storehouse.png");
    tex_storehouse_wood = init_texture("resources/textures/wood_storehouse.png");
    tex_storehouse_roof = init_texture("resources/textures/roof_storehouse.png");
    tex_tower_stone = init_texture("resources/textures/tower_stone.png");
    tex_tower_roof = init_texture("resources/textures/tower_slates.png");
    tex_merchant_plaster = init_texture("resources/textures/plaster_merchant.png");
    tex_merchant_wood = init_texture("resources/textures/wood_merchant.png");
    tex_merchant_roof = init_texture("resources/textures/roof_merchant.png");
    glBindTexture(GL_TEXTURE_2D, tex_water);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, tex_stone);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, tex_grass);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    village_terrain::initialize();
    std::cout << "Data init check:" << std::endl;
    if (gl_print_error() != 0)
        return;

    unsigned int program = init_program("resources/shaders/phong_v.glsl",
        "resources/shaders/phong_f.glsl");

    if (program == 0)
    {
        std::cerr << "Failed to compile shaders." << std::endl;
        return;
    }

    glUseProgram(program);
    g_program = program;

    /*
     * Set PVM matrix.
     */
    set_model(program);
    set_view(program);
    set_projection(program);

    /*
     * Set light parameters.
     */
    set_light_pos(program);
    set_light_color(program);
}


static void draw_cube(
    glm::vec3 position,
    glm::vec3 size,
    glm::vec3 color,
    unsigned int texture,
    float rotation = 0.0f)
{
    g_model = glm::mat4(1.0f);

    g_model = g_house_transform * glm::translate(g_model, position);

    // Rotate around X axis (used for roof slopes)
    g_model = glm::rotate(
        g_model,
        glm::radians(rotation),
        glm::vec3(1.0f, 0.0f, 0.0f)
    );

    g_model = glm::scale(g_model, size);

    set_model(g_program);
    set_vec3(g_program, color, "u_tint");

    // Activate the object's texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glDrawArrays(GL_TRIANGLES, 0, 36);
}

static void draw_roof(const glm::vec3& tint = glm::vec3(1.0f), unsigned int roof_texture = 0)
{
    g_model = g_house_transform;
    set_model(g_program);
    set_vec3(g_program, tint, "u_tint");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, roof_texture ? roof_texture : tex_roof);
    glBindVertexArray(g_roof_vao);
    glDrawArrays(GL_TRIANGLES, 0, g_roof_vertex_count);
    glBindVertexArray(g_cube_vao);
}

static void draw_gables(unsigned int plaster_texture = 0)
{
    g_model = g_house_transform;
    set_model(g_program);
    set_vec3(g_program, glm::vec3(1.0f), "u_tint");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, plaster_texture ? plaster_texture : tex_plaster);
    glBindVertexArray(g_gable_vao);
    glDrawArrays(GL_TRIANGLES, 0, g_gable_vertex_count);
    glBindVertexArray(g_cube_vao);
}


// Dedicated barn roof. The ridge runs along Z, so the triangular gables
// face the front and back doors. Does not affect the other buildings.
static void draw_storehouse_roof(unsigned int roof_texture, unsigned int wall_texture)
{
    static unsigned int roof_vao = 0, gable_vao = 0;
    static unsigned int roof_count = 0, gable_count = 0;
    if (!roof_vao)
    {
        std::vector<float> tiles, gables;
        const float half_width = 1.79f;
        const float half_length = 1.60f;
        const float ridge_y = 2.68f;
        const float eave_y = 1.78f;
        const float thickness = 0.12f;
        const glm::vec3 down(0.0f, thickness, 0.0f);

        for (float side : {-1.0f, 1.0f})
        {
            const float x = side * half_width;
            glm::vec3 r0(0.0f, ridge_y, -half_length);
            glm::vec3 r1(0.0f, ridge_y, half_length);
            glm::vec3 e0(x, eave_y, -half_length);
            glm::vec3 e1(x, eave_y, half_length);
            const glm::vec3 outward(side * (ridge_y - eave_y), half_width, 0.0f);
            add_quad(tiles, r0, e0, e1, r1, outward, 1.0f, 1.0f);
            add_quad(tiles, r0 - down, r1 - down, e1 - down, e0 - down, -outward);
            add_quad(tiles, e0, e0 - down, e1 - down, e1,
                     glm::vec3(side, 0.0f, 0.0f));
            add_quad(tiles, r0, r0 - down, e0 - down, e0,
                     glm::vec3(0.0f, 0.0f, -1.0f));
            add_quad(tiles, r1, e1, e1 - down, r1 - down,
                     glm::vec3(0.0f, 0.0f, 1.0f));
        }

        // Front and back plaster gables follow the *underside* of the roof.
        // Wall half-width is 1.50, so the roof eaves extend 0.29 beyond it.
        const float wall_top = 1.80f;
        const float peak = ridge_y - thickness;
        for (float side : {-1.0f, 1.0f})
        {
            float z = side * 1.231f;
            add_triangle(gables,
                glm::vec3(-1.50f, wall_top, z),
                glm::vec3(1.50f, wall_top, z),
                glm::vec3(0.0f, peak, z),
                glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f),
                glm::vec2(0.5f, 1.0f), glm::vec3(0.0f, 0.0f, side));
        }
        roof_vao = upload_mesh(tiles, roof_count);
        gable_vao = upload_mesh(gables, gable_count);
    }

    g_model = g_house_transform;
    set_model(g_program);
    set_vec3(g_program, glm::vec3(1.0f), "u_tint");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, wall_texture);
    glBindVertexArray(gable_vao);
    glDrawArrays(GL_TRIANGLES, 0, gable_count);
    glBindTexture(GL_TEXTURE_2D, roof_texture);
    glBindVertexArray(roof_vao);
    glDrawArrays(GL_TRIANGLES, 0, roof_count);
    glBindVertexArray(g_cube_vao);
}


// More distinctive cottage roof for the merchant-style house.
// Ridge runs along Z so the triangular gables face front/back, which
// differentiates it from the standard house silhouette.
static void draw_merchant_roof(unsigned int roof_texture, unsigned int wall_texture)
{
    static unsigned int roof_vao = 0, gable_vao = 0;
    static unsigned int roof_count = 0, gable_count = 0;
    if (!roof_vao)
    {
        std::vector<float> tiles, gables;
        const float half_width = 1.58f;
        const float half_length = 1.50f;
        const float ridge_y = 2.74f;
        const float eave_y = 1.88f;
        const float thickness = 0.12f;
        const glm::vec3 down(0.0f, thickness, 0.0f);

        for (float side : {-1.0f, 1.0f})
        {
            const float x = side * half_width;
            glm::vec3 r0(0.0f, ridge_y, -half_length);
            glm::vec3 r1(0.0f, ridge_y,  half_length);
            glm::vec3 e0(x, eave_y, -half_length);
            glm::vec3 e1(x, eave_y,  half_length);
            const glm::vec3 outward(side * (ridge_y - eave_y), half_width, 0.0f);
            add_quad(tiles, r0, e0, e1, r1, outward, 1.0f, 1.0f);
            add_quad(tiles, r0 - down, r1 - down, e1 - down, e0 - down, -outward);
            add_quad(tiles, e0, e0 - down, e1 - down, e1,
                     glm::vec3(side, 0.0f, 0.0f));
            add_quad(tiles, r0, r0 - down, e0 - down, e0,
                     glm::vec3(0.0f, 0.0f, -1.0f));
            add_quad(tiles, r1, e1, e1 - down, r1 - down,
                     glm::vec3(0.0f, 0.0f, 1.0f));
        }

        const float wall_top = 1.82f;
        const float peak = ridge_y - thickness;
        for (float side : {-1.0f, 1.0f})
        {
            const float z = side * 1.181f;
            add_triangle(gables,
                glm::vec3(-1.31f, wall_top, z),
                glm::vec3( 1.31f, wall_top, z),
                glm::vec3( 0.0f, peak, z),
                glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f),
                glm::vec2(0.5f, 1.0f), glm::vec3(0.0f, 0.0f, side));
        }
        roof_vao = upload_mesh(tiles, roof_count);
        gable_vao = upload_mesh(gables, gable_count);
    }

    g_model = g_house_transform;
    set_model(g_program);
    set_vec3(g_program, glm::vec3(1.0f), "u_tint");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, wall_texture);
    glBindVertexArray(gable_vao);
    glDrawArrays(GL_TRIANGLES, 0, gable_count);
    glBindTexture(GL_TEXTURE_2D, roof_texture);
    glBindVertexArray(roof_vao);
    glDrawArrays(GL_TRIANGLES, 0, roof_count);
    glBindVertexArray(g_cube_vao);
}

/*
 * Draw function.
 */



static void draw_house(glm::vec3 position, float scale, float yaw_degrees, int kind = 0)
{
    g_house_transform = glm::translate(glm::mat4(1.0f), position);
    g_house_transform = glm::rotate(g_house_transform, glm::radians(yaw_degrees), glm::vec3(0, 1, 0));
    g_house_transform = glm::scale(g_house_transform, glm::vec3(scale));

    const glm::vec3 white(1.0f);

    const bool upstairs = (kind == 1);
    const bool smithy = (kind == 2);
    const bool storehouse = (kind == 3);
    const bool merchant = (kind == 4);

    const unsigned int house_plaster = upstairs ? tex_upper_plaster :
        (smithy ? tex_smithy_plaster : (storehouse ? tex_storehouse_plaster : (merchant ? tex_merchant_plaster : tex_plaster)));

    const unsigned int house_wood = upstairs ? tex_upper_wood :
        (smithy ? tex_smithy_wood : (storehouse ? tex_storehouse_wood : (merchant ? tex_merchant_wood : tex_wood)));

    const unsigned int house_roof = upstairs ? tex_upper_roof :
        (smithy ? tex_smithy_roof : (storehouse ? tex_storehouse_roof : (merchant ? tex_merchant_roof : tex_roof)));

    const glm::mat4 base_transform = g_house_transform;


    // =========================================================
    // 0) MERCHANT COTTAGE / RICHER HOUSE VARIANT
    // =========================================================
    if (merchant)
    {
        // Base and main volume
        draw_cube({0.0f, 0.12f, 0.0f}, {3.05f, 0.24f, 2.85f}, white, tex_stone);
        draw_cube({0.0f, 1.00f, 0.0f}, {2.62f, 1.64f, 2.36f}, white, house_plaster);

        // Timber frame, more decorative than the standard house
        for (float x : {-1.24f, 1.24f})
        {
            draw_cube({x, 1.00f,  1.17f}, {0.12f, 1.66f, 0.12f}, white, house_wood);
            draw_cube({x, 1.00f, -1.17f}, {0.12f, 1.66f, 0.12f}, white, house_wood);
        }
        draw_cube({0.0f, 1.82f,  1.17f}, {2.60f, 0.12f, 0.12f}, white, house_wood);
        draw_cube({0.0f, 1.82f, -1.17f}, {2.60f, 0.12f, 0.12f}, white, house_wood);
        draw_cube({ 1.29f, 1.00f, 0.0f}, {0.12f, 1.66f, 2.34f}, white, house_wood);
        draw_cube({-1.29f, 1.00f, 0.0f}, {0.12f, 1.66f, 2.34f}, white, house_wood);
        draw_cube({0.0f, 1.08f, 1.17f}, {2.60f, 0.08f, 0.08f}, white, house_wood);

        // Porch platform for a richer facade
        draw_cube({0.0f, 0.18f, 1.42f}, {1.80f, 0.10f, 0.34f}, white, tex_stone);

        // Distinctive front-facing gable roof
        draw_merchant_roof(house_roof, house_plaster);

        // Front porch canopy / awning
        for (float x : {-0.86f, 0.86f})
        {
            draw_cube({x, 0.07f, 1.58f}, {0.20f, 0.14f, 0.20f}, white, tex_stone);
            draw_cube({x, 0.78f, 1.58f}, {0.11f, 1.42f, 0.11f}, white, house_wood);
        }
        draw_cube({0.0f, 1.55f, 1.42f}, {2.04f, 0.09f, 0.82f}, glm::vec3(0.95f, 0.89f, 0.83f), house_wood, 14.0f);
        draw_cube({0.0f, 1.51f, 1.72f}, {1.94f, 0.10f, 0.12f}, white, house_wood);
        for (float x : {-0.65f, 0.0f, 0.65f})
            draw_cube({x, 1.46f, 1.50f}, {0.08f, 0.10f, 0.58f}, white, house_wood, 14.0f);

        // Front door - stays near the centre so the path still lines up
        draw_cube({0.0f, 0.72f, 1.22f}, {0.74f, 1.18f, 0.08f}, white, house_wood);
        draw_cube({-0.24f, 0.72f, 1.245f}, {0.05f, 1.10f, 0.02f}, glm::vec3(0.86f, 0.79f, 0.69f), house_wood);
        draw_cube({ 0.24f, 0.72f, 1.245f}, {0.05f, 1.10f, 0.02f}, glm::vec3(0.86f, 0.79f, 0.69f), house_wood);
        draw_cube({ 0.0f, 1.29f, 1.245f}, {0.60f, 0.05f, 0.02f}, glm::vec3(0.86f, 0.79f, 0.69f), house_wood);
        draw_cube({ 0.30f, 0.66f, 1.25f}, {0.04f, 0.04f, 0.03f}, glm::vec3(0.78f, 0.66f, 0.20f), tex_stone);
        draw_cube({0.0f, 0.20f, 1.33f}, {0.96f, 0.10f, 0.24f}, white, tex_stone);

        // Large front windows for a more prosperous house
        for (float x : {-0.92f, 0.92f})
        {
            draw_cube({x, 1.18f, 1.205f}, {0.34f, 0.42f, 0.05f}, glm::vec3(0.64f, 0.82f, 0.94f), house_plaster);
            draw_cube({x - 0.19f, 1.18f, 1.23f}, {0.05f, 0.54f, 0.05f}, white, house_wood);
            draw_cube({x + 0.19f, 1.18f, 1.23f}, {0.05f, 0.54f, 0.05f}, white, house_wood);
            draw_cube({x, 1.43f, 1.23f}, {0.46f, 0.05f, 0.05f}, white, house_wood);
            draw_cube({x, 0.93f, 1.23f}, {0.46f, 0.05f, 0.05f}, white, house_wood);
            draw_cube({x, 1.18f, 1.24f}, {0.03f, 0.40f, 0.02f}, white, house_wood);
            draw_cube({x, 1.18f, 1.24f}, {0.32f, 0.03f, 0.02f}, white, house_wood);
            draw_cube({x, 0.88f, 1.24f}, {0.58f, 0.05f, 0.10f}, white, house_wood);
        }

        // Side windows and decorative bracing to break up the walls
        draw_cube({ 1.18f, 1.20f, 0.18f}, {0.03f, 0.36f, 0.32f}, glm::vec3(0.62f, 0.80f, 0.92f), house_plaster);
        draw_cube({ 1.205f, 1.20f, 0.18f}, {0.02f, 0.48f, 0.44f}, white, house_wood);
        draw_cube({ 1.215f, 1.20f, 0.18f}, {0.02f, 0.38f, 0.03f}, white, house_wood);
        draw_cube({ 1.215f, 1.20f, 0.18f}, {0.02f, 0.03f, 0.28f}, white, house_wood);
        draw_cube({ 1.21f, 0.92f, 0.18f},  {0.03f, 0.04f, 0.52f}, white, house_wood);
        draw_cube({-1.18f, 1.20f, -0.18f}, {0.03f, 0.34f, 0.30f}, glm::vec3(0.62f, 0.80f, 0.92f), house_plaster);
        draw_cube({-1.205f, 1.20f, -0.18f}, {0.02f, 0.46f, 0.42f}, white, house_wood);
        draw_cube({-1.215f, 1.20f, -0.18f}, {0.02f, 0.30f, 0.03f}, white, house_wood);
        draw_cube({-1.215f, 1.20f, -0.18f}, {0.02f, 0.03f, 0.26f}, white, house_wood);
        draw_cube({-0.84f, 1.00f,  1.16f}, {0.08f, 1.44f, 0.08f}, white, house_wood);
        draw_cube({ 0.84f, 1.00f,  1.16f}, {0.08f, 1.44f, 0.08f}, white, house_wood);
        draw_cube({-0.85f, 1.12f, -0.95f}, {0.08f, 1.20f, 0.08f}, white, house_wood, 28.0f);
        draw_cube({ 0.85f, 1.12f, -0.95f}, {0.08f, 1.20f, 0.08f}, white, house_wood, -28.0f);

        // Small decorative sign beam
        draw_cube({0.0f, 1.98f, 1.05f}, {1.18f, 0.10f, 0.10f}, white, house_wood);
        draw_cube({0.0f, 1.72f, 1.09f}, {0.56f, 0.34f, 0.04f}, glm::vec3(0.90f, 0.84f, 0.73f), house_wood);

        // Side chimney and a little bench/crate for life
        draw_cube({-0.98f, 1.98f, -0.55f}, {0.34f, 1.05f, 0.34f}, glm::vec3(0.86f, 0.82f, 0.78f), tex_stone);
        draw_cube({-0.98f, 2.52f, -0.55f}, {0.42f, 0.08f, 0.42f}, glm::vec3(0.73f, 0.70f, 0.66f), tex_stone);
        draw_cube({-1.02f, 0.30f, 1.52f}, {0.40f, 0.22f, 0.24f}, glm::vec3(0.82f, 0.76f, 0.67f), house_wood);
        draw_cube({ 1.05f, 0.24f, 1.48f}, {0.28f, 0.28f, 0.28f}, glm::vec3(0.76f, 0.70f, 0.62f), house_wood);

        g_house_transform = glm::mat4(1.0f);
        return;
    }

    // =========================================================
    // 1) КОВАЧНИЦА
    // =========================================================
    // The smithy (kind 2) is now drawn from a Blockbench OBJ in render().
    // No procedural smithy geometry is drawn here.


    // =========================================================
    // 2) СКЛАД
    // =========================================================

    if (storehouse)
    {
        // Wider barn walls. The roof is an independent, correctly sized mesh.
        draw_cube({0.0f, 0.12f, 0.0f}, {3.25f, 0.24f, 2.75f}, white, tex_stone);
        draw_cube({0.0f, 1.00f, 0.0f}, {3.00f, 1.60f, 2.45f}, white, house_plaster);

        // Four structural corner posts and straight horizontal timber bands.
        for (float x : {-1.48f, 1.48f})
        {
            for (float z : {-1.26f, 1.26f})
                draw_cube({x, 1.00f, z}, {0.14f, 1.62f, 0.14f}, white, house_wood);
        }
        for (float z : {-1.26f, 1.26f})
        {
            draw_cube({0.0f, 1.76f, z}, {3.08f, 0.12f, 0.14f}, white, house_wood);
            draw_cube({0.0f, 0.35f, z}, {3.08f, 0.10f, 0.10f}, white, house_wood);
        }
        for (float x : {-1.52f, 1.52f})
            draw_cube({x, 1.00f, 0.0f}, {0.14f, 1.62f, 2.45f}, white, house_wood);

        // Its gables face local +/-Z and overhang the walls on all sides.
        draw_storehouse_roof(house_roof, house_plaster);

        // Double barn doors are EXTERIOR to the front wall (front = Z 1.225).
        draw_cube({0.0f, 0.73f, 1.275f}, {1.43f, 1.21f, 0.09f}, white, house_wood);
        draw_cube({0.0f, 0.73f, 1.329f}, {0.065f, 1.17f, 0.025f},
                  glm::vec3(0.75f, 0.68f, 0.59f), house_wood);
        for (float x : {-0.36f, 0.36f})
        {
            draw_cube({x, 0.72f, 1.332f}, {0.05f, 1.10f, 0.025f},
                      glm::vec3(0.79f, 0.71f, 0.61f), house_wood);
            draw_cube({x, 0.45f, 1.337f}, {0.55f, 0.05f, 0.025f},
                      glm::vec3(0.76f, 0.68f, 0.59f), house_wood);
        }
        // Visible door frame and the threshold directly underneath it.
        for (float x : {-0.76f, 0.76f})
            draw_cube({x, 0.77f, 1.338f}, {0.09f, 1.35f, 0.10f}, white, house_wood);
        draw_cube({0.0f, 1.43f, 1.338f}, {1.59f, 0.10f, 0.10f}, white, house_wood);
        draw_cube({0.0f, 0.20f, 1.44f}, {1.40f, 0.10f, 0.30f}, white, tex_stone);

        // Small loft vent, above the barn entrance, is also outside the wall.
        draw_cube({0.0f, 1.61f, 1.282f}, {0.48f, 0.24f, 0.07f},
                  glm::vec3(0.72f, 0.67f, 0.60f), house_wood);
        draw_cube({0.0f, 1.61f, 1.332f}, {0.055f, 0.20f, 0.035f}, white, house_wood);

        // Side wooden service hatch on right-hand exterior wall (X = 1.50).
        draw_cube({1.552f, 0.98f, -0.10f}, {0.08f, 0.86f, 0.70f}, white, house_wood);
        draw_cube({1.601f, 0.98f, -0.10f}, {0.025f, 0.80f, 0.05f},
                  glm::vec3(0.78f, 0.70f, 0.61f), house_wood);
        draw_cube({1.552f, 1.48f, 0.72f}, {0.075f, 0.25f, 0.27f},
                  glm::vec3(0.61f, 0.75f, 0.84f), house_plaster);
        draw_cube({1.601f, 1.48f, 0.72f}, {0.025f, 0.33f, 0.35f}, white, house_wood);

        // Crates are outside the building and sit on the ground.
        draw_cube({-1.10f, 0.21f, 1.82f}, {0.42f, 0.42f, 0.42f},
                  glm::vec3(0.83f, 0.77f, 0.69f), house_wood);
        draw_cube({1.10f, 0.18f, 1.76f}, {0.36f, 0.36f, 0.36f},
                  glm::vec3(0.76f, 0.70f, 0.63f), house_wood);

        g_house_transform = glm::mat4(1.0f);
        return;
    }

    // =========================================================
    // 3) ОБИКНОВЕНА / ДВУЕТАЖНА
    // =========================================================
    const float upper_height = upstairs ? 1.22f : 0.0f;

    // base
    draw_cube({ 0.0f, 0.12f, 0.0f }, { 2.8f, 0.24f, 2.5f }, white, tex_stone);
    draw_cube({ 0.0f, 1.025f, 0.0f }, { 2.5f, 1.60f, 2.2f }, white, house_plaster);

    // first floor timber skeleton
    draw_cube({ -1.23f, 1.05f, 1.16f }, { 0.12f, 1.7f, 0.12f }, white, house_wood);
    draw_cube({ 1.23f, 1.05f, 1.16f }, { 0.12f, 1.7f, 0.12f }, white, house_wood);
    draw_cube({ 0.0f, 1.88f, 1.16f }, { 2.58f, 0.12f, 0.12f }, white, house_wood);
    draw_cube({ 0.0f, 1.02f, 1.16f }, { 0.10f, 1.72f, 0.10f }, white, house_wood);

    draw_cube({ 1.29f, 1.05f, -1.04f }, { 0.12f, 1.7f, 0.12f }, white, house_wood);
    draw_cube({ 1.29f, 1.05f,  1.04f }, { 0.12f, 1.7f, 0.12f }, white, house_wood);
    draw_cube({ 1.29f, 1.88f,  0.0f }, { 0.12f, 0.12f, 2.10f }, white, house_wood);

    draw_cube({ -1.29f, 1.05f, -1.04f }, { 0.12f, 1.7f, 0.12f }, white, house_wood);
    draw_cube({ -1.29f, 1.05f,  1.04f }, { 0.12f, 1.7f, 0.12f }, white, house_wood);
    draw_cube({ -1.29f, 1.88f,  0.0f }, { 0.12f, 0.12f, 2.10f }, white, house_wood);

    // second floor
    if (upstairs)
    {
        draw_cube({ 0.0f, 2.435f, 0.0f }, { 2.5f, 1.22f, 2.2f }, white, house_plaster);

        // strong belt between floors
        draw_cube({ 0.0f, 1.85f,  1.15f }, { 2.64f, 0.13f, 0.15f }, white, house_wood);
        draw_cube({ 0.0f, 1.85f, -1.15f }, { 2.64f, 0.13f, 0.15f }, white, house_wood);
        draw_cube({ 1.29f, 1.85f, 0.0f }, { 0.14f, 0.13f, 2.22f }, white, house_wood);
        draw_cube({ -1.29f, 1.85f, 0.0f }, { 0.14f, 0.13f, 2.22f }, white, house_wood);

        // vertical corner posts second floor
        for (float x : {-1.24f, 1.24f})
        {
            draw_cube({ x, 2.43f,  1.15f }, { 0.12f, 1.22f, 0.12f }, white, house_wood);
            draw_cube({ x, 2.43f, -1.15f }, { 0.12f, 1.22f, 0.12f }, white, house_wood);
        }

        // front upper windows
        for (float x : {-0.62f, 0.62f})
        {
            draw_cube({ x, 2.50f, 1.135f }, { 0.39f, 0.44f, 0.06f }, glm::vec3(0.65f, 0.80f, 0.95f), house_plaster);
            for (float dx : {-0.225f, 0.225f})
                draw_cube({ x + dx, 2.50f, 1.19f }, { 0.055f, 0.54f, 0.08f }, white, house_wood);

            for (float dy : {-0.25f, 0.25f})
                draw_cube({ x, 2.50f + dy, 1.19f }, { 0.51f, 0.055f, 0.08f }, white, house_wood);

            draw_cube({ x, 2.50f, 1.20f }, { 0.045f, 0.49f, 0.045f }, white, house_wood);

            // window sill
            draw_cube({ x, 2.22f, 1.18f }, { 0.56f, 0.05f, 0.10f }, white, house_wood);
        }

        // lower side window on the left wall
        draw_cube({ -1.18f, 1.18f, -0.05f }, { 0.03f, 0.34f, 0.30f }, glm::vec3(0.60f, 0.78f, 0.90f), house_plaster);
        draw_cube({ -1.205f, 1.18f, -0.05f }, { 0.02f, 0.46f, 0.42f }, white, house_wood);
        draw_cube({ -1.215f, 1.18f, -0.05f }, { 0.02f, 0.30f, 0.03f }, white, house_wood);
        draw_cube({ -1.215f, 1.18f, -0.05f }, { 0.02f, 0.03f, 0.26f }, white, house_wood);
        draw_cube({ -1.21f, 0.92f, -0.05f }, { 0.03f, 0.04f, 0.50f }, white, house_wood);

        // upper side window on the left wall
        draw_cube({ -1.18f, 2.50f, 0.15f }, { 0.03f, 0.36f, 0.34f }, glm::vec3(0.65f, 0.80f, 0.95f), house_plaster);
        draw_cube({ -1.205f, 2.50f, 0.15f }, { 0.02f, 0.48f, 0.46f }, white, house_wood);
        draw_cube({ -1.215f, 2.50f, 0.15f }, { 0.02f, 0.32f, 0.03f }, white, house_wood);
        draw_cube({ -1.215f, 2.50f, 0.15f }, { 0.02f, 0.03f, 0.30f }, white, house_wood);
        draw_cube({ -1.21f, 2.23f, 0.15f }, { 0.03f, 0.04f, 0.54f }, white, house_wood);

        // upper side window on the right wall
        draw_cube({ 1.18f, 2.50f, -0.15f }, { 0.03f, 0.36f, 0.34f }, glm::vec3(0.65f, 0.80f, 0.95f), house_plaster);
        draw_cube({ 1.205f, 2.50f, -0.15f }, { 0.02f, 0.48f, 0.46f }, white, house_wood);
        draw_cube({ 1.215f, 2.50f, -0.15f }, { 0.02f, 0.32f, 0.03f }, white, house_wood);
        draw_cube({ 1.215f, 2.50f, -0.15f }, { 0.02f, 0.03f, 0.30f }, white, house_wood);
        draw_cube({ 1.21f, 2.23f, -0.15f }, { 0.03f, 0.04f, 0.54f }, white, house_wood);

        // extra side timber to break flat walls
        draw_cube({ 1.18f, 2.43f, 0.0f }, { 0.08f, 1.18f, 0.08f }, white, house_wood);
        draw_cube({ -1.18f, 2.43f, 0.0f }, { 0.08f, 1.18f, 0.08f }, white, house_wood);

        // small decorative wood strips below roof on both sides
        draw_cube({ 0.0f, 3.05f,  1.02f }, { 2.10f, 0.08f, 0.08f }, white, house_wood);
        draw_cube({ 0.0f, 3.05f, -1.02f }, { 2.10f, 0.08f, 0.08f }, white, house_wood);

        // small support blocks under eaves
        for (float z : {-0.75f, 0.75f})
        {
            draw_cube({ 1.18f, 3.00f, z }, { 0.10f, 0.18f, 0.10f }, white, house_wood);
            draw_cube({ -1.18f, 3.00f, z }, { 0.10f, 0.18f, 0.10f }, white, house_wood);
        }
    }

    // roof
    g_house_transform = glm::translate(base_transform, glm::vec3(0.0f, upper_height, 0.0f));
    draw_gables(house_plaster);
    draw_roof(glm::vec3(1.0f), house_roof);
    g_house_transform = base_transform;

    // chimney
    draw_cube({ 0.85f, 2.68f + upper_height, -0.22f }, { 0.34f, 0.88f, 0.34f }, glm::vec3(0.85f, 0.82f, 0.78f), tex_stone);
    draw_cube({ 0.85f, 3.15f + upper_height, -0.22f }, { 0.42f, 0.08f, 0.42f }, glm::vec3(0.72f, 0.70f, 0.67f), tex_stone);

    // front door
    draw_cube({ 0.0f, 0.70f, 1.16f }, { 0.62f, 1.12f, 0.06f }, white, house_wood);
    draw_cube({ -0.40f, 0.70f, 1.18f }, { 0.08f, 1.20f, 0.08f }, white, house_wood);
    draw_cube({ 0.40f, 0.70f, 1.18f }, { 0.08f, 1.20f, 0.08f }, white, house_wood);
    draw_cube({ 0.0f, 1.28f, 1.18f }, { 0.82f, 0.08f, 0.08f }, white, house_wood);
    draw_cube({ -0.18f, 0.70f, 1.19f }, { 0.05f, 1.08f, 0.02f }, glm::vec3(0.85f, 0.82f, 0.78f), house_wood);
    draw_cube({ 0.18f, 0.70f, 1.19f }, { 0.05f, 1.08f, 0.02f }, glm::vec3(0.85f, 0.82f, 0.78f), house_wood);
    draw_cube({ 0.0f, 0.78f, 1.19f }, { 0.52f, 0.05f, 0.02f }, glm::vec3(0.85f, 0.82f, 0.78f), house_wood);
    draw_cube({ 0.21f, 0.66f, 1.20f }, { 0.04f, 0.04f, 0.03f }, glm::vec3(0.75f, 0.65f, 0.20f), tex_stone);
    draw_cube({ 0.0f, 0.20f, 1.30f }, { 0.90f, 0.10f, 0.24f }, white, tex_stone);

    // little door canopy only for two-story house
    if (upstairs)
    {
        draw_cube({ 0.0f, 1.58f, 1.43f }, { 1.00f, 0.08f, 0.42f }, glm::vec3(0.93f, 0.86f, 0.78f), house_wood);
        draw_cube({ -0.38f, 1.42f, 1.34f }, { 0.07f, 0.26f, 0.07f }, white, house_wood);
        draw_cube({ 0.38f, 1.42f, 1.34f }, { 0.07f, 0.26f, 0.07f }, white, house_wood);
    }

    // front window
    draw_cube({ -0.90f, 1.20f, 1.15f }, { 0.34f, 0.38f, 0.04f }, glm::vec3(0.60f, 0.78f, 0.90f), house_plaster);
    draw_cube({ -1.10f, 1.20f, 1.17f }, { 0.05f, 0.50f, 0.05f }, white, house_wood);
    draw_cube({ -0.70f, 1.20f, 1.17f }, { 0.05f, 0.50f, 0.05f }, white, house_wood);
    draw_cube({ -0.90f, 1.42f, 1.17f }, { 0.45f, 0.05f, 0.05f }, white, house_wood);
    draw_cube({ -0.90f, 0.98f, 1.17f }, { 0.45f, 0.05f, 0.05f }, white, house_wood);
    draw_cube({ -0.90f, 1.20f, 1.18f }, { 0.03f, 0.40f, 0.02f }, white, house_wood);
    draw_cube({ -0.90f, 1.20f, 1.18f }, { 0.32f, 0.03f, 0.02f }, white, house_wood);
    draw_cube({ -0.90f, 0.90f, 1.18f }, { 0.55f, 0.04f, 0.10f }, white, house_wood);
    draw_cube({ -1.26f, 1.20f, 1.14f }, { 0.10f, 0.48f, 0.03f }, white, house_wood);
    draw_cube({ -0.54f, 1.20f, 1.14f }, { 0.10f, 0.48f, 0.03f }, white, house_wood);

    // standard side window on right wall
    draw_cube({ 1.18f, 1.20f, 0.0f }, { 0.03f, 0.36f, 0.32f }, glm::vec3(0.60f, 0.78f, 0.90f), house_plaster);
    draw_cube({ 1.205f, 1.20f, 0.0f }, { 0.02f, 0.48f, 0.44f }, white, house_wood);
    draw_cube({ 1.215f, 1.20f, 0.0f }, { 0.02f, 0.38f, 0.03f }, white, house_wood);
    draw_cube({ 1.215f, 1.20f, 0.0f }, { 0.02f, 0.03f, 0.28f }, white, house_wood);
    draw_cube({ 1.21f, 0.92f, 0.0f }, { 0.03f, 0.04f, 0.52f }, white, house_wood);

    g_house_transform = glm::mat4(1.0f);
}

// Village layout. Positions, heights, scales and angles are exactly the
// manually adjusted values from the supplied main.cpp.
struct VillageHouse
{
    glm::vec3 position;
    float scale;
    float yaw;
};

// The front door is on local +Z. Convert it to world coordinates so that
// roads always reach the actual entrance after rotating/scaling a house.
static glm::vec2 house_door(const VillageHouse& h)
{
    const float r = glm::radians(h.yaw);
    const float d = 1.10f * h.scale;
    return glm::vec2(h.position.x + std::sin(r) * d,
                     h.position.z + std::cos(r) * d);
}

// Build a curved ribbon of triangles directly on the terrain. All paths are
// combined in ONE VAO; no hundreds of cubes or one draw call per paving stone.
static void add_road_curve(std::vector<float>& mesh,
                           glm::vec2 a, glm::vec2 control, glm::vec2 b,
                           float width = 0.83f)
{
    const float approx_length = glm::length(control-a) + glm::length(b-control);
    const int steps = std::max(12, (int)std::ceil(approx_length / 0.18f));
    auto point = [&](float t) -> glm::vec2 {
        const float u = 1.0f - t;
        return u*u*a + 2.0f*u*t*control + t*t*b;
    };
    auto tangent = [&](float t) -> glm::vec2 {
        glm::vec2 d = 2.0f*(1.0f-t)*(control-a) + 2.0f*t*(b-control);
        if (glm::dot(d,d) < 0.00001f) d = b-a;
        return glm::normalize(d);
    };
    auto vertex = [&](glm::vec2 p, float u, float v) -> glm::vec3 {
        return glm::vec3(p.x, village_terrain::ground_height(p.x,p.y)+0.075f, p.y);
    };
    float travelled = 0.0f;
    for (int i=0; i<steps; ++i)
    {
        const float t0 = (float)i/steps;
        const float t1 = (float)(i+1)/steps;
        const glm::vec2 p0 = point(t0), p1 = point(t1);
        const glm::vec2 d0 = tangent(t0), d1 = tangent(t1);
        const glm::vec2 n0(-d0.y,d0.x), n1(-d1.y,d1.x);
        const float v0 = travelled / 0.72f;
        travelled += glm::length(p1-p0);
        const float v1 = travelled / 0.72f;
        const glm::vec3 l0 = vertex(p0+n0*width*0.5f,0,v0);
        const glm::vec3 r0 = vertex(p0-n0*width*0.5f,1,v0);
        const glm::vec3 l1 = vertex(p1+n1*width*0.5f,0,v1);
        const glm::vec3 r1 = vertex(p1-n1*width*0.5f,1,v1);
        add_triangle(mesh,l0,r0,r1,{0,v0},{1,v0},{1,v1},{0,1,0});
        add_triangle(mesh,l0,r1,l1,{0,v0},{1,v1},{0,v1},{0,1,0});
    }
}

// Round joins smooth over tiny cracks where road ribbons meet.
static void add_path_junction(std::vector<float>& mesh, glm::vec2 center,
                              float radius = 0.47f)
{
    const int slices = 16;
    const auto vertex = [&](glm::vec2 p) {
        return glm::vec3(p.x, village_terrain::ground_height(p.x, p.y) + 0.080f, p.y);
    };
    const glm::vec3 c = vertex(center);
    for (int i = 0; i < slices; ++i)
    {
        float a0 = (2.0f * 3.14159265f * i) / slices;
        float a1 = (2.0f * 3.14159265f * (i + 1)) / slices;
        glm::vec2 p0 = center + radius * glm::vec2(std::cos(a0),std::sin(a0));
        glm::vec2 p1 = center + radius * glm::vec2(std::cos(a1),std::sin(a1));
        add_triangle(mesh, c, vertex(p0), vertex(p1),
            {0.5f,0.5f}, {0.5f+0.5f*std::cos(a0),0.5f+0.5f*std::sin(a0)},
            {0.5f+0.5f*std::cos(a1),0.5f+0.5f*std::sin(a1)}, {0,1,0});
    }
}

static void init_village_paths(const std::array<VillageHouse,5>& houses)
{
    std::vector<float> mesh;
    const float bx = village_terrain::river_x(0.0f);
    auto road = [&](glm::vec2 a,glm::vec2 control,glm::vec2 b,float width=0.83f) {
        add_road_curve(mesh,a,control,b,width);
    };
    // Main route: the central house and upper junction to the bridge.
    const glm::vec2 upper(-0.8f,4.1f);
    const glm::vec2 middle(2.15f,2.25f);
    const glm::vec2 lower(-2.8f,-1.5f);
    const glm::vec2 lower_east(2.2f,-1.7f);
    const glm::vec2 bridge_left(bx-2.10f,0.0f);
    const glm::vec2 bridge_right(bx+2.10f,0.0f);
    const glm::vec2 east_hub(9.0f,0.0f);

    road(house_door(houses[0]),glm::vec2(-1.6f,3.68f),upper);
    road(upper,glm::vec2(0.7f,4.0f),middle);
    road(middle,glm::vec2(3.2f,0.65f),bridge_left);
    // Upper-left and lower-left houses reach the network without crossing
    // through the central house.
    road(house_door(houses[2]),glm::vec2(-4.6f,4.7f),glm::vec2(-3.0f,4.05f));
    road(glm::vec2(-3.0f,4.05f),glm::vec2(-1.9f,4.1f),upper);
    // New watchtower approach. The tower entrance is on its local +Z side.
    // Connect it to the existing upper-west junction, away from house footprints.
    const glm::vec2 tower_door(-11.6f, 1.80f);
    const glm::vec2 tower_bend(-8.9f, 2.10f);
    const glm::vec2 tower_join(-3.0f, 4.05f);
    road(tower_door, glm::vec2(-10.40f, 2.10f), tower_bend, 0.76f);
    road(tower_bend, glm::vec2(-6.50f, 3.15f), tower_join, 0.76f);

    road(house_door(houses[1]),glm::vec2(-4.5f,-2.3f),lower);
    road(lower,glm::vec2(-0.4f,-2.2f),lower_east);
    road(lower_east,glm::vec2(3.65f,-1.45f),bridge_left);
    // On the opposite bank, each road ends at the rotated front door.
    road(bridge_right,glm::vec2(8.3f,0.0f),east_hub);
    road(east_hub,glm::vec2(9.0f,-1.55f),house_door(houses[3]));
    road(east_hub,glm::vec2(9.0f,1.1f),house_door(houses[4]));

    // Fill the meeting points so curves join like one continuous road.
    for (glm::vec2 junction : {upper, middle, lower, lower_east,
                              bridge_left, bridge_right, east_hub,
                              glm::vec2(-3.0f,4.05f),
                              glm::vec2(-8.9f,2.10f)})
        add_path_junction(mesh, junction);

    g_paths_vao = upload_mesh(mesh,g_paths_vertex_count);
    glBindVertexArray(g_cube_vao);
}

static void draw_village_paths()
{
    g_model = glm::mat4(1.0f);
    set_model(g_program);
    set_vec3(g_program,glm::vec3(1.0f),"u_tint");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,tex_stone);
    glBindVertexArray(g_paths_vao);
    glDrawArrays(GL_TRIANGLES,0,g_paths_vertex_count);
    glBindVertexArray(g_cube_vao);
}

static void render(void)
{
    glUseProgram(g_program);
    // Animate the river texture with elapsed time (no geometry movement).
    glUniform1f(glGetUniformLocation(g_program, "u_time"),
                static_cast<float>(glfwGetTime()));
    glUniform1i(glGetUniformLocation(g_program, "u_is_water"), 0);
    g_house_transform = glm::mat4(1.0f);
    village_terrain::draw(g_program,tex_grass,tex_water);
    glBindVertexArray(g_cube_vao);
    draw_bridge();

    // The four outer houses keep their manually adjusted coordinates.
    // Only the central house moves left to open the narrow passage.
    const std::array<VillageHouse,5> houses = {{
        {glm::vec3(-2.10f,0.0f,1.50f),1.0f,0.0f},
        {glm::vec3(-6.5f,village_terrain::ground_height(-6.0f,-5.0f),-4.0f),0.88f,60.0f},
        {glm::vec3(-6.5f,village_terrain::ground_height(-7.5f,5.0f),6.0f),1.08f,112.0f},
        {glm::vec3(10.2f,village_terrain::ground_height(2.5f,-12.5f),-4.0f),0.92f,342.0f},
        {glm::vec3(10.5f,village_terrain::ground_height(10.5f,5.0f),3.0f),1.02f,202.0f}
    }};

    // Upload only once, not every frame.
    if (g_paths_vao == 0) init_village_paths(houses);
    draw_village_paths();
    // Reuse the same user-positioned houses; only their local geometry varies.
    const int building_types[5] = {4, 1, 0, 2, 3};
    for (size_t i = 0; i < houses.size(); ++i)
    {
        if (building_types[i] == 2) continue; // replaced by imported Blacksmith.obj
        draw_house(houses[i].position, houses[i].scale, houses[i].yaw,
                   building_types[i]);
    }
    draw_imported_blacksmith(houses[3].position, houses[3].yaw);

    // Separate landmark beyond the village houses, on the western hill.
    draw_imported_watchtower();

    // Reuse the same imported nature assets at varied locations.
    draw_forest();
    draw_rocks();
    draw_bushes();
    draw_grass_tufts();
}



// -------- Blockbench OBJ tower (kept separate from procedural house geometry) --------
struct ImportedTowerPart
{
    unsigned int vao = 0;
    unsigned int count = 0;
    unsigned int texture = 0;
};
static std::vector<ImportedTowerPart> g_imported_tower_parts;
static bool g_imported_tower_ready = false;

static std::string tower_trim(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b-a+1);
}

static bool load_imported_watchtower()
{
    if (g_imported_tower_ready) return !g_imported_tower_parts.empty();
    g_imported_tower_ready = true;
    const std::string dir = "resources/models/watchtower/";
    std::ifstream source(dir + "Medieval_Watchtower.obj");
    if (!source)
    {
        std::cerr << "[Watchtower] OBJ not found at "
                  << dir << "Medieval_Watchtower.obj (working directory?)\n";
        return false;
    }

    // Read the exported material table, including all distinct image files.
    std::unordered_map<std::string, std::string> material_images;
    {
        std::ifstream mtl(dir + "Medieval_Watchtower.mtl");
        if (!mtl) std::cerr << "[Watchtower] MTL missing!\n";
        std::string line, active;
        while (std::getline(mtl, line))
        {
            std::istringstream stream(line);
            std::string tag; stream >> tag;
            if (tag == "newmtl") stream >> active;
            else if (tag == "map_Kd" && !active.empty())
            {
                std::string image; std::getline(stream, image);
                image = tower_trim(image);
                // Restrict texture lookup to the model folder, not absolute paths.
                size_t slash = image.find_last_of("/\\");
                if (slash != std::string::npos) image = image.substr(slash+1);
                material_images[active] = image;
            }
        }
    }

    struct Index { int p=0, t=0, n=0; };
    auto parse_index = [](const std::string& token) {
        Index a;
        // OBJ format: position/texcoord/normal; accepts absent values.
        std::string parts[3]; size_t st=0;
        for (int k=0;k<3;++k)
        {
            size_t sep = token.find('/', st);
            parts[k] = token.substr(st, sep==std::string::npos ? sep : sep-st);
            if (sep == std::string::npos) break;
            st = sep+1;
        }
        if (!parts[0].empty()) a.p=std::stoi(parts[0]);
        if (!parts[1].empty()) a.t=std::stoi(parts[1]);
        if (!parts[2].empty()) a.n=std::stoi(parts[2]);
        return a;
    };
    auto index_of = [](int obj_index, size_t count) -> int {
        if (obj_index>0) return obj_index-1;
        if (obj_index<0) return static_cast<int>(count)+obj_index;
        return -1;
    };
    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> uvs;
    std::unordered_map<std::string, std::vector<float>> by_material;
    std::vector<std::string> material_order;
    std::string active="none", line;
    size_t triangles=0;
    try
    {
        while (std::getline(source, line))
        {
            std::istringstream s(line);
            std::string tag; s >> tag;
            if (tag=="v")
            {
                glm::vec3 p; s >> p.x >> p.y >> p.z;
                positions.push_back(p);
            }
            else if (tag=="vt")
            {
                glm::vec2 uv; s >> uv.x >> uv.y;
                uvs.push_back(uv);
            }
            else if (tag=="vn")
            {
                glm::vec3 n; s >> n.x >> n.y >> n.z;
                normals.push_back(n);
            }
            else if (tag=="usemtl") s >> active;
            else if (tag=="f")
            {
                std::vector<Index> polygon;
                std::string v;
                while (s >> v) polygon.push_back(parse_index(v));
                if (polygon.size() < 3) continue;
                if (by_material.find(active)==by_material.end())
                    material_order.push_back(active);
                auto& dst = by_material[active];
                for (size_t i=1; i+1<polygon.size(); ++i)
                {
                    Index face[3] = {polygon[0],polygon[i],polygon[i+1]};
                    glm::vec3 p[3];
                    bool valid=true;
                    for (int j=0;j<3;++j)
                    {
                        int idx=index_of(face[j].p,positions.size());
                        if (idx<0 || static_cast<size_t>(idx)>=positions.size()) {valid=false;break;}
                        p[j]=positions[idx];
                    }
                    if (!valid) continue;
                    glm::vec3 flat = glm::cross(p[1]-p[0],p[2]-p[0]);
                    if (glm::dot(flat,flat)>1e-12f) flat=glm::normalize(flat);
                    else flat=glm::vec3(0,1,0);
                    for (int j=0;j<3;++j)
                    {
                        glm::vec3 n=flat;
                        int ni=index_of(face[j].n,normals.size());
                        if (ni>=0 && static_cast<size_t>(ni)<normals.size()) n=normals[ni];
                        glm::vec2 uv(0,0);
                        int ti=index_of(face[j].t,uvs.size());
                        if (ti>=0 && static_cast<size_t>(ti)<uvs.size()) uv=uvs[ti];
                        // Blockbench OBJ 'vt' is already in OpenGL convention.
                        dst.insert(dst.end(),{p[j].x,p[j].y,p[j].z,n.x,n.y,n.z,uv.x,uv.y});
                    }
                    ++triangles;
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Watchtower] OBJ parse error: " << e.what() << "\n";
        return false;
    }
    std::unordered_map<std::string,unsigned int> texture_cache;
    for (const auto& material : material_order)
    {
        auto it=by_material.find(material);
        if (it==by_material.end() || it->second.empty()) continue;
        unsigned int count=0;
        unsigned int vao=upload_mesh(it->second,count);
        unsigned int texture=tex_stone; // fallback only if export has no texture
        auto img=material_images.find(material);
        if (img!=material_images.end() && !img->second.empty())
        {
            auto existing=texture_cache.find(img->second);
            if (existing!=texture_cache.end()) texture=existing->second;
            else
            {
                unsigned int t=init_texture(dir+img->second);
                if (t) texture=t;
                else std::cerr << "[Watchtower] Missing texture: " << img->second << "\n";
                texture_cache[img->second]=texture;
            }
        }
        g_imported_tower_parts.push_back({vao,count,texture});
    }
    std::cout << "[Watchtower] Imported " << triangles << " triangles, "
              << g_imported_tower_parts.size() << " material batches, "
              << texture_cache.size() << " PNG textures\n";
    return !g_imported_tower_parts.empty();
}

// Imported Blockbench blacksmith. Uses the same tested OBJ/MTL parsing
// strategy as the watchtower, but independent VAOs and material batches.
static std::vector<ImportedTowerPart> g_imported_blacksmith_parts;
static bool g_imported_blacksmith_ready = false;

static bool load_imported_blacksmith()
{
    if (g_imported_blacksmith_ready) return !g_imported_blacksmith_parts.empty();
    g_imported_blacksmith_ready = true;
    const std::string dir = "resources/models/blacksmith/";
    std::ifstream source(dir + "Blacksmith.obj");
    if (!source)
    {
        std::cerr << "[Blacksmith] OBJ not found at "
                  << dir << "Blacksmith.obj (working directory?)\n";
        return false;
    }

    // Read the exported material table, including all distinct image files.
    std::unordered_map<std::string, std::string> material_images;
    {
        std::ifstream mtl(dir + "Blacksmith.mtl");
        if (!mtl) std::cerr << "[Blacksmith] MTL missing!\n";
        std::string line, active;
        while (std::getline(mtl, line))
        {
            std::istringstream stream(line);
            std::string tag; stream >> tag;
            if (tag == "newmtl") stream >> active;
            else if (tag == "map_Kd" && !active.empty())
            {
                std::string image; std::getline(stream, image);
                image = tower_trim(image);
                // Restrict texture lookup to the model folder, not absolute paths.
                size_t slash = image.find_last_of("/\\");
                if (slash != std::string::npos) image = image.substr(slash+1);
                material_images[active] = image;
            }
        }
    }

    struct Index { int p=0, t=0, n=0; };
    auto parse_index = [](const std::string& token) {
        Index a;
        // OBJ format: position/texcoord/normal; accepts absent values.
        std::string parts[3]; size_t st=0;
        for (int k=0;k<3;++k)
        {
            size_t sep = token.find('/', st);
            parts[k] = token.substr(st, sep==std::string::npos ? sep : sep-st);
            if (sep == std::string::npos) break;
            st = sep+1;
        }
        if (!parts[0].empty()) a.p=std::stoi(parts[0]);
        if (!parts[1].empty()) a.t=std::stoi(parts[1]);
        if (!parts[2].empty()) a.n=std::stoi(parts[2]);
        return a;
    };
    auto index_of = [](int obj_index, size_t count) -> int {
        if (obj_index>0) return obj_index-1;
        if (obj_index<0) return static_cast<int>(count)+obj_index;
        return -1;
    };
    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> uvs;
    std::unordered_map<std::string, std::vector<float>> by_material;
    std::vector<std::string> material_order;
    std::string active="none", line;
    size_t triangles=0;
    try
    {
        while (std::getline(source, line))
        {
            std::istringstream s(line);
            std::string tag; s >> tag;
            if (tag=="v")
            {
                glm::vec3 p; s >> p.x >> p.y >> p.z;
                positions.push_back(p);
            }
            else if (tag=="vt")
            {
                glm::vec2 uv; s >> uv.x >> uv.y;
                uvs.push_back(uv);
            }
            else if (tag=="vn")
            {
                glm::vec3 n; s >> n.x >> n.y >> n.z;
                normals.push_back(n);
            }
            else if (tag=="usemtl") s >> active;
            else if (tag=="f")
            {
                std::vector<Index> polygon;
                std::string v;
                while (s >> v) polygon.push_back(parse_index(v));
                if (polygon.size() < 3) continue;
                if (by_material.find(active)==by_material.end())
                    material_order.push_back(active);
                auto& dst = by_material[active];
                for (size_t i=1; i+1<polygon.size(); ++i)
                {
                    Index face[3] = {polygon[0],polygon[i],polygon[i+1]};
                    glm::vec3 p[3];
                    bool valid=true;
                    for (int j=0;j<3;++j)
                    {
                        int idx=index_of(face[j].p,positions.size());
                        if (idx<0 || static_cast<size_t>(idx)>=positions.size()) {valid=false;break;}
                        p[j]=positions[idx];
                    }
                    if (!valid) continue;
                    glm::vec3 flat = glm::cross(p[1]-p[0],p[2]-p[0]);
                    if (glm::dot(flat,flat)>1e-12f) flat=glm::normalize(flat);
                    else flat=glm::vec3(0,1,0);
                    for (int j=0;j<3;++j)
                    {
                        glm::vec3 n=flat;
                        int ni=index_of(face[j].n,normals.size());
                        if (ni>=0 && static_cast<size_t>(ni)<normals.size()) n=normals[ni];
                        glm::vec2 uv(0,0);
                        int ti=index_of(face[j].t,uvs.size());
                        if (ti>=0 && static_cast<size_t>(ti)<uvs.size()) uv=uvs[ti];
                        // Blockbench OBJ 'vt' is already in OpenGL convention.
                        dst.insert(dst.end(),{p[j].x,p[j].y,p[j].z,n.x,n.y,n.z,uv.x,uv.y});
                    }
                    ++triangles;
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Blacksmith] OBJ parse error: " << e.what() << "\n";
        return false;
    }
    std::unordered_map<std::string,unsigned int> texture_cache;
    for (const auto& material : material_order)
    {
        auto it=by_material.find(material);
        if (it==by_material.end() || it->second.empty()) continue;
        unsigned int count=0;
        unsigned int vao=upload_mesh(it->second,count);
        unsigned int texture=tex_stone; // fallback only if export has no texture
        auto img=material_images.find(material);
        if (img!=material_images.end() && !img->second.empty())
        {
            auto existing=texture_cache.find(img->second);
            if (existing!=texture_cache.end()) texture=existing->second;
            else
            {
                unsigned int t=init_texture(dir+img->second);
                if (t) texture=t;
                else std::cerr << "[Blacksmith] Missing texture: " << img->second << "\n";
                texture_cache[img->second]=texture;
            }
        }
        g_imported_blacksmith_parts.push_back({vao,count,texture});
    }
    std::cout << "[Blacksmith] Imported " << triangles << " triangles, "
              << g_imported_blacksmith_parts.size() << " material batches, "
              << texture_cache.size() << " PNG textures\n";
    return !g_imported_blacksmith_parts.empty();
}

// -------- Medieval village forest (three cached Blockbench-compatible OBJ assets) --------
// Materials are looked up next to each OBJ. Missing models do not break the village.
struct VillageTreeAsset
{
    bool attempted = false;
    std::vector<ImportedTowerPart> parts;
};
static VillageTreeAsset g_tree_assets[3];

static bool load_tree_asset(VillageTreeAsset& asset, const std::string& name)
{
    if (asset.attempted) return !asset.parts.empty();
    asset.attempted = true;
    const std::string folder = "resources/models/trees/" + name + "/";
    std::ifstream obj(folder + name + ".obj");
    if (!obj)
    {
        std::cerr << "[Trees] Missing " << folder << name << ".obj\n";
        return false;
    }

    std::unordered_map<std::string, std::string> image_for_material;
    {
        std::ifstream mtl(folder + name + ".mtl");
        if (!mtl) std::cerr << "[Trees] Missing MTL for " << name << "\n";
        std::string line, material;
        while (std::getline(mtl, line))
        {
            std::istringstream s(line);
            std::string tag; s >> tag;
            if (tag == "newmtl") s >> material;
            else if (tag == "map_Kd" && !material.empty())
            {
                std::string image; std::getline(s, image);
                image = tower_trim(image);
                size_t slash = image.find_last_of("/\\");
                if (slash != std::string::npos) image = image.substr(slash + 1);
                image_for_material[material] = image;
            }
        }
    }

    struct Ref { int p = 0, t = 0, n = 0; };
    auto parse_ref = [](const std::string& token) -> Ref
    {
        Ref r;
        std::string p[3];
        size_t start = 0;
        for (int j = 0; j < 3; ++j)
        {
            size_t split = token.find('/', start);
            p[j] = token.substr(start, split == std::string::npos ? split : split - start);
            if (split == std::string::npos) break;
            start = split + 1;
        }
        if (!p[0].empty()) r.p = std::stoi(p[0]);
        if (!p[1].empty()) r.t = std::stoi(p[1]);
        if (!p[2].empty()) r.n = std::stoi(p[2]);
        return r;
    };
    auto idx = [](int n, size_t count) -> int
    {
        if (n > 0) return n - 1;
        if (n < 0) return static_cast<int>(count) + n;
        return -1;
    };

    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> uvs;
    std::unordered_map<std::string, std::vector<float>> meshes;
    std::vector<std::string> material_order;
    std::string active = "none", line;
    size_t triangles = 0;
    try
    {
        while (std::getline(obj, line))
        {
            std::istringstream s(line);
            std::string tag; s >> tag;
            if (tag == "v")
            {
                glm::vec3 p; s >> p.x >> p.y >> p.z;
                positions.push_back(p);
            }
            else if (tag == "vt")
            {
                glm::vec2 uv; s >> uv.x >> uv.y;
                uvs.push_back(uv);
            }
            else if (tag == "vn")
            {
                glm::vec3 n; s >> n.x >> n.y >> n.z;
                normals.push_back(n);
            }
            else if (tag == "usemtl") s >> active;
            else if (tag == "f")
            {
                std::vector<Ref> face;
                std::string token;
                while (s >> token) face.push_back(parse_ref(token));
                if (face.size() < 3) continue;
                if (meshes.find(active) == meshes.end()) material_order.push_back(active);
                auto& out = meshes[active];
                for (size_t i = 1; i + 1 < face.size(); ++i)
                {
                    Ref tri[3] = {face[0], face[i], face[i + 1]};
                    glm::vec3 p[3];
                    bool valid = true;
                    for (int j = 0; j < 3; ++j)
                    {
                        int index = idx(tri[j].p, positions.size());
                        if (index < 0 || static_cast<size_t>(index) >= positions.size())
                        { valid = false; break; }
                        p[j] = positions[index];
                    }
                    if (!valid) continue;
                    glm::vec3 n = glm::cross(p[1] - p[0], p[2] - p[0]);
                    n = glm::dot(n, n) > 1e-10f ? glm::normalize(n) : glm::vec3(0, 1, 0);
                    for (int j = 0; j < 3; ++j)
                    {
                        int ni = idx(tri[j].n, normals.size());
                        int ti = idx(tri[j].t, uvs.size());
                        glm::vec3 normal = (ni >= 0 && static_cast<size_t>(ni) < normals.size()) ? normals[ni] : n;
                        glm::vec2 uv = (ti >= 0 && static_cast<size_t>(ti) < uvs.size()) ? uvs[ti] : glm::vec2(0);
                        out.insert(out.end(), {p[j].x, p[j].y, p[j].z,
                                               normal.x, normal.y, normal.z, uv.x, uv.y});
                    }
                    ++triangles;
                }
            }
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "[Trees] OBJ parse error for " << name << ": " << error.what() << "\n";
        return false;
    }

    std::unordered_map<std::string, unsigned int> texture_cache;
    for (const std::string& material : material_order)
    {
        auto vertices = meshes.find(material);
        if (vertices == meshes.end() || vertices->second.empty()) continue;
        unsigned int count = 0;
        unsigned int vao = upload_mesh(vertices->second, count);
        unsigned int texture = tex_wood;
        auto it = image_for_material.find(material);
        if (it != image_for_material.end() && !it->second.empty())
        {
            auto saved = texture_cache.find(it->second);
            if (saved != texture_cache.end()) texture = saved->second;
            else
            {
                unsigned int loaded = init_texture(folder + it->second);
                if (loaded) texture = loaded;
                else std::cerr << "[Trees] Texture missing: " << folder << it->second << "\n";
                texture_cache[it->second] = texture;
            }
        }
        asset.parts.push_back({vao, count, texture});
    }
    std::cout << "[Trees] " << name << ": " << triangles << " triangles, "
              << asset.parts.size() << " material batches\n";
    return !asset.parts.empty();
}

static bool near_river(float x, float z, float margin)
{
    const float dx = std::abs(x - village_terrain::river_x(z));
    return dx < (1.22f + margin);
}

// Fixed placements keep the village roads, bridge, house doors, and river open.
// scale is relative to OBJ size (Oak 5.12, Birch 5.67, Pine 6.05 units tall).
static void draw_forest()
{
    static const struct TreePosition
    {
        int species;  // 0 Oak, 1 Birch, 2 Pine
        float x, z, scale, yaw;
    } trees[] = {
        {0, -12.9f, -10.7f, 0.76f,  12.0f},
        {0,  -9.4f,  12.0f, 0.70f,  42.0f},
        {0,  -4.0f, -11.6f, 0.81f,  85.0f},
        {0,   0.7f,  12.5f, 0.74f,  25.0f},
        {0,  13.0f,  10.4f, 0.74f, 150.0f},
        {0,  12.9f, -10.7f, 0.77f,  45.0f},
        {0,  14.3f,   0.6f, 0.62f,  70.0f},
        {1, -13.2f,  -3.8f, 0.70f,   0.0f},
        {1,  -5.4f,  13.4f, 0.73f,  61.0f},
        {1,   1.3f,  -8.3f, 0.76f,  86.0f},
        {1,   8.7f,  12.1f, 0.77f,  34.0f},
        {1,   9.2f, -13.0f, 0.70f, 103.0f},
        {1,  14.1f,  -5.1f, 0.73f,  80.0f},
        {2, -13.1f,   8.9f, 0.76f,  52.0f},
        {2, -11.0f, -13.0f, 0.70f,  23.0f},
        {2,  -1.4f,  14.0f, 0.72f,  92.0f},
        {2,   1.7f, -13.9f, 0.71f,  30.0f},
        {2,  13.8f,   7.2f, 0.73f,  56.0f},
        {2,  13.0f, -13.9f, 0.79f,  17.0f},
        // a few extra trees in the central open areas for more life
        {0,  -0.8f,   7.4f, 0.60f,  28.0f},
        {0,   4.1f,   5.1f, 0.62f, 111.0f},
        {1,  -2.8f,  -8.4f, 0.66f,  21.0f},
        {2,   4.9f,  -6.8f, 0.62f, 137.0f}
    };
    static const char* species_names[] = {"Oak", "Birch", "Pine"};
    for (int i = 0; i < 3; ++i) load_tree_asset(g_tree_assets[i], species_names[i]);

    set_vec3(g_program, glm::vec3(1.0f), "u_tint");
    glActiveTexture(GL_TEXTURE0);
    for (const auto& tree : trees)
    {
        const VillageTreeAsset& asset = g_tree_assets[tree.species];
        if (asset.parts.empty() || near_river(tree.x, tree.z, 0.85f)) continue;
        float y = village_terrain::ground_height(tree.x, tree.z);
        g_model = glm::translate(glm::mat4(1.0f), glm::vec3(tree.x, y, tree.z));
        g_model = glm::rotate(g_model, glm::radians(tree.yaw), glm::vec3(0,1,0));
        g_model = glm::scale(g_model, glm::vec3(tree.scale));
        set_model(g_program);
        for (const auto& part : asset.parts)
        {
            glBindTexture(GL_TEXTURE_2D, part.texture);
            glBindVertexArray(part.vao);
            glDrawArrays(GL_TRIANGLES, 0, part.count);
        }
    }
    glBindVertexArray(g_cube_vao);
    g_model = glm::mat4(1.0f);
}
// -------- End forest --------

// -------- Decorative rocks and swaying grass --------
struct VillageRockAsset
{
    bool attempted = false;
    std::vector<ImportedTowerPart> parts;
};
static VillageRockAsset g_rock_assets[2];

static bool load_rock_asset(VillageRockAsset& asset, const std::string& name)
{
    if (asset.attempted) return !asset.parts.empty();
    asset.attempted = true;
    const std::string folder = "resources/models/rocks/" + name + "/";
    std::ifstream obj(folder + name + ".obj");
    if (!obj)
    {
        std::cerr << "[Rocks] Missing " << folder << name << ".obj\n";
        return false;
    }

    std::unordered_map<std::string, std::string> image_for_material;
    {
        std::ifstream mtl(folder + name + ".mtl");
        std::string line, material;
        while (std::getline(mtl, line))
        {
            std::istringstream s(line);
            std::string tag; s >> tag;
            if (tag == "newmtl") s >> material;
            else if (tag == "map_Kd" && !material.empty())
            {
                std::string image; std::getline(s, image);
                image = tower_trim(image);
                size_t slash = image.find_last_of("/\\");
                if (slash != std::string::npos) image = image.substr(slash + 1);
                image_for_material[material] = image;
            }
        }
    }

    struct Ref { int p = 0, t = 0, n = 0; };
    auto parse_ref = [](const std::string& token) -> Ref
    {
        Ref r; std::string p[3]; size_t start = 0;
        for (int j = 0; j < 3; ++j)
        {
            size_t split = token.find('/', start);
            p[j] = token.substr(start, split == std::string::npos ? split : split - start);
            if (split == std::string::npos) break;
            start = split + 1;
        }
        if (!p[0].empty()) r.p = std::stoi(p[0]);
        if (!p[1].empty()) r.t = std::stoi(p[1]);
        if (!p[2].empty()) r.n = std::stoi(p[2]);
        return r;
    };
    auto idx = [](int n, size_t count) -> int
    {
        if (n > 0) return n - 1;
        if (n < 0) return static_cast<int>(count) + n;
        return -1;
    };

    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> uvs;
    std::unordered_map<std::string, std::vector<float>> meshes;
    std::vector<std::string> material_order;
    std::string active = "none", line;
    while (std::getline(obj, line))
    {
        std::istringstream s(line);
        std::string tag; s >> tag;
        if (tag == "v")
        {
            glm::vec3 p; s >> p.x >> p.y >> p.z; positions.push_back(p);
        }
        else if (tag == "vt")
        {
            glm::vec2 uv; s >> uv.x >> uv.y; uvs.push_back(uv);
        }
        else if (tag == "vn")
        {
            glm::vec3 n; s >> n.x >> n.y >> n.z; normals.push_back(n);
        }
        else if (tag == "usemtl") s >> active;
        else if (tag == "f")
        {
            std::vector<Ref> face; std::string token;
            while (s >> token) face.push_back(parse_ref(token));
            if (face.size() < 3) continue;
            if (meshes.find(active) == meshes.end()) material_order.push_back(active);
            auto& out = meshes[active];
            for (size_t i = 1; i + 1 < face.size(); ++i)
            {
                Ref tri[3] = {face[0], face[i], face[i + 1]};
                glm::vec3 p[3]; bool valid = true;
                for (int j = 0; j < 3; ++j)
                {
                    int index = idx(tri[j].p, positions.size());
                    if (index < 0 || static_cast<size_t>(index) >= positions.size())
                    { valid = false; break; }
                    p[j] = positions[index];
                }
                if (!valid) continue;
                glm::vec3 n = glm::cross(p[1] - p[0], p[2] - p[0]);
                n = glm::dot(n, n) > 1e-10f ? glm::normalize(n) : glm::vec3(0, 1, 0);
                for (int j = 0; j < 3; ++j)
                {
                    int ni = idx(tri[j].n, normals.size());
                    int ti = idx(tri[j].t, uvs.size());
                    glm::vec3 normal = (ni >= 0 && static_cast<size_t>(ni) < normals.size()) ? normals[ni] : n;
                    glm::vec2 uv = (ti >= 0 && static_cast<size_t>(ti) < uvs.size()) ? uvs[ti] : glm::vec2(0);
                    out.insert(out.end(), {p[j].x, p[j].y, p[j].z, normal.x, normal.y, normal.z, uv.x, uv.y});
                }
            }
        }
    }

    std::unordered_map<std::string, unsigned int> texture_cache;
    for (const std::string& material : material_order)
    {
        auto vertices = meshes.find(material);
        if (vertices == meshes.end() || vertices->second.empty()) continue;
        unsigned int count = 0;
        unsigned int vao = upload_mesh(vertices->second, count);
        unsigned int texture = tex_stone;
        auto it = image_for_material.find(material);
        if (it != image_for_material.end() && !it->second.empty())
        {
            auto saved = texture_cache.find(it->second);
            if (saved != texture_cache.end()) texture = saved->second;
            else
            {
                unsigned int loaded = init_texture(folder + it->second);
                if (loaded)
                {
                    texture = loaded;
                    std::cout << "[Rocks] " << name << " texture loaded: "
                              << folder + it->second << "\n";
                }
                else
                    std::cerr << "[Rocks] Missing texture: "
                              << folder + it->second << "\n";
                texture_cache[it->second] = texture;
            }
        }
        asset.parts.push_back({vao, count, texture});
    }
    if (!asset.parts.empty())
        std::cout << "[Rocks] " << name << ": " << asset.parts.size()
                  << " material batch(es) ready\n";
    return !asset.parts.empty();
}

static void draw_rocks()
{
    static const char* names[] = {"RockA", "RockB"};
    for (int i = 0; i < 2; ++i) load_rock_asset(g_rock_assets[i], names[i]);

    static const struct RockPlacement
    {
        int variant; float x, z, scale, yaw;
    } rocks[] = {
        {0, -10.8f,  4.7f, 0.78f,  12.0f},
        {1,  -8.9f,  9.3f, 0.64f,  73.0f},
        {0,  -4.8f, -9.1f, 0.58f, 130.0f},
        {1,  -1.9f, 10.8f, 0.71f,  18.0f},
        {1,   2.6f,  8.4f, 0.60f,  92.0f},
        {0,   5.1f, -8.2f, 0.66f, 156.0f},
        {1,   7.4f, -6.4f, 0.57f,  37.0f},
        {0,   8.9f,  6.8f, 0.84f, 142.0f},
        {1,  11.3f,  8.8f, 0.62f,  49.0f},
        {0,  12.1f, -2.6f, 0.69f, 101.0f},
        {1,  -7.7f,  1.9f, 0.56f,  11.0f},
        {0,   1.2f, -10.9f,0.73f,  65.0f}
    };

    set_vec3(g_program, glm::vec3(1.0f), "u_tint");
    glActiveTexture(GL_TEXTURE0);
    for (const auto& rock : rocks)
    {
        const auto& asset = g_rock_assets[rock.variant];
        if (asset.parts.empty() || near_river(rock.x, rock.z, 0.55f)) continue;
        float y = village_terrain::ground_height(rock.x, rock.z) + 0.02f;
        g_model = glm::translate(glm::mat4(1.0f), glm::vec3(rock.x, y, rock.z));
        g_model = glm::rotate(g_model, glm::radians(rock.yaw), glm::vec3(0,1,0));
        g_model = glm::scale(g_model, glm::vec3(rock.scale));
        set_model(g_program);
        for (const auto& part : asset.parts)
        {
            glBindTexture(GL_TEXTURE_2D, part.texture);
            glBindVertexArray(part.vao);
            glDrawArrays(GL_TRIANGLES, 0, part.count);
        }
    }
    glBindVertexArray(g_cube_vao);
    g_model = glm::mat4(1.0f);
}

static void init_grass_tuft_mesh()
{
    if (g_grass_tuft_vao != 0) return;
    std::vector<float> mesh;
    const float half_w = 0.17f;
    const float h = 0.58f;
    const std::array<float, 3> angles = {0.0f, 60.0f, 120.0f};
    for (float deg : angles)
    {
        float a = glm::radians(deg);
        glm::vec3 dir(std::cos(a), 0.0f, std::sin(a));
        glm::vec3 side = dir * half_w;
        glm::vec3 a0 = -side;
        glm::vec3 b0 = -side + glm::vec3(0,h,0);
        glm::vec3 c0 = side + glm::vec3(0,h,0);
        glm::vec3 d0 = side;
        glm::vec3 outward(dir.z, 0.0f, -dir.x);
        add_quad(mesh, a0, b0, c0, d0, outward);
    }
    g_grass_tuft_vao = upload_mesh(mesh, g_grass_tuft_vertex_count);
}

static void draw_grass_tufts()
{
    init_grass_tuft_mesh();
    if (!tex_grass_tuft) return;
    static const struct GrassClump
    {
        float x, z, scale, yaw, phase, amp;
    } clumps[] = {
        {-9.6f,  1.1f, 0.95f,  12.0f, 0.1f, 6.0f}, {-8.1f, -0.4f, 1.10f, 85.0f, 1.0f, 5.0f},
        {-7.0f,  3.5f, 1.00f,  37.0f, 2.1f, 6.5f}, {-6.1f, -6.5f, 1.06f, 60.0f, 0.7f, 5.0f},
        {-4.7f,  8.2f, 1.14f,  12.0f, 1.8f, 5.5f}, {-3.6f, -8.5f, 1.08f, 44.0f, 0.2f, 6.0f},
        {-3.2f,  0.0f, 0.92f,  71.0f, 2.6f, 4.5f}, {-2.1f,  4.2f, 0.98f, 16.0f, 1.4f, 5.5f},
        {-1.1f,  7.9f, 1.12f,  21.0f, 0.5f, 6.0f}, {-0.6f, -2.8f, 0.90f, 90.0f, 2.3f, 4.0f},
        { 0.8f, 10.3f, 1.15f,  31.0f, 0.9f, 6.5f}, { 1.0f,  2.8f, 1.02f, 55.0f, 1.1f, 4.6f},
        { 1.5f, -10.2f,1.18f,  17.0f, 2.4f, 5.5f}, { 2.4f, -6.0f, 0.96f, 70.0f, 0.3f, 4.4f},
        { 2.7f,  7.1f, 1.06f,  23.0f, 1.7f, 5.0f}, { 3.6f,  4.4f, 0.94f, 39.0f, 0.6f, 4.8f},
        { 4.8f, -8.8f, 1.20f,  12.0f, 1.2f, 6.5f}, { 5.4f, -3.7f, 0.98f, 75.0f, 2.2f, 5.0f},
        { 5.9f,  1.8f, 1.02f,  28.0f, 0.8f, 5.4f}, { 6.9f,  5.8f, 1.05f, 44.0f, 1.6f, 5.8f},
        { 7.8f, -7.6f, 1.16f,  65.0f, 0.4f, 5.5f}, { 8.2f,  8.6f, 1.00f, 80.0f, 2.0f, 4.8f},
        { 9.2f, -1.3f, 0.90f,  35.0f, 1.4f, 4.0f}, {10.6f,  4.0f, 0.96f, 24.0f, 0.1f, 4.6f},
        {-11.8f,-3.5f, 1.08f,  41.0f, 2.5f, 5.2f}, {11.8f,-8.8f, 1.10f, 59.0f, 1.9f, 5.7f},
        {-12.5f, 6.1f, 1.04f,  15.0f, 0.7f, 5.4f}, {12.6f, 1.9f, 0.94f, 73.0f, 2.1f, 4.4f}
    };

    const float t = static_cast<float>(glfwGetTime());
    set_vec3(g_program, glm::vec3(1.0f), "u_tint");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_grass_tuft);
    glBindVertexArray(g_grass_tuft_vao);
    for (const auto& tuft : clumps)
    {
        if (near_river(tuft.x, tuft.z, 0.30f)) continue;
        float y = village_terrain::ground_height(tuft.x, tuft.z) + 0.01f;
        float sway_z = std::sin(t * 1.55f + tuft.phase) * tuft.amp;
        float sway_x = std::cos(t * 1.10f + tuft.phase * 1.7f) * (tuft.amp * 0.35f);
        g_model = glm::translate(glm::mat4(1.0f), glm::vec3(tuft.x, y, tuft.z));
        g_model = glm::rotate(g_model, glm::radians(tuft.yaw), glm::vec3(0,1,0));
        g_model = glm::rotate(g_model, glm::radians(sway_z), glm::vec3(0,0,1));
        g_model = glm::rotate(g_model, glm::radians(sway_x), glm::vec3(1,0,0));
        g_model = glm::scale(g_model, glm::vec3(tuft.scale));
        set_model(g_program);
        glDrawArrays(GL_TRIANGLES, 0, g_grass_tuft_vertex_count);
    }
    glBindVertexArray(g_cube_vao);
    g_model = glm::mat4(1.0f);
}

static void init_bush_mesh()
{
    if (g_bush_vao != 0) return;
    std::vector<float> mesh;
    const float half_w = 0.55f;
    const float h = 0.95f;
    const std::array<float, 3> angles = {0.0f, 60.0f, 120.0f};
    for (float deg : angles)
    {
        float a = glm::radians(deg);
        glm::vec3 dir(std::cos(a), 0.0f, std::sin(a));
        glm::vec3 side = dir * half_w;
        glm::vec3 a0 = -side;
        glm::vec3 b0 = -side + glm::vec3(0,h,0);
        glm::vec3 c0 = side + glm::vec3(0,h,0);
        glm::vec3 d0 = side;
        glm::vec3 outward(dir.z, 0.0f, -dir.x);
        add_quad(mesh, a0, b0, c0, d0, outward);
    }
    g_bush_vao = upload_mesh(mesh, g_bush_vertex_count);
}

static void draw_bushes()
{
    init_bush_mesh();
    if (!tex_bush) return;
    static const struct BushPlacement
    {
        float x, z, scale, yaw;
    } bushes[] = {
        {-7.8f, -1.8f, 0.95f, 22.0f},
        {-1.8f,  8.7f, 1.08f, 70.0f},
        { 8.6f, -4.5f, 1.02f, 38.0f},
        {10.9f,  4.7f, 0.92f, 95.0f}
    };

    set_vec3(g_program, glm::vec3(1.0f), "u_tint");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_bush);
    glBindVertexArray(g_bush_vao);
    for (const auto& bush : bushes)
    {
        if (near_river(bush.x, bush.z, 0.60f)) continue;
        float y = village_terrain::ground_height(bush.x, bush.z) + 0.01f;
        g_model = glm::translate(glm::mat4(1.0f), glm::vec3(bush.x, y, bush.z));
        g_model = glm::rotate(g_model, glm::radians(bush.yaw), glm::vec3(0,1,0));
        g_model = glm::scale(g_model, glm::vec3(bush.scale));
        set_model(g_program);
        glDrawArrays(GL_TRIANGLES, 0, g_bush_vertex_count);
    }
    glBindVertexArray(g_cube_vao);
    g_model = glm::mat4(1.0f);
}

// One shared dial mesh and texture, reused on the tower's four faces.
// Hands are independent transforms driven by elapsed GLFW time.
static unsigned int g_clock_dial_vao = 0;
static unsigned int g_clock_dial_vertices = 0;
static unsigned int g_clock_dial_texture = 0;

static void draw_tower_clock_face(const glm::mat4& model, double seconds)
{
    if (!g_clock_dial_vao)
    {
        std::vector<float> plane;
        // Width 0.44 in OBJ-local coordinates. The watchtower scales by 2.30.
        add_triangle(plane,
            { -0.14f,-0.14f,0 }, { 0.14f,-0.14f,0 }, { 0.14f,0.14f,0 },
            { 0,0 }, { 1,0 }, { 1,1 }, { 0,0,1 });

        add_triangle(plane,
            { -0.14f,-0.14f,0 }, { 0.14f,0.14f,0 }, { -0.14f,0.14f,0 },
            { 0,0 }, { 1,1 }, { 0,1 }, { 0,0,1 });
        g_clock_dial_vao=upload_mesh(plane,g_clock_dial_vertices);
        g_clock_dial_texture=init_texture("resources/models/watchtower/clock_dial.png");
    }
    g_model=model;
    set_model(g_program);
    set_vec3(g_program, glm::vec3(1.0f), "u_tint");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,g_clock_dial_texture);
    glBindVertexArray(g_clock_dial_vao);
    glDrawArrays(GL_TRIANGLES,0,g_clock_dial_vertices);
    glBindVertexArray(g_cube_vao);

    const double tau=6.283185307179586;
    const float minute_angle=static_cast<float>(-tau*std::fmod(seconds,3600.0)/3600.0);
    const float second_angle=static_cast<float>(-tau*std::fmod(seconds,60.0)/60.0);
    auto hand=[&](float angle,float length,float thickness,float depth,
                  glm::vec3 color)
    {
        // A hand is aligned upwards in dial-local XY space. Rotating around
        // local Z keeps its base at the exact centre of the clock.
        g_model=glm::translate(model,glm::vec3(0,0,depth));
        g_model=glm::rotate(g_model,angle,glm::vec3(0,0,1));
        g_model=glm::translate(g_model,glm::vec3(0,length*0.43f,0));
        g_model=glm::scale(g_model,glm::vec3(thickness,length,0.015f));
        set_model(g_program);
        set_vec3(g_program,color,"u_tint");
        glBindTexture(GL_TEXTURE_2D,tex_wood);
        glDrawArrays(GL_TRIANGLES,0,36);
    };
    hand(minute_angle, 0.099f, 0.012f, 0.018f, glm::vec3(0.20f, 0.18f, 0.17f));
    hand(second_angle, 0.121f, 0.007f, 0.032f, glm::vec3(0.84f, 0.25f, 0.16f));
    g_model=glm::translate(model,glm::vec3(0,0,0.044f));
    g_model=glm::scale(g_model,glm::vec3(0.035f,0.035f,0.025f));
    set_model(g_program);
    set_vec3(g_program,glm::vec3(0.25f,0.20f,0.16f),"u_tint");
    glBindTexture(GL_TEXTURE_2D,tex_wood);
    glDrawArrays(GL_TRIANGLES,0,36);
}

static void draw_imported_watchtower()
{
    if (!load_imported_watchtower()) return;
    // Blockbench exported Y=0 at the model bottom and a height of 2.84375.
    // At scale 2.30, the physical tower is approx. 6.54 world units tall.
    const float scale=2.30f;
    const float x=-11.6f, z=0.6f;
    const float y=village_terrain::ground_height(x,z);
    g_model=glm::translate(glm::mat4(1.0f),glm::vec3(x,y,z));
    g_model=glm::scale(g_model,glm::vec3(scale));
    set_model(g_program);
    set_vec3(g_program,glm::vec3(1.0f),"u_tint");
    glActiveTexture(GL_TEXTURE0);
    for (const auto& part : g_imported_tower_parts)
    {
        glBindTexture(GL_TEXTURE_2D,part.texture);
        glBindVertexArray(part.vao);
        glDrawArrays(GL_TRIANGLES,0,part.count);
    }
    glBindVertexArray(g_cube_vao);

    // All four faces share one clock design. The OBJ mesh remains unchanged.
    // Clock faces are mounted on the narrower upper octagonal section.
    const double elapsed=glfwGetTime();
    glm::mat4 tower_world=glm::translate(glm::mat4(1.0f),glm::vec3(x,y,z));
    tower_world=glm::scale(tower_world,glm::vec3(scale));
    for (float facing : {0.0f,90.0f,180.0f,270.0f})
    {
        glm::mat4 face=glm::rotate(tower_world,glm::radians(facing),glm::vec3(0,1,0));
        face=glm::translate(face,glm::vec3(0.0f,1.850f,0.385f));
        draw_tower_clock_face(face,elapsed);
    }
    g_model=glm::mat4(1.0f);
}
// -------- End Blockbench OBJ importer --------

// Blockbench blacksmith replaces houses[3], kind==2.
// Preserve its original village coordinates and orientation.
static void draw_imported_blacksmith(const glm::vec3& position, float yaw_degrees)
{
    if (!load_imported_blacksmith()) return;

    // Blockbench OBJ bounds: X[-0.755,0.537], Y[-0.003,1.063],
    // Z[-0.531,0.703]. Roughly 2.4x in-game reproduces the old footprint.
    constexpr float model_scale = 2.40f;
    constexpr float model_center_x = -0.109f;
    constexpr float model_center_z = 0.086f;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::rotate(model, glm::radians(yaw_degrees), glm::vec3(0,1,0));
    model = glm::scale(model, glm::vec3(model_scale));
    model = glm::translate(model, glm::vec3(-model_center_x, 0.0f, -model_center_z));

    g_model = model;
    set_model(g_program);
    set_vec3(g_program, glm::vec3(1.0f), "u_tint");
    glActiveTexture(GL_TEXTURE0);
    for (const auto& part : g_imported_blacksmith_parts)
    {
        glBindTexture(GL_TEXTURE_2D, part.texture);
        glBindVertexArray(part.vao);
        glDrawArrays(GL_TRIANGLES, 0, part.count);
    }
    glBindVertexArray(g_cube_vao);
    g_model = glm::mat4(1.0f);
}

static void draw_bridge()
{
    const glm::vec3 white(1.0f);
    const float cx = village_terrain::river_x(0.0f);
    auto deck_y = [](float local_x) {
        const float u = std::clamp(local_x / 2.1f,-1.0f,1.0f);
        return 0.28f + 0.28f*(1.0f-u*u);
    };
    // Gently arched deck. Short boards follow the curve.
    for (int i=0;i<16;++i) {
        const float local_x = -2.1f + (i+0.5f)*4.2f/16.0f;
        draw_cube(glm::vec3(cx+local_x,deck_y(local_x),0),
                  glm::vec3(0.25f,0.12f,2.10f),white,tex_wood);
    }
    for (float z : {-0.87f,0.87f}) {
        for (int i=0;i<7;++i) {
            float local_x=-1.8f+i*0.60f;
            draw_cube(glm::vec3(cx+local_x,deck_y(local_x)+0.46f,z),
                      glm::vec3(0.12f,0.90f,0.12f),white,tex_wood);
        }
        for (int i=0;i<16;++i) {
            const float local_x=-2.1f+(i+0.5f)*4.2f/16.0f;
            draw_cube(glm::vec3(cx+local_x,deck_y(local_x)+0.89f,z),
                      glm::vec3(0.27f,0.12f,0.13f),white,tex_wood);
            draw_cube(glm::vec3(cx+local_x,deck_y(local_x)-0.18f,z*0.83f),
                      glm::vec3(0.27f,0.16f,0.13f),white,tex_wood);
        }
    }
}




/*
 * Create window and begin drawing.
 */
static void run(void)
{
    GLFWwindow* window = init_window();
    if (window == nullptr)
        std::exit(1);

    cg::init_ImGui(window);
    init();

    /*
     * Main loop.
     * Runs every frame.
     */
    while (glfwWindowShouldClose(window) == 0)
    {
        glfwPollEvents();
        update_camera(window);

        cg::render_ImGui();

        clear();

        render();

        cg::display_ImGui();

        glfwSwapBuffers(window);
    }

    /*
     * Cleanup.
     */
    if (g_paths_vao) glDeleteVertexArrays(1, &g_paths_vao);
    village_terrain::cleanup();
    cg::cleanup_ImGui();
    cleanup_window(window);
}

int main(void)
{
    /*
     * Keep main function brief.
     */
    run();
}

