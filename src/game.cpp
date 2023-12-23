#include "game.hpp"

// necessary includes.
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <glad/glad.h> 
#include <SDL/SDL_opengl.h>

// optional fmt access. not used in any of the "core" libraries, but used by game.
#include "log/log.hpp"


#include "log/log_threaded.hpp"
// test for async logging

//imgui
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

// opengl shader stuff.
#include <stdlib.h>
#include "renderer/shader_program.hpp"


// physics stuff.
#include "systems/joltphysicssystem.hpp"

// system includes.
#include <iostream>
#include <array>
#include <thread>
#include <numeric> // for std::accumulate


moodycamel::ConcurrentQueue<std::string> q;

std::thread logger(logger_thread, std::ref(q));



glm::vec3 default_camera_position{0.0,0.0, 10.0};
glm::vec3 default_up{0.0,1.0,0.0};
glm::vec3 default_camera_target{0.0, 0.0, -1.0};

uint32_t triangle_VAO;
uint32_t triangle_VBO;

uint32_t cube_VAO;
uint32_t cube_VBO;
uint32_t cube_EBO;

uint32_t instanced_cube_VAO;
uint32_t instanced_cube_VBO;
uint32_t instanced_cube_EBO;

uint32_t instanced_cube_no_index_buffer_VAO;
uint32_t instanced_cube_no_index_buffer_VBO;

uint32_t compute_VAO;
uint32_t compute_VBO;
uint32_t compute_offset_VBO;

uint32_t query_start;
uint32_t query_end;


Shader_Program passthrough_shader{};
Shader_Program fixed_color_shader{};
Shader_Program fixed_color_instanced_shader{};
Shader_Program compute_shader{};
Shader_Program fixed_color_instanced_vec4_shader{};

constexpr int compute_stride_size_x = 256;
constexpr int cube_count = 32768 * 2;
static_assert(cube_count % compute_stride_size_x == 0);
std::array<glm::vec4, cube_count> cube_positions;


static void GLAPIENTRY opengl_debug_message_callback(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void* userParam );

static void handle_input(Game& game);
static void update(Game& game);
static void render(Game& game);

static void create_triangle_buffers(uint32_t& VAO, uint32_t& VBO);
static void create_cube_buffers(uint32_t&VAO, uint32_t&VBO, uint32_t& EBO);
static void create_instanced_cube_buffers(uint32_t&VAO, uint32_t&VBO, uint32_t& EBO);
static void create_indexed_instanced_triangle_buffers(uint32_t&VAO, uint32_t& VBO);
static void create_compute_buffers(uint32_t& VAO, uint32_t& VBO, uint32_t& offset_VBO);



static void create_deferred_framebuffer(const int frame_buffer_width, const int frame_buffer_height);


