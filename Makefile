SRCS=alloc.c bitint.c codegen.c hashmap.c main.c parse.c platform.c preprocess.c strings.c tokenize.c type.c unicode.c
CFLAGS = -g

OBJS=$(SRCS:.c=.o)

libslimcc.a: $(OBJS)
	ar rcs libslimcc.a $(OBJS)

%.o: %.c slimcc.h
	$(CC) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

clean:
	rm -f *.o libslimcc.a

.PHONY: clean
