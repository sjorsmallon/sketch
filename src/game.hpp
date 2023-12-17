#include "ecs/ecs.hpp"
#include <SDL/SDL.h>
#include <glad/glad.h> 
#include <SDL/SDL_opengl.h>



const int TARGET_FPS = 120;
const int MILLISECONDS_PER_FRAME = 1000 / TARGET_FPS;
#define OK 0

struct Game {
	static inline int window_width = 1920;
	static inline int window_height = 1080;

	Registry registry;

	int previous_frame_start_ms = 0;
	bool frame_rate_capped = false;
	bool is_running = true;
	bool debug = true;

	SDL_Window* window;
  	SDL_GLContext context;

  	
};

void init(Game& game);
void run(Game& game);
void deinit(Game& game);