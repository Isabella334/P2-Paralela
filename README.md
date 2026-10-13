# P2-Paralela: Fuerza bruta sobre DES con MPI

Proyecto 2 de Computación Paralela y Distribuida. Encuentra por fuerza bruta la
llave privada con la que se cifró un texto, comparando una versión secuencial contra
varias estrategias paralelas con MPI.

## Requisitos

- `gcc` (para las versiones secuenciales)
- Open MPI (`mpicc`, `mpirun`) para las versiones paralelas
  - Debian/Ubuntu: `sudo apt install openmpi-bin libopenmpi-dev`

## Compilación

```bash
make bruteforce_seq      # secuencial (gcc)
make cipher_tool         # cifrar/descifrar archivos (gcc)
make bruteforce_mpi      # acercamiento naive (mpicc)
make bruteforce_master   # estrategia maestro-trabajador (mpicc)
```

## Ejecución

### Secuencial: romper el cipher embebido
```bash
./bruteforce_seq [bits]        # bits = cota 2^bits (por defecto 28)
```

### Cifrar / descifrar un archivo
```bash
echo -n "Esta es una prueba de proyecto 2" > frase.txt
./cipher_tool encrypt frase.txt cifrado.bin 42
./cipher_tool decrypt cifrado.bin 42 "es una prueba de"
```

### Naive paralelo
```bash
mpirun -np 4 ./bruteforce_mpi [bits]
```

### Maestro-trabajador
```bash
# cipher embebido del catedratico (frase clave " the ")
mpirun -np 4 ./bruteforce_master --bits 28

# plantar una llave propia y romperla (pruebas)
mpirun -np 4 ./bruteforce_master --key 105734 --bits 24 --keyword "es una prueba de"

# cifrar un .txt con una llave y romperlo
mpirun -np 4 ./bruteforce_master --file frase.txt --key 42 --keyword "es una prueba de"
```
Opciones: `--file`, `--key`, `--keyword`, `--bits`, `--chunk`. Con `-np 1` corre en modo
secuencial (línea base para speedup).

## Benchmarks

```bash
make bench            # secuencial: tiempos por longitud de llave -> resultados.csv
bash scripts/bench_master.sh [bits]   # maestro-trabajador -> resultados_master.csv
```
(En OpenMPI, si pides más procesos que núcleos: `MPIRUN_FLAGS=--oversubscribe bash scripts/bench_master.sh`.)

## Documentación

Pegar enlace a google docs
