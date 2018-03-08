#-------------------------------------------
#			Vmaxx A.I.R
#	(c) Copyright 2017-2020, Vmaxx
#        All Rights Reserved
#
# File		: im_conver.h
# Author	: Auron
# Time		: 2018-2-28
#-------------------------------------------
GPU=1
OPENCV=1
DEBUG=1
PYTHON=0
LIBPATH=1
TESTTIME=1
IPP=0

ARCH= -gencode arch=compute_35,code=sm_35 \
      -gencode arch=compute_50,code=[sm_50,compute_50] \
      -gencode arch=compute_52,code=[sm_52,compute_52] \
      -gencode arch=compute_61,code=[sm_61,compute_61]

# This is what I use, uncomment if you know your arch and want to specify
# ARCH=  -gencode arch=compute_52,code=compute_52


VPATH=./src/
EXEC=cudadecode
PYLIB=libpymoonergy.so
OBJDIR=./obj/

ARCH+= -rdc=true

CC=g++ -std=c++11
GCC=gcc
NVCC=/usr/local/cuda/bin/nvcc 
OPTS=-Ofast
LDFLAGS= -lm -pthread -Xlinker --unresolved-symbols=ignore-in-shared-libs -L /usr/local/boost/libstatic
COMMON=-I ./include/ -I ./Inc/ 
CFLAGS=-Wall -Wfatal-errors -fPIC

BOOSTLIB=-lboost_system -lboost_thread -lboost_date_time -lboost_filesystem

ifeq ($(DEBUG), 1) 
OPTS=-O0 -g
EXEC=test
endif

ifeq ($(LIBPATH),1)
#设置默认库路径
LDFLAGS+= -Wl,--rpath=./
endif

CFLAGS+=$(OPTS)

ifeq ($(IPP), 1) 
COMMON+= -DIPP -I /opt/intel/compilers_and_libraries_2017.2.174/linux/ipp/include/
LDFLAGS+=-L /opt/intel/compilers_and_libraries_2017.2.174/linux/ipp/lib/intel64
CFLAGS+= -DIPP
endif

ifeq ($(OPENCV), 1) 
COMMON+= -DOPENCV
CFLAGS+= -DOPENCV
LDFLAGS+= `pkg-config --libs opencv` 
COMMON+= `pkg-config --cflags opencv` 
endif

ifeq ($(PYTHON), 1)
PINCLUDE=$(shell python-config --prefix)
COMMON+= -I$(PINCLUDE)/include/python2.7
LDFLAGS+= -L$(PINCLUDE)/lib -lpython2.7
endif

ifeq ($(TESTTIME),1)

COMMON+= -DTEST_TIME
CFLAGS+= -DTEST_TIME
endif

ifeq ($(GPU), 1) 
COMMON+= -DGPU -I/usr/local/cuda/include/
CFLAGS+= -DGPU
LDFLAGS+= -L/usr/local/cuda/lib64 -lcuda -lcudart -lcublas -lnvcuvid
endif

ifeq ($(CUDNN), 1) 
COMMON+= -DCUDNN 
CFLAGS+= -DCUDNN
LDFLAGS+= -lcudnn
endif

OBJ=

ifeq ($(PYTHON), 1)
OBJ+=pymoonergy.o
endif

ifeq ($(GPU), 1) 
LDFLAGS+= -lstdc++ 
OBJ_KERNEL=FrameQueue.o cudaDecode.o VideoDecoder.o VideoParser.o VideoSource.o

endif
OBJ+=$(OBJ_KERNEL)
OBJ+=gpu_kernel.o
OBJS = $(addprefix $(OBJDIR), $(OBJ))
OBJ_KERNELS = $(addprefix $(OBJDIR), $(OBJ_KERNEL))


all: obj backup results $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(COMMON) $(CFLAGS) $^ -o $@ $(LDFLAGS)
ifeq ($(PYTHON), 1)
	#$(CC) -shared $(COMMON) $(CFLAGS) $^ -o $(PYLIB) $(LDFLAGS) $(BOOSTLIB)
endif


$(OBJDIR)%.o: ./%.cpp $(DEPS)
	$(CC) $(COMMON) $(CFLAGS) -c $< -o $@

$(OBJDIR)%.o: %.cpp $(DEPS)
	$(CC) $(COMMON) $(CFLAGS) -c $< -o $@
$(OBJDIR)%.o: %.c $(DEPS)
	$(GCC) $(COMMON) $(CFLAGS) -c $< -o $@
ifeq ($(GPU), 1)
$(OBJDIR)gpu_kernel.o: $(OBJ_KERNELS) $(DEPS)
	$(NVCC) $(ARCH) $(COMMON) --compiler-options "$(CFLAGS)" -dlink -o $@ $(OBJ_KERNELS) -lcudadevrt -lcudart
endif
$(OBJDIR)%.o: ./src/%.cu $(DEPS)
	$(NVCC) $(ARCH) $(COMMON) --compiler-options "$(CFLAGS)" -c $< -o $@ -lcudadevrt

obj:
	mkdir -p obj
backup:
	mkdir -p backup
results:
	mkdir -p results

.PHONY: clean

clean:
	rm -rf $(OBJS) $(EXEC) $(PYLIB)

