
MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = \
-I. \
-I$(top_srcdir)/src/lib \
-I$(top_srcdir)/src/lib/include \
@FREETYPE_CFLAGS@ \
@EINA_CFLAGS@

if BUILD_ENGINE_GL_COMMON

noinst_LTLIBRARIES = libevas_engine_gl_common.la
libevas_engine_gl_common_la_SOURCES  = \
evas_gl_private.h \
evas_gl_common.h \
evas_gl_context.c \
evas_gl_shader.c \
evas_gl_rectangle.c \
evas_gl_texture.c \
evas_gl_image.c \
evas_gl_font.c \
evas_gl_polygon.c \
evas_gl_line.c \
shader/rect_frag.h \
shader/rect_frag_bin_s3c6410.h \
shader/rect_vert.h \
shader/rect_vert_bin_s3c6410.h \
shader/font_frag.h \
shader/font_frag_bin_s3c6410.h \
shader/font_vert.h \
shader/font_vert_bin_s3c6410.h \
shader/img_frag.h \
shader/img_frag_bin_s3c6410.h \
shader/img_vert.h \
shader/img_vert_bin_s3c6410.h \
shader/img_nomul_frag.h \
shader/img_nomul_frag_bin_s3c6410.h \
shader/img_nomul_vert.h \
shader/img_nomul_vert_bin_s3c6410.h \
shader/img_bgra_frag.h \
shader/img_bgra_frag_bin_s3c6410.h \
shader/img_bgra_vert.h \
shader/img_bgra_vert_bin_s3c6410.h \
shader/img_bgra_nomul_frag.h \
shader/img_bgra_nomul_frag_bin_s3c6410.h \
shader/img_bgra_nomul_vert.h \
shader/img_bgra_nomul_vert_bin_s3c6410.h \
shader/yuv_frag.h \
shader/yuv_frag_bin_s3c6410.h \
shader/yuv_vert.h \
shader/yuv_vert_bin_s3c6410.h \
shader/yuv_nomul_frag.h \
shader/yuv_nomul_frag_bin_s3c6410.h \
shader/yuv_nomul_vert.h \
shader/yuv_nomul_vert_bin_s3c6410.h \
shader/tex_frag.h \
shader/tex_frag_bin_s3c6410.h \
shader/tex_vert.h \
shader/tex_vert_bin_s3c6410.h \
shader/tex_nomul_frag.h \
shader/tex_nomul_frag_bin_s3c6410.h \
shader/tex_nomul_vert.h \
shader/tex_nomul_vert_bin_s3c6410.h

libevas_engine_gl_common_la_LIBADD = @EINA_LIBS@ @evas_engine_gl_common_libs@ @dlopen_libs@
endif

EXTRA_DIST = \
shader/compile-s3c6410.sh \
shader/compile-sgx.sh \
shader/make-c-bin.sh \
shader/make-c-str.sh \
shader/rect_frag.shd \
shader/rect_frag_s3c6410.asm \
shader/rect_vert.shd \
shader/font_frag.shd \
shader/font_frag_s3c6410.asm \
shader/font_vert.shd \
shader/img_frag.shd \
shader/img_frag_s3c6410.asm \
shader/img_vert.shd \
shader/img_nomul_frag.shd \
shader/img_nomul_vert.shd \
shader/img_bgra_vert.shd \
shader/img_bgra_frag.shd \
shader/img_bgra_vert.shd \
shader/img_bgra_nomul_frag.shd \
shader/img_bgra_nomul_vert.shd \
shader/yuv_frag.shd \
shader/yuv_frag_s3c6410.asm \
shader/yuv_vert.shd \
shader/yuv_nomul_frag.shd \
shader/yuv_nomul_vert.shd \
shader/tex_frag.shd \
shader/tex_frag_s3c6410.asm \
shader/tex_vert.shd \
shader/tex_nomul_frag.shd \
shader/tex_nomul_vert.shd
