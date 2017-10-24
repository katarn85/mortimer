INSTALLDIR = usr/lib

## Since include directives do not impose additional dependencies, we can make
## Makefile more clear, simply putting all includes we ever need in single
## variable. Keep it alphabetic, please.
DEBUG_FLAGS=					\
#		-DSTDTOFILE				\#

INCLUDE_CPPFLAGS =				\
		-I./include			\
		-I./probe_thread		\
		-I/usr/include/appfw		\
		-I/usr/include/dbus-1.0		\
		-I/usr/include/e_dbus-1		\
		-I/usr/include/ecore-1		\
		-I/usr/include/edje-1		\
		-I/usr/include/eet-1		\
		-I/usr/include/efreet-1		\
		-I/usr/include/eina-1		\
		-I/usr/include/eina-1/eina	\
		-I/usr/include/elementary-1	\
		-I/usr/include/ethumb-1		\
		-I/usr/include/evas-1		\
		-I/usr/include/pixman-1		\
		-I/usr/include/system		\
		-I/usr/include/capi-system-runtime-info	\
		-I/usr/include/vconf		\
		-I/usr/lib/dbus-1.0/include	\

WARN_CFLAGS = 				\
		-Wall			\
		-funwind-tables		\
		-fomit-frame-pointer	\
		-Xlinker		\
		--no-undefined		\
		-Werror			\
		-Wextra			\
		-O2			\
		-Wwrite-strings		\
		-Wlogical-op		\
		-Wpacked		\
		-Winline		\
		-Wno-psabi		\

## Since linking unneeded libraries bloats output of ldd(1), this variable
## holds search paths and common libraries.

LDFLAGS = -shared   	\
	-lX11				\
	-lXext				\
	-lcapi-appfw-application	\
	-lcapi-system-runtime-info	\
	-ldl				\
	-lecore				\
	-lecore_input			\
	-lecore_x			\
	-leina				\
	-levas				\
	-lpthread			\
	-lrt				\
	-Wl,-z,noexecstack

ASMFLAG = -O0 -g

## FIXME: Ideally, UTILITY_SRCS is sources for probe infrastructure and
## PROBE_SRCS is sources for actual replacement functions.  Unfortunatelly,
## it is not so easy and UTILITY_SRCS do not link alone.

COMMON_SRCS = $(UTILITY_SRCS) $(PROBE_SRCS)
UTILITY_SRCS =				\
	./helper/real_functions.c	\
	./helper/libdaprobe.c		\
	./helper/dahelper.c		\
	./helper/btsym.c		\
	./helper/dacollection.c		\
	./helper/dacapture.c		\
	./helper/daforkexec.c		\
	./helper/damaps.c			\
	./helper/dastdout.c			\
	./custom_chart/da_chart.c	\

PROBE_SRCS =	   				\
	./probe_memory/libdamemalloc.c		\
	./probe_memory/libdamemmanage.c		\
	./probe_socket/libdasocket.c		\
	./probe_event/da_event.c		\
	./probe_event/keytouch.c		\
	./probe_event/orientation.c		\
	./probe_third/libdaemon.c		\
	./probe_thread/libdathread.c		\
	./probe_thread/libdasync.c		\
	./probe_userfunc/libdauserfunc.c	\
	./probe_file/da_io_posix.c		\
	./probe_file/da_io_stdc.c		\

DUMMY_SRCS = ./custom_chart/da_chart_dummy.c
CAPI_SRCS = 	$(COMMON_SRCS)			\
		./probe_capi/capi_appfw.c		\
		./probe_ui/capi_capture.c

TIZEN_SRCS =	$(COMMON_SRCS) $(CAPI_SRCS)\
		./helper/addr-tizen.c						\
		./helper/common_probe_init.c				\
		./probe_memory/libdanew.cpp					\
		./probe_graphics/da_evas_gl.c				\
		./probe_graphics/da_gl_api_init.c			\
		./probe_graphics/da_gles20_tizen.cpp			\
		./probe_graphics/da_gles20_native.cpp

ASM_SRC = ./helper/da_call_original.S

## Totally brain-dead.
## FIXME: Rewrite this normally with eval.
ASM_OBJ = $(patsubst %.S,%.o, $(ASM_SRC))
CAPI_OBJS = $(patsubst %.c,%.o, $(CAPI_SRCS)) $(ASM_OBJ)
TIZEN_OBJS = $(patsubst %.cpp,%.o, $(patsubst %.c,%.o, $(TIZEN_SRCS))) $(ASM_OBJ)
DUMMY_OBJS = $(patsubst %.c,%.o, $(DUMMY_SRCS))


TIZEN_TARGET = da_probe_tizen.so
DUMMY_TARGET = libdaprobe.so

CPPFLAGS = $(INCLUDE_CPPFLAGS) -D_GNU_SOURCE -DSELF_LIB_NAME="\"/$(INSTALLDIR)/$(TIZEN_TARGET)\""
CFLAGS = $(WARN_CFLAGS) -fPIC
CXXFLAGS = $(WARN_CFLAGS) -fPIC

TIZEN_CPPFLAGS = -DTIZENAPP $(SWAP_PROBE_DEFS)
TIZEN_LDFLAGS = -lstdc++

all:	capi tizen dummy
tizen:	headers $(TIZEN_TARGET)
dummy:	headers $(DUMMY_TARGET)

$(ASM_OBJ): $(ASM_SRC)
	$(CC) $(ASMFLAG) -c $^ -o $@

GENERATED_CONFIG = include/api_config.h
GENERATED_HEADERS = include/api_id_mapping.h include/api_id_list.h include/id_list
headers: $(GENERATED_CONFIG) $(GENERATED_HEADERS)
rmheaders:
	rm -f $(GENERATED_CONFIG) $(GENERATED_HEADERS)

$(GENERATED_CONFIG): ./scripts/gen_api_config.sh
	sh $< > $@

include/api_id_mapping.h: ./scripts/gen_api_id_mapping_header.awk
include/api_id_list.h: ./scripts/gen_api_id_mapping_header_list.awk
include/id_list: ./scripts/gen_api_id_mapping_list.awk

da_api_map: $(GENERATED_HEADERS)

$(GENERATED_HEADERS): APINAMES=scripts/api_names.txt
$(GENERATED_HEADERS): ./scripts/api_names.txt
$(GENERATED_HEADERS):
	awk -f $< < $(APINAMES) > $@

$(TIZEN_TARGET): LDFLAGS+=$(TIZEN_LDFLAGS)
$(TIZEN_TARGET): CPPFLAGS+=$(TIZEN_CPPFLAGS)
$(TIZEN_TARGET): CPPFLAGS+=$(DEBUG_FLAGS)
$(TIZEN_TARGET): $(TIZEN_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(DUMMY_TARGET): $(DUMMY_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

install: all
	[ -d "$(DESTDIR)/$(INSTALLDIR)" ] || mkdir -p $(DESTDIR)/$(INSTALLDIR)
	install $(TIZEN_TARGET) $(DUMMY_TARGET) $(DESTDIR)/$(INSTALLDIR)/
	install -m 644 include/id_list $(DESTDIR)/$(INSTALLDIR)/da_api_map

clean:
	rm -f *.so *.o $(GENERATED_HEADERS)

.PHONY: all capi tizen dummy clean install headers
