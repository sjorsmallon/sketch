
cl src/main.cpp src/game.cpp src/glad.c src/ecs/ecs.cpp /omain.exe /EHsc /Zi /std:c++20 /I"include/" /link /LIBPATH:"libs/SDL/" SDL2.lib SDL2_ttf.lib