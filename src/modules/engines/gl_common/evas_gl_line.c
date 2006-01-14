#include "evas_gl_private.h"

void
evas_gl_common_line_draw(Evas_GL_Context *gc, RGBA_Draw_Context *dc, int x1, int y1, int x2, int y2)
{
   int r, g, b, a;

   a = (dc->col.col >> 24) & 0xff;
   r = (dc->col.col >> 16) & 0xff;
   g = (dc->col.col >> 8 ) & 0xff;
   b = (dc->col.col      ) & 0xff;
   evas_gl_common_context_color_set(gc, r, g, b, a);
   if (a < 255) evas_gl_common_context_blend_set(gc, 1);
   else evas_gl_common_context_blend_set(gc, 0);
   if (dc->clip.use)
     evas_gl_common_context_clip_set(gc, 1,
				     dc->clip.x, dc->clip.y,
				     dc->clip.w, dc->clip.h);
   else
     evas_gl_common_context_clip_set(gc, 0,
				     0, 0, 0, 0);
   evas_gl_common_context_texture_set(gc, NULL, 0, 0, 0);
   evas_gl_common_context_read_buf_set(gc, GL_BACK);
   evas_gl_common_context_write_buf_set(gc, GL_BACK);
   glBegin(GL_LINES);
   glVertex2i(x1, y1);
   glVertex2i(x2, y2);
   glEnd();
}