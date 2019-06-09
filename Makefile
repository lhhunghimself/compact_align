CC=gcc
CPP=g++
LD=g++ -L /usr/local/lib
ifdef STATIC 
 STATIC_FLAG = -static
endif
ifdef DEBUG
CFLAGS= -I./ -I./optparse -ggdb -I -Wall -fopenmp -std=c++11 ${STATIC_FLAG}
else 
 ifdef AVX
  CFLAGS= -I./ -I./optparse -Ofast -std=c++11 -fexpensive-optimizations -funroll-all-loops -ffast-math -finline-functions -fopenmp -m64 -mavx -msse3 -msse4 -frerun-loop-opt ${STATIC_FLAG} $(HDRS) $(DEFINES) 
 else
  ifdef SSE2
   ifdef 32Bit
    CFLAGS= -I./ -I./optparse -Ofast -std=c++11 -fexpensive-optimizations -funroll-all-loops -ffast-math -finline-functions -fopenmp -frerun-loop-opt ${STATIC_FLAG} $(HDRS) $(DEFINES)
   else
    CFLAGS= -I./ -I./optparse -Ofast -std=c++11 -fexpensive-optimizations -funroll-all-loops -ffast-math -finline-functions -fopenmp -m64 -frerun-loop-opt ${STATIC_FLAG} $(HDRS) $(DEFINES)
   endif
  else
   ifdef 32Bit
    CFLAGS= -I./ -I./optparse -Ofast -std=c++11 -fexpensive-optimizations -funroll-all-loops -ffast-math -finline-functions -fopenmp -msse3 -frerun-loop-opt ${STATIC_FLAG} $(HDRS) $(DEFINES)
   else
    CFLAGS= -I./ -I./optparse -Ofast -std=c++11 -fexpensive-optimizations -funroll-all-loops -ffast-math -finline-functions -fopenmp -m64 -msse3 -frerun-loop-opt ${STATIC_FLAG} $(HDRS) $(DEFINES)
   endif 
  endif
 endif
endif
LIBS= -lboost_filesystem -lboost_system -lgomp -lm -lc -lgcc -lrt -lz
MERGELIBS= -lm -lc -lgcc -lrt -lz
MKDIR_P = mkdir -p
#ifdef ALPINE
 ALPINE_FLAG= -fPIC -static
#endif
OCFLAGS = -std=c99 -Wall -Wextra -g3 ${ALPINE_FLAG}
LFLAGS = -static-libstdc++

.PHONY: clean cleanDeps cleanBins all 

default: all

OBJ_fqsplit = fqsplit.o optparse.o 

fqsplit : $(OBJ_fqsplit)
	$(LD) -o fqsplit $(LFLAGS) $(OBJ_fqsplit) $(LIBS)
fqsplit.o : fqsplit.cpp 
	$(CPP) $(CFLAGS) -c fqsplit.cpp
		
optparse.o : optparse/optparse.c
	$(CC) $(OCFLAGS) -c optparse/optparse.c

all: fqsplit

clean: cleanBins cleanDeps

cleanDeps:
	-@rm -rf *.o 2>/dev/null || true
	-@rm -rf optparse/*.o 2>/dev/null || true
	-@rm -rf core.* 2>/dev/null || true

cleanBins: 
	-@rm  -f fqsplit
