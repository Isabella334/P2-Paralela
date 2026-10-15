# P2-Paralela: Fuerza bruta sobre DES con MPI

Proyecto 2 de Computación Paralela y Distribuida. Encuentra por fuerza bruta la
llave privada con la que se cifró un texto, comparando una versión secuencial contra
tres estrategias paralelas con MPI (naive, round-robin y maestro-trabajador).
DES se usa con fines didácticos.

## Requisitos

- `gcc` (versiones secuenciales)
- Open MPI (`mpicc`, `mpirun`) para las versiones paralelas
  - Debian/Ubuntu: `sudo apt install openmpi-bin libopenmpi-dev`

> **Procesos vs. núcleos:** Open MPI cuenta **núcleos físicos**. Usa `-np` ≤ núcleos
> físicos (p. ej. 6 en un i5-10400) para tiempos limpios; para más procesos agrega
> `--oversubscribe`, pero eso introduce contención y sesga las mediciones.

## Compilación

```bash
make bruteforce_seq        # secuencial (gcc)
make cipher_tool           # cifrar/descifrar archivos (gcc)
make bruteforce_mpi        # naive (mpicc)
make bruteforce_roundrobin # round-robin (mpicc)
make bruteforce_master     # maestro-trabajador (mpicc)
```

Todos se compilan con los **mismos** flags de optimización (`-O3 -march=native
-funroll-loops`) para que los tiempos sean comparables entre secuencial y MPI.

## Ejecución

### Secuencial (sin MPI)
```bash
# Romper el cipher embebido del catedratico
./bruteforce_seq [bits]        # bits = cota 2^bits (por defecto 28)

# Texto propio con llave plantada (mismo workload que las versiones MPI)
./bruteforce_seq --file mensaje.txt --key 777777 --keyword "mi frase" --bits 24
./bruteforce_seq --key 777777 --bits 24        # texto interno
```

### Cifrar / descifrar un archivo (Parte B)
```bash
echo -n "Esta es una prueba de proyecto 2" > frase.txt
./cipher_tool encrypt frase.txt cifrado.bin 42
./cipher_tool decrypt cifrado.bin 42 "es una prueba de"
```

### Estrategias paralelas

Las tres aceptan los mismos flags: `--file <txt>`, `--key <k>` (planta una llave en un
texto para pruebas), `--keyword "<frase>"`, `--bits <n>`; `master` además tiene `--chunk`.
Sin flags, rompen el cipher embebido del catedratico (frase clave `" the "`).
Con `-np 1` corren en **modo secuencial** (línea base de speedup).

```bash
# Naive (reparto estático contiguo)
mpirun -np 4 ./bruteforce_mpi --bits 24

# Round-robin (reparto cíclico)
mpirun -np 4 ./bruteforce_roundrobin --bits 24

# Maestro-trabajador (reparto dinámico; necesita -np >= 2, el rank 0 coordina)
mpirun -np 4 ./bruteforce_master --bits 24

# Con texto propio y llave plantada (cualquiera de las tres)
mpirun -np 6 ./bruteforce_mpi --file mensaje.txt --key 777777 --keyword "mi frase" --bits 24
```

> El naive ya **no** acepta bits posicional: usa `--bits 24`, no `./bruteforce_mpi 24`.
> Requisitos de la llave: `2^bits > key`, y la `keyword` debe aparecer literal en el texto.

## Ejemplo paso a paso (mensaje propio)

Romper un mensaje propio con los 4 algoritmos, usando la misma llave y los mismos datos.

**Paso 1 (opcional) — crear el archivo de texto desde la terminal:**
```bash
printf 'El teorema de Pitagoras es fundamental en geometria' > mimensaje.txt
```

**Paso 2 — compilar los 4 programas:**
```bash
make bruteforce_seq bruteforce_mpi bruteforce_roundrobin bruteforce_master
```

**Paso 3 — correr cada algoritmo** con estos datos: llave plantada `777777`, frase clave
`"es fundamental"` (está dentro del mensaje) y cota de búsqueda `2^24`:

```bash
# 1) Secuencial (sin MPI)
./bruteforce_seq --file mimensaje.txt --key 777777 --keyword "es fundamental" --bits 24

# 2) Naive (reparto estático contiguo)
mpirun -np 6 ./bruteforce_mpi --file mimensaje.txt --key 777777 --keyword "es fundamental" --bits 24

# 3) Round-robin (reparto cíclico)
mpirun -np 6 ./bruteforce_roundrobin --file mimensaje.txt --key 777777 --keyword "es fundamental" --bits 24

# 4) Maestro-trabajador (reparto dinámico; el rank 0 coordina)
mpirun -np 6 ./bruteforce_master --file mimensaje.txt --key 777777 --keyword "es fundamental" --bits 24
```

Los cuatro deben imprimir `Llave hallada : 777777` y el texto descifrado. Para probar casos
más difíciles, sube `--key` (llave más al fondo del espacio) y `--bits` en conjunto, siempre
con `2^bits > key`.

## Benchmarks

```bash
# Rendimiento del secuencial por longitud de llave (barrido completo, peor caso)
make bench                              # -> resultados.csv

# Benchmark de una estrategia individual (speedup fácil/media/difícil, 1 corrida)
bash scripts/bench_naive.sh [bits]      # -> resultados_naive.csv
bash scripts/bench_roundrobin.sh [bits] # -> resultados_roundrobin.csv
bash scripts/bench_master.sh [bits]     # -> resultados_master.csv

# Benchmark CANÓNICO: las 3 estrategias + baseline, 1 máquina, mediana de 5 corridas
bash scripts/bench_robusto.sh           # -> resultados_benchmark.csv

# Gráfica de speedup a partir del CSV canónico (requiere matplotlib en un venv)
python3 -m venv .venv && .venv/bin/pip install matplotlib
.venv/bin/python scripts/plot_benchmark.py resultados_benchmark.csv speedup_comparativa.png
```

**Para comparar estrategias en el informe usa `resultados_benchmark.csv`** (una sola
máquina, mismos flags, mediana de 5). Mezclar tiempos de máquinas distintas o con flags de
compilación distintos invalida la comparación; en WSL2 la variación es mayor, por eso la
mediana. Los benchmarks individuales sirven para probar una estrategia aislada.

## Documentación

Pegar enlace a google docs
