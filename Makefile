TARGETS         = rcopy
SRCS            = rcopy.cpp networks.c
HEADERS         = networks.h rcopy.h cpe464.h
OBJS            = $(SRCS:.cc=.o)
srcdir          = .
INCLUDES	= -I$(srcdir)
CXXFLAGS          = -Wall -pedantic -ansi -D_GNU_SOURCE -g
# CXXFLAGS        = -O3
LDFLAGS		= 
LIBS		= -lstdc++ -lrt libcpe464.2.12.a

# DEPEND		= makedepend
# DEPEND_FLAGS	= 
# DEPEND_DEFINES	= 


default: $(TARGETS) server

all: default

server : server.o networks.o networks.h cpe464.h
	$(CXX) $(LDFLAGS) -o $@ server.cpp networks.c $(LIBS)

$(TARGETS): $(OBJS) $(HEADERS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:  
	-rm -f *.o $(TARGETS) server


# depend:
# 	$(DEPEND) -s '# DO NOT DELETE: updated by make depend'		   \
# 	$(DEPEND_FLAGS) -- $(INCLUDES) $(DEFS) $(DEPEND_DEFINES) $(CFLAGS) \
# 	-- $(SRCS)
