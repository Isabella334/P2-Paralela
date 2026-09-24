# Makefile - Demo de comunicacion MPI (Persona 3)
MPICC   = mpicc
CFLAGS  = -O2 -Wall

NP      ?= 4
TARGET  ?= 700
SPACE   ?= 1000
DELAY   ?= 1000

all: mpi_demo

mpi_demo: mpi_demo.c
	$(MPICC) $(CFLAGS) -o mpi_demo mpi_demo.c

# Ejemplo: make run NP=4 TARGET=123 SPACE=1000
run: mpi_demo
	mpirun --oversubscribe -np $(NP) ./mpi_demo $(TARGET) $(SPACE) $(DELAY)

clean:
	rm -f mpi_demo

.PHONY: all run clean
