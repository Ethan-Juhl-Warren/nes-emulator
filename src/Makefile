CC = gcc
CFLAGS = -std=c11 -O2 -Wall `sdl2-config --cflags`
LDLIBS = `sdl2-config --libs`
SRCS = main.c cpu.c ines.c ppu.c apu.c cntrler.c logging.c screen.c
OBJS = $(addprefix ../bin/,$(SRCS:.c=.o))
BINDIR = ../bin

exe: $(BINDIR) $(OBJS)
	$(CC) $(CFLAGS) -o $(BINDIR)/nes $(OBJS) $(LDLIBS)

$(BINDIR):
	mkdir -p $(BINDIR)

../bin/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BINDIR)

.PHONY: clean
