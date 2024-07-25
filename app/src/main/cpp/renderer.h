#ifndef RENDERER_H
#define RENDERER_H

void init_gl(int width, int height);
void resize_gl(int new_width, int new_height);
void render_gl(int width, int height);
void deinit_gl(int width, int height);

#endif // RENDERER_H