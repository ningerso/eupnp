MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/src/lib @EINA_CFLAGS@
AM_CFLAGS = -I$(top_srcdir)/src/lib @EINA_CFLAGS@

lib_LTLIBRARIES = libeupnp.la

include_HEADERS = \
	eupnp.h \
	eupnp_ssdp.h \
	eupnp_error.h \
	eupnp_http_message.h \
	eupnp_udp_transport.h \
	eupnp_control_point.h

libeupnp_la_SOURCES = \
	eupnp.c \
	eupnp_ssdp.c \
	eupnp_error.c \
	eupnp_http_message.c \
	eupnp_udp_transport.c \
	eupnp_control_point.c

libeupnp_la_LIBADD = @EINA_LIBS@
libeupnp_la_LDFLAGS = -version-info @version_info@
