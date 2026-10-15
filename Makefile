# Makefile - Proyecto 2 (Persona 2: implementacion secuencial y pruebas)
CC      = gcc
MPICC   = mpicc
CFLAGS  = -O3 -march=native -funroll-loops -Wall

all: bruteforce_seq

# --- Version secuencial (no requiere MPI) ---
bruteforce_seq: bruteforce_seq.c des.h
	$(CC) $(CFLAGS) -o bruteforce_seq bruteforce_seq.c

# --- Acercamiento NAIVE paralelo / linea base (requiere MPI) ---
bruteforce_naive: bruteforce_mpi.c bruteforce_common.h des.h
	$(MPICC) -O3 -march=native -funroll-loops -Wall -o bruteforce_naive bruteforce_mpi.c

# Alias historico: mismo codigo que bruteforce_naive
bruteforce_mpi: bruteforce_mpi.c bruteforce_common.h des.h
	$(MPICC) -O3 -march=native -funroll-loops -Wall -o bruteforce_mpi bruteforce_mpi.c

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
	rm -f bruteforce_seq bruteforce_mpi bruteforce_naive bruteforce_master bruteforce_roundrobin cipher_tool resultados.csv resultados_master.csv resultados_roundrobin.csv resultados_naive.csv .tmp.err

.PHONY: all crack bench clean

# --- utilitario de cifrado + estrategia maestro-trabajador ---

# Cifrar/descifrar un .txt con una llave (Parte B, no requiere MPI)
cipher_tool: cipher_tool.c bruteforce_common.h des.h
	$(CC) $(CFLAGS) -o cipher_tool cipher_tool.c

# Estrategia maestro-trabajador (reparto dinamico del espacio de llaves)
bruteforce_master: bruteforce_mpi_master.c bruteforce_common.h des.h
	$(MPICC) -O3 -march=native -funroll-loops -Wall -o bruteforce_master bruteforce_mpi_master.c

# Estrategia round-robin (reparto ciclico del espacio de llaves)
bruteforce_roundrobin: bruteforce_mpi_roundrobin.c bruteforce_common.h des.h
	$(MPICC) -O3 -march=native -funroll-loops -Wall -o bruteforce_roundrobin bruteforce_mpi_roundrobin.c

# Benchmark de la estrategia maestro-trabajador (llave facil/media/dificil)
bench_master: bruteforce_master
	bash scripts/bench_master.sh

# Benchmark de la estrategia round-robin (llave facil/media/dificil)
bench_roundrobin: bruteforce_roundrobin
	MPIRUN_FLAGS=--oversubscribe bash scripts/bench_roundrobin.sh

# Benchmark del acercamiento naive / linea base (llave facil/media/dificil)
bench_naive: bruteforce_naive
	MPIRUN_FLAGS=--oversubscribe bash scripts/bench_naive.sh

.PHONY: bench_master bench_roundrobin bench_naive
