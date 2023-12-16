#include "game.hpp"

// necessary includes.
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <glad/glad.h> 
#include <SDL/SDL_opengl.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

// opengl shader stuff.
#include "renderer/shader_program.hpp"

// system includes.
#include <iostream>

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

    // from this point, we have a window and we can load shaders. and set some renderer state.


    // step 1: load a shader!
    uint32_t pbr_shader_program_id = temp_create_pbr_shader_program();


   // Define the parameters for the projection matrix
    float fov = 60.0f;             // Field of view (in degrees)
    float aspect_ratio = 16.0f / 9.0f;  // Aspect ratio (width / height)
    float near_plane = 0.1f;       // Near clipping plane
    float far_plane = 100.0f;      // Far clipping plane

    // Create the projection matrix using glm::perspective
    glm::mat4 projection = glm::perspective(glm::radians(fov), aspect_ratio, near_plane, far_plane);

    Shader_Program pbr_shader{
        .program_id = pbr_shader_program_id
    };

    glUseProgram(pbr_shader.program_id);
    set_uniform(pbr_shader, "projection", projection);


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
    std::cerr << "end of run.\n";
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
        if (type == GL_DEBUG_TYPE_ERROR) std::cerr <<  "[OpenGL]: type =  " << type << " severity = " << severity << ", message = " << message  << "\n";
        else
        {
            std::cerr <<  "[OpenGL]: type =  " << type << " severity = " << severity << ", message = " << message  << "\n";
        }
    }
}


static void handle_input(Game& game)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        std::cerr << "polling event." << '\n';
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
        ImGui_ImplSDL2_ProcessEvent(&event); // Forward your event to backend
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

    

    // end of render
    {
         ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        // Swap buffers
        SDL_GL_SwapWindow(game.window);
    }
    
}
