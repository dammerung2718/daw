#ifndef RENDERER_H
#define RENDERER_H

#include "vertex.h"

typedef struct Renderer *Renderer;

Renderer makeRenderer(char *title, int width, int height,
                      struct Vertex *vertices, int vertexCount);
void mainLoop(Renderer r);
void freeRenderer(Renderer r);

#endif
