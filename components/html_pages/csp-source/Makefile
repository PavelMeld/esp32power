CFILES = csp.cpp StdAfx.cpp
OBJS   = $(CFILES:.cpp=.o)
ifeq ($(OS), linux)
FLAGS   = -DLINUX
endif

%o : %cpp Makefile
	@echo "Compiling '$<'"
	@$(CC) $(FLAGS) -c $< -o $@

all: $(OBJS) $(CFILES) Makefile
	@echo "Linking $(OBJS)..."
	g++ $(OBJS) -o csp	
