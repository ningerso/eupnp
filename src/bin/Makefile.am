MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib @EINA_CFLAGS@
AM_CFLAGS = -I$(top_srcdir)/src/lib @EINA_CFLAGS@

noinst_PROGRAMS = \
	eupnp_basic_control_point


eupnp_basic_control_point_SOURCES = eupnp_basic_control_point.c
eupnp_basic_control_point_LDADD = $(top_builddir)/src/lib/libeupnp.la
eupnp_basic_control_point_DEPENDENCIES = $(top_builddir)/src/lib/libeupnp.la

