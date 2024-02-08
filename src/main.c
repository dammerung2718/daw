#include "renderer.h"
#include <stdio.h>

int main(void) {
  // for attaching debugger
  fprintf(stderr, "Press enter to continue\n");
  getchar();

  Renderer r = makeRenderer("DAW", 640, 480);
  mainLoop(r);
  freeRenderer(r);
}
