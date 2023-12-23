
cl /std:c++20 src/main.cpp src/game.cpp src/glad.c src/ecs/ecs.cpp ^
src/imgui.cpp src/imgui_demo.cpp src/imgui_tables.cpp src/imgui_widgets.cpp src/imgui_draw.cpp src/imgui_impl_sdl2.cpp  ^
src/imgui_impl_opengl3.cpp  src/format.cc ^
 /omain.exe /EHsc /Zi /MP32  /I"include/" ^
 /link /LIBPATH:"libs/SDL/" SDL2.lib SDL2_ttf.lib 