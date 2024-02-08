#include "renderer.h"
#include <stdio.h>
#include <stdlib.h>

struct VertexBuilder {
  struct Vertex *vertices;
  int vertexCount;
};

static void pushVertex(struct VertexBuilder *b, struct Vertex v) {
  b->vertices =
      realloc(b->vertices, (b->vertexCount + 1) * sizeof(struct Vertex));
  b->vertices[b->vertexCount++] = v;
}

static void rectangle(struct VertexBuilder *b, int x, int y, int width,
                      int height) {
  struct Vertex topLeft = {{x, y}};
  struct Vertex topRight = {{x + width, y}};
  struct Vertex bottomRight = {{x + width, y + height}};
  struct Vertex bottomLeft = {{x, y + height}};

  pushVertex(b, topLeft);
  pushVertex(b, topRight);
  pushVertex(b, bottomRight);
  pushVertex(b, bottomRight);
  pushVertex(b, bottomLeft);
  pushVertex(b, topLeft);
}

static void triangle(struct VertexBuilder *b, int x, int y, int width,
                     int height) {
  struct Vertex top = {{x, y}};
  struct Vertex left = {{x, y + height}};
  struct Vertex right = {{x + width, y + height}};

  pushVertex(b, top);
  pushVertex(b, right);
  pushVertex(b, left);
}

int main(void) {
  // for attaching debugger
  fprintf(stderr, "Press enter to continue\n");
  getchar();

  // ui
  struct VertexBuilder b = {0};
  rectangle(&b, 100, 100, 100, 100);
  triangle(&b, 300, 100, 100, 100);

  Renderer r = makeRenderer("DAW", 640, 480, b.vertices, b.vertexCount);
  mainLoop(r);
  freeRenderer(r);
}