void init(Game& game)
{


    game.window = nullptr;
    {
        // @NOTE(SMIA): this is _NECESSARY_ for capturing with renderdoc to work.
        if (SDL_Init(SDL_INIT_EVERYTHING) != OK)
        {
           Log::error("error initializing SDL.\n");
            return;
        }

        if (TTF_Init() != OK)
        {
            Log::error("error initializing SDL TTF.\n");
            return; 
        }

        SDL_DisplayMode display_mode;
        SDL_GetCurrentDisplayMode(0, &display_mode);

         // Request specific OpenGL version and core profile
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);

        uint32_t window_flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
        game.window = SDL_CreateWindow(
            "OpenGL Window",
             SDL_WINDOWPOS_CENTERED,
             SDL_WINDOWPOS_CENTERED,
            game.window_width, game.window_height, 
            window_flags);

        if (!game.window)
        {
            Log::error("creating openGL window failed. whoops.\n");
            return;
        }
        SDL_RaiseWindow(game.window);

        // Create an OpenGL context
        game.context = SDL_GL_CreateContext(game.window);
        if (!game.context) {
            // Error handling if context creation fails
            SDL_DestroyWindow(game.window);
            SDL_Quit();
            Log::error("context creation failed.\n");

        }

        // Initialize Glad (after creating the OpenGL context)
        if (!gladLoadGL()) {
            // Error handling if Glad fails to load OpenGL
            SDL_GL_DeleteContext(game.context);
            SDL_DestroyWindow(game.window);
            SDL_Quit();
            Log::error("gladLoadGL failed..\n");
        }

        // enable openGL debug output
        // Enable debug output
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // Ensure callback functions are called synchronously
        glDebugMessageCallback(opengl_debug_message_callback, nullptr);

        // Set debug output control parameters (optional)
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
        glDisable(GL_CULL_FACE);

        // generate queries to use for e.g. timing information.
        glGenQueries(1, &query_start);
        glGenQueries(1, &query_end);


        // dear imgui
        {
            // Setup Dear ImGui context
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

            // Setup Platform/Renderer backends
            ImGui_ImplSDL2_InitForOpenGL(game.window, game.context);
            ImGui_ImplOpenGL3_Init();
        }

    }

    Log::log("hello {}!", "world");
    Log::warn("warning the {}", "world");
    Log::error("alex the {}???", "lion");

    // load shaders.
    {
        uint32_t passthrough_shader_id = create_shader_program_from_files({
            {"assets/shaders/passthrough/passthrough.vert", GL_VERTEX_SHADER},
            {"assets/shaders/passthrough/passthrough.frag", GL_FRAGMENT_SHADER}}
            );

        passthrough_shader.program_id = passthrough_shader_id;

        uint32_t fixed_color_shader_id = create_shader_program_from_files({
            {"assets/shaders/fixed_color/fixed_color.vert", GL_VERTEX_SHADER},
            {"assets/shaders/fixed_color/fixed_color.frag", GL_FRAGMENT_SHADER}}
        );

        fixed_color_shader.program_id = fixed_color_shader_id;


        uint32_t fixed_color_instanced_shader_id = create_shader_program_from_files({
            {"assets/shaders/fixed_color_instanced/fixed_color_instanced.vert", GL_VERTEX_SHADER},
            {"assets/shaders/fixed_color_instanced/fixed_color_instanced.frag", GL_FRAGMENT_SHADER}}
        );
        fixed_color_instanced_shader.program_id = fixed_color_instanced_shader_id;

        uint32_t compute_shader_id = create_shader_program_from_files({
            {"assets/shaders/compute_shader/compute_shader.comp", GL_COMPUTE_SHADER}
        });
        compute_shader.program_id = compute_shader_id;



        uint32_t fixed_color_instanced_vec4_shader_id = create_shader_program_from_files({
            {"assets/shaders/fixed_color_instanced_vec4/fixed_color_instanced_vec4.vert", GL_VERTEX_SHADER},
            {"assets/shaders/fixed_color_instanced_vec4/fixed_color_instanced_vec4.frag", GL_FRAGMENT_SHADER}}
        );
        fixed_color_instanced_vec4_shader.program_id = fixed_color_instanced_vec4_shader_id;



        create_triangle_buffers(triangle_VAO, triangle_VBO);
        create_cube_buffers(cube_VAO, cube_VBO, cube_EBO);
        create_instanced_cube_buffers(instanced_cube_VAO, instanced_cube_VBO, instanced_cube_EBO);
        create_indexed_instanced_triangle_buffers(instanced_cube_no_index_buffer_VAO, instanced_cube_no_index_buffer_VBO);
        create_compute_buffers(compute_VAO, compute_VBO, compute_offset_VBO);

        create_deferred_framebuffer(game.window_width, game.window_height);

    }

    // set up systems
    {
        // game.registry.AddSystem<JoltPhysicsSystem>();
    }
}



void run(Game& game)
{
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f); // Example: Set clear color
    
    // Main loop
    while (game.is_running)
    {
        handle_input(game);
        update(game);
        render(game);
    }
}


void deinit
(Game& game)
{

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    // Clean up and exit
    SDL_GL_DeleteContext(game.context);
    SDL_DestroyWindow(game.window);
    SDL_Quit();

}


/////////////////////////////////////////////
// file scope
////////////////////////////////////////////


// openGL debug callback
static void GLAPIENTRY opengl_debug_message_callback(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void* userParam )
{
    /// these warning IDs are ignored.
    /// 0x8251: bound to video memory (intended.)
    /// 0x8250: buffer performance warning: copying atomic buffer (which we want to do, since we need the values.)
    std::set<GLenum> warning_types_to_ignore{0x8251, 0x8250};

    const bool warning_can_be_ignored = (warning_types_to_ignore.find(type) != warning_types_to_ignore.end());

    if (!warning_can_be_ignored) 
    {
        if (type == GL_DEBUG_TYPE_ERROR) 
        {
            Log::error("GL CALLBACK: type = 0x{:x}, severity = 0x{:x}, message = {}\n", type, severity, message);
        }
        else
        {
            Log::log("GL CALLBACK: type = 0x{:x}, severity = 0x{:x}, message = {}\n", type, severity, message);
        }
    }
}


