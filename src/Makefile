PROG =		unklog
CC =		clang
CFLAGS =	-g -ggdb -pthread -Wall -Werror
HEADERS =	unklog.h
SRCS =		log.c			\
		dispatch.c		\
		config.c		\
		input.c			\
		output.c		\
		output_es.c		\
		output_exec.c		\
		input_kafka.c		\
		metrics.c		\
		daemon.c
OBJS =		$(SRCS:.c=.o)
RM =		rm -f
LDADD =		-lbsd -lpthread -lyajl -lrdkafka -lcurl -luv

.PHONY: all
all: $(PROG)

$(PROG):	$(OBJS)
	$(CC) -o $(PROG) $(OBJS) $(LDFLAGS) $(LDADD)

$(OBJS): $(HEADERS)

.PHONY: clean
clean:
	$(RM) $(OBJS) $(PROG) *~ *core

.c.o:	$< $(HEADERS)
	$(CC) $(CFLAGS) -c $<
