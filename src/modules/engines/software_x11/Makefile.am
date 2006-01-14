## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS     = 1.4 foreign

# A list of all the files in the current directory which can be regenerated
MAINTAINERCLEANFILES = Makefile.in

INCLUDES	     = -I. \
		       -I$(top_srcdir)/src/lib \
		       -I$(top_srcdir)/src/lib/include \
		       @FREETYPE_CFLAGS@ @x_cflags@
		       
if BUILD_ENGINE_SOFTWARE_X11

pkgdir                               = $(libdir)/evas/modules/engines/software_x11/$(MODULE_ARCH)

pkg_LTLIBRARIES		= module.la

module_la_SOURCES  = \
evas_engine.h \
evas_engine.c \
evas_outbuf.c \
evas_x_buffer.c \
evas_x_color.c \
evas_x_main.c

module_la_LIBADD       = @x_libs@
module_la_LDFLAGS      = -module -avoid-version \
				-L$(top_builddir)/src/lib -L$(top_builddir)/src/lib/.libs
				       
module_la_DEPENDENCIES = \
$(top_builddir)/config.h

endif

EXTRA_DIST = \
evas_engine.h \
evas_engine.c \
evas_outbuf.c \
evas_x_buffer.c \
evas_x_color.c \
evas_x_main.c