static void handle_input(Game& game)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_KEYDOWN:
            {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    game.is_running = false;
                } else 
                {
                    game.is_running = true;
                }

                break;

            }
         
            case SDL_QUIT:
            {
                game.is_running = false;
                break;
            }

            default:
            {
                break;
            }
        }
        ImGui_ImplSDL2_ProcessEvent(&event); // forward events to imgui.
    }
}

static void update(Game& game) 
{
    // game.registry.GetSystem<JoltPhysicsSystem>().OnUpdate();
    //@NOTE(SJM): SDL_Delay solution (suspend thread?)
    if (game.fixed_framerate)
    {
        int time_elapsed_since_last_frame_ms = (SDL_GetTicks() - game.previous_frame_start_ms);
        int time_to_wait = MILLISECONDS_PER_FRAME - time_elapsed_since_last_frame_ms;
        if (time_to_wait > 0 && time_to_wait <= MILLISECONDS_PER_FRAME)
        {
            SDL_Delay(time_to_wait);
        }
    }
    
    // store the start of "this" (the upcoming) frame.
    float delta_time = (SDL_GetTicks() - game.previous_frame_start_ms) / 1000.0f;
    game.previous_frame_start_ms = SDL_GetTicks();
}


static void render(Game& game) 
{

    static int draw_mode = 0;

    int drawing_triangles = 0;
    int drawing_cube = 1;
    int drawing_instanced_cubes = 2;
    int drawing_instanced_triangles = 3;
    int drawing_instanced_cubes_compute_shader = 4;
    int drawing_instanced_cubes_compute_shader_deferred = 5;


    // start of render
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        
        game.debug = true; // this causes issues with renderdoc.

        if (game.debug) 
        {
           
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            ImGui::Begin("Shapes");
            
            ImGui::Text("Select a shape to draw:");
            

            if (ImGui::TreeNode("Combo"))
            {
                ImGui::Combo("combo (one-liner)", &draw_mode, "Draw Triangle\0Draw Cube\0Draw Instanced Cubes\0Draw Instanced Triangles\0Draw Instanced Cubes with compute shader update\0Drawing Deferred Instanced Cubes with compute shader\0\0");
                ImGui::TreePop();
            }

            ImGui::End();
        }

    }

    // Your rendering code goes here
    {
        if (draw_mode == drawing_triangles)
        {
            glUseProgram(passthrough_shader.program_id);
            glBindVertexArray(triangle_VAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        if (draw_mode == drawing_cube)
        {
            glUseProgram(fixed_color_shader.program_id);

            glm::mat4 model = glm::mat4(1.0f);
            glm::vec3 camera_position = default_camera_position;
            glm::vec3 camera_target = default_camera_target;
            glm::vec3 camera_up = default_up;

            // Create the view matrix
            glm::mat4 view = glm::lookAt(camera_position, camera_target, camera_up);

            float fov = glm::radians(60.0f); // Field of View in radians
            float aspectRatio = 1920.0f / 1080.0f; // Aspect ratio (Width / Height)
            float nearPlane = 0.1f;  // Near clipping plane
            float farPlane = 100.0f; // Far clipping plane

            // Create the projection matrix
            glm::mat4 projection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);


            set_uniform(fixed_color_shader, "model", model);
            set_uniform(fixed_color_shader, "view", view);
            set_uniform(fixed_color_shader, "projection", projection);
            set_uniform(fixed_color_shader, "color", glm::vec4(1.0,0.0,0.0,1.0));

            glUseProgram(fixed_color_shader.program_id);
            glBindVertexArray(cube_VAO);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        if (draw_mode == drawing_instanced_cubes) 
        {
            glUseProgram(fixed_color_instanced_shader.program_id);

            glm::vec3 camera_position = default_camera_position;
            glm::vec3 camera_target = default_camera_target;
            glm::vec3 camera_up = default_up;

            // Create the view matrix
            glm::mat4 view = glm::lookAt(camera_position, camera_target, camera_up);

            float fov = glm::radians(60.0f); // Field of View in radians
            float aspectRatio = 1920.0f / 1080.0f; // Aspect ratio (Width / Height)
            float nearPlane = 0.1f;  // Near clipping plane
            float farPlane = 100.0f; // Far clipping plane

            // Create the projection matrix
            glm::mat4 projection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);

            set_uniform(fixed_color_instanced_shader, "view", view);
            set_uniform(fixed_color_instanced_shader, "projection", projection);

            glBindVertexArray(instanced_cube_VAO);
            glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0, cube_count);
        }


        if (draw_mode == drawing_instanced_triangles)
        {
             glUseProgram(fixed_color_instanced_shader.program_id);

            glm::vec3 camera_position = default_camera_position;
            glm::vec3 camera_target = default_camera_target;
            glm::vec3 camera_up = default_up;

            // Create the view matrix
            glm::mat4 view = glm::lookAt(camera_position, camera_target, camera_up);

            float fov = glm::radians(60.0f); // Field of View in radians
            float aspectRatio = 1920.0f / 1080.0f; // Aspect ratio (Width / Height)
            float nearPlane = 0.1f;  // Near clipping plane
            float farPlane = 100.0f; // Far clipping plane

            // Create the projection matrix
            glm::mat4 projection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);

            set_uniform(fixed_color_instanced_shader, "view", view);
            set_uniform(fixed_color_instanced_shader, "projection", projection);

            glBindVertexArray(instanced_cube_no_index_buffer_VAO);
            GLuint indices[] = {
                0, 1, 2 // Define indices for a single triangle
            };

            glm::vec3 instanceOffsets[] = {
                glm::vec3(0.0f, 0.0f, 0.0f),    // Offset for first instance
                glm::vec3(2.0f, 5.0f, -15.0f),  // Offset for second instance
                glm::vec3(-1.5f, -2.2f, -2.5f) // Offset for third instance
            // ... Add more offsets for additional instances
            };

            int numInstances = sizeof(instanceOffsets) / sizeof(glm::vec3);
            glDrawElementsInstanced(GL_TRIANGLES, sizeof(indices) / sizeof(GLuint), GL_UNSIGNED_INT, 0, 256);
        }

        if (draw_mode == drawing_instanced_cubes_compute_shader)
        {
            // enble if sampling.
            // const int sample_count = 2048;
            // static std::array<double, sample_count> samples;
            // static int idx = 0;

            glUseProgram(compute_shader.program_id);

            // should I bind this vao? or a different one? since the VAO that we used to use 
            //  has a different binding at spot 0? or what's going on?
            // It turns out that binding this VAO is "good enough"? or is it actually wrong and it works by accident?

            glQueryCounter(query_start, GL_TIMESTAMP);
            glBindVertexArray(compute_VAO);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, compute_offset_VBO);
            glDispatchCompute(cube_count / compute_stride_size_x, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            // End query
            glQueryCounter(query_end, GL_TIMESTAMP);

            // Retrieve results
            GLuint64 start_time, end_time;
            glGetQueryObjectui64v(query_start, GL_QUERY_RESULT, &start_time);
            glGetQueryObjectui64v(query_end, GL_QUERY_RESULT, &end_time);

            GLuint64 time_elapsed = end_time - start_time;
            double time_elapsed_ms = static_cast<double>(time_elapsed) / 1e6;
            // use async logging
            log(q,"Compute shader time (ms) : {}", time_elapsed_ms);
            // samples[idx] = time_elapsed_ms;
            // idx += 1;
            // if (idx == sample_count)
            // {
            //     double sum = std::accumulate(samples.begin(), samples.end(), 0.0);

            //     log(q, "average compute shader time (ms): {}", static_cast<double>(sum) / samples.size());

            //     std::cerr << "average compute shader time (ms): " << static_cast<double>(sum) / samples.size() << '\n';

            //     // ah, we exited before being done logging. whoops.
            // }

            glUseProgram(0);


            glBindVertexArray(compute_VAO);
            glBindBuffer(GL_ARRAY_BUFFER, compute_VBO);

            glUseProgram(fixed_color_instanced_vec4_shader.program_id);

            glm::vec3 camera_position = default_camera_position;
            glm::vec3 camera_target = default_camera_target;
            glm::vec3 camera_up = default_up;

            // Create the view matrix
            glm::mat4 view = glm::lookAt(camera_position, camera_target, camera_up);

            float fov = glm::radians(60.0f); // Field of View in radians
            float aspectRatio = 1920.0f / 1080.0f; // Aspect ratio (Width / Height)
            float nearPlane = 0.1f;  // Near clipping plane
            float farPlane = 100.0f; // Far clipping plane

            // Create the projection matrix
            glm::mat4 projection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);

            set_uniform(fixed_color_instanced_vec4_shader, "view", view);
            set_uniform(fixed_color_instanced_vec4_shader, "projection", projection);


    
            glQueryCounter(query_start, GL_TIMESTAMP);
            glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0, cube_count);
            glQueryCounter(query_end, GL_TIMESTAMP);

            // Retrieve results
            {
                GLuint64 start_time, end_time;
                glGetQueryObjectui64v(query_start, GL_QUERY_RESULT, &start_time);
                glGetQueryObjectui64v(query_end, GL_QUERY_RESULT, &end_time);

                GLuint64 time_elapsed = end_time - start_time;
                double time_elapsed_ms = static_cast<double>(time_elapsed) / 1e6;
                // use async logging
                log(q,"drawing time (ms) : {}", time_elapsed_ms);    
            }
            
        }


        // combining everything :O
        if (draw_mode == drawing_instanced_cubes_compute_shader_deferred)
        {
            // compute shader invocation stays the same.
            {
                glUseProgram(compute_shader.program_id);
                // should I bind this vao? or a different one? since the VAO that we used to use 
                //  has a different binding at spot 0? or what's going on?
                // It turns out that binding this VAO is "good enough"? or is it actually wrong and it works by accident?

                glQueryCounter(query_start, GL_TIMESTAMP);
                glBindVertexArray(compute_VAO);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, compute_offset_VBO);
                glDispatchCompute(cube_count / compute_stride_size_x, 1, 1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                // End query
                glQueryCounter(query_end, GL_TIMESTAMP);

                // Retrieve results
                GLuint64 start_time, end_time;
                glGetQueryObjectui64v(query_start, GL_QUERY_RESULT, &start_time);
                glGetQueryObjectui64v(query_end, GL_QUERY_RESULT, &end_time);

                GLuint64 time_elapsed = end_time - start_time;
                double time_elapsed_ms = static_cast<double>(time_elapsed) / 1e6;
                // use async logging
                log(q,"Compute shader time (ms) : {}", time_elapsed_ms);

                glUseProgram(0);
            }


            glBindVertexArray(compute_VAO);
            glBindBuffer(GL_ARRAY_BUFFER, compute_VBO);

            glUseProgram(fixed_color_instanced_vec4_shader.program_id);

            glm::vec3 camera_position = default_camera_position;
            glm::vec3 camera_target = default_camera_target;
            glm::vec3 camera_up = default_up;

            // Create the view matrix
            glm::mat4 view = glm::lookAt(camera_position, camera_target, camera_up);

            float fov = glm::radians(60.0f); // Field of View in radians
            float aspectRatio = 1920.0f / 1080.0f; // Aspect ratio (Width / Height)
            float nearPlane = 0.1f;  // Near clipping plane
            float farPlane = 100.0f; // Far clipping plane

            // Create the projection matrix
            glm::mat4 projection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);

            set_uniform(fixed_color_instanced_vec4_shader, "view", view);
            set_uniform(fixed_color_instanced_vec4_shader, "projection", projection);


    
            glQueryCounter(query_start, GL_TIMESTAMP);
            glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0, cube_count);
            glQueryCounter(query_end, GL_TIMESTAMP);

            // Retrieve results
            {
                GLuint64 start_time, end_time;
                glGetQueryObjectui64v(query_start, GL_QUERY_RESULT, &start_time);
                glGetQueryObjectui64v(query_end, GL_QUERY_RESULT, &end_time);

                GLuint64 time_elapsed = end_time - start_time;
                double time_elapsed_ms = static_cast<double>(time_elapsed) / 1e6;
                // use async logging
                log(q,"drawing time (ms) : {}", time_elapsed_ms);    
            }
            
        }

    }


    // end of render
    {
        if (game.debug) 
        {
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
        // Swap buffers
        SDL_GL_SwapWindow(game.window);
    }
    
}

