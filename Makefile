# Makefile - Proyecto 2 (Persona 2: implementacion secuencial y pruebas)
CC      = gcc
MPICC   = mpicc
CFLAGS  = -O3 -march=native -funroll-loops -Wall

all: bruteforce_seq

# --- Version secuencial (no requiere MPI) ---
bruteforce_seq: bruteforce_seq.c des.h
	$(CC) $(CFLAGS) -o bruteforce_seq bruteforce_seq.c

# --- Programa base paralelo (requiere MPI: sudo apt install mpich) ---
bruteforce_mpi: bruteforce_mpi.c des.h
	$(MPICC) -O3 -Wall -o bruteforce_mpi bruteforce_mpi.c

# --- Descifrar el cipher real del catedratico ---
crack: bruteforce_seq
	./bruteforce_seq

# --- Barrido de tiempos (peor caso: recorre todo [0,2^bits)) ---
bench: bruteforce_seq
	@echo "bits,espacio,tiempo_s,llaves_seg" > resultados.csv
	@for b in 12 14 16 18 20 22; do \
		./bruteforce_seq bench $$b 2>>.tmp.err >/dev/null; \
	done; \
	grep '^CSV,' .tmp.err | sed 's/^CSV,//' >> resultados.csv; \
	rm -f .tmp.err; \
	cat resultados.csv

clean:
	rm -f bruteforce_seq bruteforce_mpi resultados.csv .tmp.err

.PHONY: all crack bench clean
