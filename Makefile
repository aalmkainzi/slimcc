CC ?= gcc

SRCS = alloc.c bitint.c codegen.c hashmap.c main.c parse.c platform.c preprocess.c strings.c tokenize.c type.c unicode.c

# Detect MSVC cl / cl.exe
ifeq ($(filter cl cl.exe,$(notdir $(CC))),)
    # GCC / Clang style
    OBJEXT = o
    LIBTARGET = libslimcc.a
    AR = ar
    ARFLAGS = rcs $(LIBTARGET)
    CFLAGS ?= -g
    COMPILE.c = $(CC) $(CFLAGS) -c $< -o $@ $(LDFLAGS)
else
    # MSVC style
    OBJEXT = obj
    LIBTARGET = libslimcc.lib
    AR = lib.exe
    ARFLAGS = /nologo /out:$(LIBTARGET)
    CFLAGS ?= /MDd /Zi /RTC1
    COMPILE.c = $(CC) /nologo /c $(CFLAGS) /Fo$@ $< $(LDFLAGS)
endif

OBJS = $(SRCS:.c=.$(OBJEXT))

all: $(LIBTARGET)

$(LIBTARGET): $(OBJS)
	$(AR) $(ARFLAGS) $(OBJS)

%.o: %.c slimcc.h
	$(COMPILE.c)

%.obj: %.c slimcc.h
	$(COMPILE.c)

clean:
ifeq ($(filter cl cl.exe,$(notdir $(CC))),)
	rm -f *.o *.a
else
	del /f /q *.obj *.lib *.pdb 2>NUL
endif

.PHONY: all clean