static void create_triangle_buffers(uint32_t& VAO, uint32_t& VBO)
{
        // Define the vertices of a triangle
    float vertices[] = {
        -0.5f, -0.5f, 0.0f, // Bottom-left
         0.5f, -0.5f, 0.0f, // Bottom-right
         0.0f,  0.5f, 0.0f  // Top-center
    };

    // Create a Vertex Array Object (VAO) and Vertex Buffer Object (VBO)
   
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    // Bind VAO and VBO
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Copy vertex data to VBO
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Set vertex attribute pointers
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Unbind VAO and VBO
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

static void create_cube_buffers(uint32_t& VAO, uint32_t& VBO, uint32_t& EBO)
{
    float vertices[] = {
        // Positions
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f
    };

    // Indices to render the cube
    unsigned int indices[] = {
        0, 1, 2, 2, 3, 0,
        1, 5, 6, 6, 2, 1,
        5, 4, 7, 7, 6, 5,
        4, 0, 3, 3, 7, 4,
        3, 2, 6, 6, 7, 3,
        4, 5, 1, 1, 0, 4
    };

    // Create Vertex Array Object (VAO), Vertex Buffer Object (VBO), and Element Buffer Object (EBO)
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // Bind VAO, VBO, and EBO
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    // Copy vertex data to VBO
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Copy index data to EBO
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Set vertex attribute pointers
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
}


static void create_instanced_cube_buffers(
    uint32_t& VAO,
    uint32_t& EBO,
    uint32_t& VBO) 
{
    float vertices[] = {
        // Positions
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f
    };

    // Indices to render the cube
    unsigned int indices[] = {
        0, 1, 2, 2, 3, 0,
        1, 5, 6, 6, 2, 1,
        5, 4, 7, 7, 6, 5,
        4, 0, 3, 3, 7, 4,
        3, 2, 6, 6, 7, 3,
        4, 5, 1, 1, 0, 4
    };

    // Create Vertex Array Object (VAO), Vertex Buffer Object (VBO), and Element Buffer Object (EBO)
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // Bind VAO, VBO, and EBO
    glBindVertexArray(VAO);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    // Copy index data to EBO
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);


    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // Copy vertex data to VBO
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Set vertex attribute pointers
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);


    // fixed amount of cubes for now.
    std::array<glm::vec3, cube_count> cube_offsets;
    int buffer_size = cube_count * sizeof(glm::vec3);

    uint32_t offset_VBO = 0;
    glGenBuffers(1, &offset_VBO);

    auto random_float = [](float min, float max) -> float {
      return min + ((float)rand() / RAND_MAX) * (max - min);
    };

    float min_x = -10;
    float max_x = 10;
    float min_y = -5;
    float max_y = 5;
    float min_z = -1;
    float max_z = -90;

    for (size_t idx =0 ; idx != cube_offsets.size(); ++idx) 
    {
        glm::vec3& offset = cube_offsets[idx];
        float x = random_float(min_x, max_x);
        float y = random_float(min_y, max_y);
        float z = random_float(min_z, max_z);

        offset = glm::vec3(x,y,z);
    }
    // glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, offset_VBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        buffer_size,
        cube_offsets.data(),
        GL_DYNAMIC_DRAW
    );

   
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
    glVertexAttribDivisor(1, 1); // Set divisor to 1 (instanced rendering)


    // Unbind buffers and VAO
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}



