#ifndef RENDERER_H
#define RENDERER_H

typedef struct Renderer *Renderer;

Renderer makeRenderer(char *title, int width, int height);
void mainLoop(Renderer r);
void freeRenderer(Renderer r);

#endif
