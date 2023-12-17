#include "game.hpp"

// necessary includes.
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <glad/glad.h> 
#include <SDL/SDL_opengl.h>

// optional fmt access. not used in any of the "core" libraries, but used by log.
#include "log/log.hpp"



//imgui
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

// opengl shader stuff.
#include "renderer/shader_program.hpp"

// system includes.
#include <iostream>

GLuint VAO, VBO;
Shader_Program passthrough_shader_program{};


static void GLAPIENTRY opengl_debug_message_callback(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void* userParam );
static void render(Game& game);
static void handle_input(Game& game);

void init(Game& game)
{
    game.window = nullptr;
    {
        if (SDL_Init(SDL_INIT_EVERYTHING != OK))
        {
            std::cerr << "error initializing SDL.\n";
            return;
        }

        if (TTF_Init() != OK)
        {
            std::cerr << "error initializing SDL TTF.\n";
            return; 
        }

        SDL_DisplayMode display_mode;
        SDL_GetCurrentDisplayMode(0, &display_mode);

         // Request specific OpenGL version and core profile
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4); // Request OpenGL 4.3
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE); // Use core profile


        uint32_t window_flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
        game.window = SDL_CreateWindow(
            "OpenGL Window",
             SDL_WINDOWPOS_CENTERED,
             SDL_WINDOWPOS_CENTERED,
            game.window_width, game.window_height, 
            window_flags);

        if (!game.window)
        {
            std::cerr << "creating openGL window failed. whoops.\n";
            return;
        }
        SDL_RaiseWindow(game.window);

        // Create an OpenGL context
        game.context = SDL_GL_CreateContext(game.window);
        if (!game.context) {
            // Error handling if context creation fails
            SDL_DestroyWindow(game.window);
            SDL_Quit();
            std::cerr << "context creation failed.\n";

        }

        // Initialize Glad (after creating the OpenGL context)
        if (!gladLoadGL()) {
            // Error handling if Glad fails to load OpenGL
            SDL_GL_DeleteContext(game.context);
            SDL_DestroyWindow(game.window);
            SDL_Quit();
            std::cerr << "gladLoadGL failed..\n";
        }

        // enable openGL debug output
        // Enable debug output
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // Ensure callback functions are called synchronously
        glDebugMessageCallback(opengl_debug_message_callback, nullptr);

        // Set debug output control parameters (optional)
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);

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

    // openGL "setup"
    {


        uint32_t passthrough_shader_id = create_shader_program_from_files({
            {"assets/shaders/passthrough/passthrough.vert", GL_VERTEX_SHADER},
            {"assets/shaders/passthrough/passthrough.frag", GL_FRAGMENT_SHADER}}
            );

        passthrough_shader_program.program_id = passthrough_shader_id;



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
}

void run(Game& game)
{
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f); // Example: Set clear color
    
    // Main loop
    while (game.is_running)
    {
        handle_input(game);    
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


static void render(Game& game) 
{
    // start of render
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        if (game.debug) 
        {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
            ImGui::ShowDemoWindow(); // Show demo window! :)
        }

    }

    // Your rendering code goes here
    // Use the shader program
    glUseProgram(passthrough_shader_program.program_id);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);


    // end of render
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        // Swap buffers
        SDL_GL_SwapWindow(game.window);
    }
    
}