static void create_indexed_instanced_triangle_buffers(uint32_t& VAO, uint32_t& VBO)
{
    uint32_t EBO = 0;
    // Create Vertex Array Object (VAO), Vertex Buffer Object (VBO), and Element Buffer Object (EBO)
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // Define triangle vertices (example vertices)
    GLfloat vertices[] = {
        // Vertex positions (x, y, z)
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        0.0f, 0.5f, 0.0f
    };

    // Define triangle indices (example indices)
    GLuint indices[] = {
        0, 1, 2 // Define indices for a single triangle
    };

    // Bind VAO and VBO
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Bind and buffer EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Set vertex attribute pointers (position attribute)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);



    // ... (Setup matrices and shader program)

    // Instance data: offsets for each instance
    glm::vec3 instanceOffsets[] = {
        glm::vec3(0.0f, 0.0f, 0.0f),    // Offset for first instance
        glm::vec3(2.0f, 5.0f, -15.0f),  // Offset for second instance
        glm::vec3(-1.5f, -2.2f, -2.5f) // Offset for third instance
        // ... Add more offsets for additional instances
    };

    float min_x = -5;
    float max_x = 5;
    float min_y = -5;
    float max_y = 5;
    float min_z = -1;
    float max_z = -90;

    constexpr auto random_float = [](float min, float max) -> float {
      return min + ((float)rand() / RAND_MAX) * (max - min);
    };

    std::array<glm::vec3, 256> instance_offsets;
    int byte_size = sizeof(glm::vec3) * 256;
    for (size_t idx =0 ; idx != instance_offsets.size(); ++idx) 
    {
        glm::vec3& offset = instance_offsets[idx];
        float x = random_float(min_x, max_x);
        float y = random_float(min_y, max_y);
        float z = random_float(min_z, max_z);

        offset = glm::vec3(x,y,z);
    }

    glBindVertexArray(VAO);
    // Create VBO for instance data (offsets)
    GLuint instanceVBO;
    glGenBuffers(1, &instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    // glBufferData(GL_ARRAY_BUFFER, sizeof(instanceOffsets), instanceOffsets, GL_STATIC_DRAW);
    glBufferData(GL_ARRAY_BUFFER, byte_size, instance_offsets.data(), GL_STATIC_DRAW);


    // Bind the VAO for instance data setup
    // Set up instance VBO (attribute pointer and divisor)
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1); // Set divisor to 1 (instanced rendering)

    // Unbind buffers and VAO
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ... (Rest of rendering loop remains unchanged)

}


