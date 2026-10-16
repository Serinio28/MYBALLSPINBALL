#include "raylib.h"
#include "box2d/box2d.h"

int main()
{
    InitWindow(1000, 1000, ":)");
    while (!WindowShouldClose())
    {

        BeginDrawing();
        ClearBackground(BLACK);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}