static void create_compute_buffers(uint32_t& VAO, uint32_t& VBO, uint32_t& offset_VBO)
{ 
    float vertices[] = {
        // Positions
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f
    };

    // Indices to render the cube
    unsigned int indices[] = {
        0, 1, 2, 2, 3, 0,
        1, 5, 6, 6, 2, 1,
        5, 4, 7, 7, 6, 5,
        4, 0, 3, 3, 7, 4,
        3, 2, 6, 6, 7, 3,
        4, 5, 1, 1, 0, 4
    };

    // Create Vertex Array Object (VAO), Vertex Buffer Object (VBO), and Element Buffer Object (EBO)
    uint EBO = 0;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // Bind VAO, VBO, and EBO
    glBindVertexArray(VAO);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    // Copy index data to EBO
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);


    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // Copy vertex data to VBO
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Set vertex attribute pointers
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);


    // fixed amount of cubes for now.
    int buffer_size = cube_count * sizeof(glm::vec4);


    glGenBuffers(1, &offset_VBO);

    auto random_float = [](float min, float max) -> float {
      return min + ((float)rand() / RAND_MAX) * (max - min);
    };

    float min_x = -10;
    float max_x = 10;
    float min_y = -5;
    float max_y = 5;
    float min_z = -1;
    float max_z = -90;

    for (size_t idx = 0 ; idx != cube_positions.size(); ++idx) 
    {
        glm::vec4& offset = cube_positions[idx];
        float x = random_float(min_x, max_x);
        float y = random_float(min_y, max_y);
        float z = random_float(min_z, max_z);

        offset = glm::vec4(x,y,z,0);
    }
    // glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, offset_VBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        buffer_size,
        cube_positions.data(),
        GL_DYNAMIC_DRAW
    );

   
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
    glVertexAttribDivisor(1, 1); // Set divisor to 1 (instanced rendering)

    // Unbind buffers and VAO
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}


static void create_deferred_framebuffer(const int frame_buffer_width, const int frame_buffer_height)
{

    // query if what we're doing is actually possible.
    int max_color_attachments;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_color_attachments);
    Log::log("max color attachments: {}", max_color_attachments);

    assert(max_color_attachments >= 8 && "not enouch max color attachments needed for deferred framebuffer.");

    // init geometry_fbo
    uint32_t geometry_fbo;
    glGenFramebuffers(1, &geometry_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, geometry_fbo);
 
    // - position frame buffer
    uint32_t position_tfbo;
    glGenTextures(1, &position_tfbo);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, position_tfbo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, frame_buffer_width, frame_buffer_height, 0, GL_RGBA, GL_FLOAT, nullptr); // note that these are 16 byte precision!
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, position_tfbo, 0);
      
    // - normals frame buffer
    uint32_t normal_tfbo;
    glGenTextures(1, &normal_tfbo);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normal_tfbo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, frame_buffer_width, frame_buffer_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normal_tfbo, 0);
      
    // - color + specular frame buffer
    uint32_t albedo_specular_tfbo;
    glGenTextures(1, &albedo_specular_tfbo);
    glActiveTexture(GL_TEXTURE2);

    glBindTexture(GL_TEXTURE_2D, albedo_specular_tfbo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame_buffer_width, frame_buffer_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, albedo_specular_tfbo, 0);
      
    // - roughness frame buffer
    uint32_t roughness_tfbo;
    glGenTextures(1, &roughness_tfbo);
    glActiveTexture(GL_TEXTURE3);

    glBindTexture(GL_TEXTURE_2D, roughness_tfbo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, frame_buffer_width, frame_buffer_height, 0, GL_RGBA, GL_FLOAT, nullptr); // 16 byte precision
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER,  GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, roughness_tfbo, 0);

    // -  metallic frame buffer.
    uint32_t metallic_tfbo;
    glGenTextures(1, &metallic_tfbo);
    glActiveTexture(GL_TEXTURE4);

    glBindTexture(GL_TEXTURE_2D, metallic_tfbo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, frame_buffer_width, frame_buffer_height, 0, GL_RGBA, GL_FLOAT, nullptr);  // 16 byte precision
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER,  GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, metallic_tfbo, 0);

    // - ambient occlusion frame buffer.
    uint32_t ambient_occlusion_tfbo;
    glGenTextures(1, &ambient_occlusion_tfbo);
    glActiveTexture(GL_TEXTURE5);

    glBindTexture(GL_TEXTURE_2D, ambient_occlusion_tfbo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, frame_buffer_width, frame_buffer_height, 0, GL_RGBA, GL_FLOAT, nullptr); // 16 byte precision
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER,  GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, ambient_occlusion_tfbo, 0);

    // -  displacement frame buffer
    uint32_t displacement_tfbo;
    glGenTextures(1, &displacement_tfbo);
    glActiveTexture(GL_TEXTURE6);

    glBindTexture(GL_TEXTURE_2D, displacement_tfbo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, frame_buffer_width, frame_buffer_height, 0, GL_RGBA, GL_FLOAT, nullptr); // 16 byte precision
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER,  GL_COLOR_ATTACHMENT6, GL_TEXTURE_2D, displacement_tfbo, 0);

    // - texture normal (why is this necessary?)
    uint32_t texture_normal_tfbo;
    glGenTextures(1, &texture_normal_tfbo);
    glActiveTexture(GL_TEXTURE7);

    glBindTexture(GL_TEXTURE_2D, texture_normal_tfbo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, frame_buffer_width, frame_buffer_height, 0, GL_RGBA, GL_FLOAT, nullptr); // 16 byte precision
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER,  GL_COLOR_ATTACHMENT7, GL_TEXTURE_2D, texture_normal_tfbo, 0);

    // - tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
    unsigned int attachments[8] = {
        GL_COLOR_ATTACHMENT0, // fragment position
        GL_COLOR_ATTACHMENT1, // fragment normal
        GL_COLOR_ATTACHMENT2, // albedo_specular
        GL_COLOR_ATTACHMENT3, // roughness
        GL_COLOR_ATTACHMENT4, // metallic
        GL_COLOR_ATTACHMENT5, // ambient_occlusion
        GL_COLOR_ATTACHMENT6, // displacement
        GL_COLOR_ATTACHMENT7}; // texture_normal

    assert(max_color_attachments == 8);

    glDrawBuffers(max_color_attachments, attachments);
      
    // then also add render buffer object as depth buffer and check for completeness.
    unsigned int depth_rbo;
    glGenRenderbuffers(1, &depth_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, frame_buffer_width, frame_buffer_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rbo);

    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) Log::error("[OpenGL] deferred Framebuffer is incomplete.\n");
    assert((glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) && "glCheckFrameBufferStatus: deferred framebuffer is incomplete.");

    // unbind geometry framebuffer, bind default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}