# Demo de comunicación MPI

Programa pequeño que reproduce el **patrón de comunicación** de `bruteforce.c` sin usar
DES. Cada proceso busca un valor secreto (`TARGET`) en su parte del espacio `[0, SPACE)`.
El que lo encuentra avisa a todos, y P0 reporta quién lo encontró.

Primitivas que usa:

| Primitiva   | Dónde                   | Para qué                                            |
|-------------|-------------------------|-----------------------------------------------------|
| `MPI_Irecv` | antes del bucle         | escuchar el aviso desde `MPI_ANY_SOURCE`            |
| `MPI_Send`  | al encontrar el valor   | enviarlo a los `N` procesos (incluido él mismo)     |
| `MPI_Wait`  | después del bucle       | completar la recepción. P0 lee `status.MPI_SOURCE`  |
| `MPI_Test`  | cada 16 intentos        | dejar de buscar si otro ya lo encontró              |

La explicación de cada primitiva está en [`docs/02-primitivas-mpi.md`](../docs/02-primitivas-mpi.md)
y el diagrama en [`docs/03-diagrama-comunicacion.md`](../docs/03-diagrama-comunicacion.md).

## Requisitos

OpenMPI (probado con Open MPI 4.1.6 en WSL2 / Ubuntu):

```bash
sudo apt install openmpi-bin libopenmpi-dev
```

## Compilar y ejecutar

```bash
cd mpi_demo
make                                   # compila mpi_demo
make run                               # 4 procesos, TARGET=700, SPACE=1000
make run NP=8 TARGET=123 SPACE=1000    # con otros parámetros
# o directamente:
mpirun --oversubscribe -np 4 ./mpi_demo <target> <espacio> <delay_us>
```

- `delay_us` es una pausa por intento (por defecto 1000 µs) para que la ejecución dure lo
  suficiente y se vea la comunicación. Con `0` corre a máxima velocidad.
- `--oversubscribe` permite usar más procesos que núcleos disponibles.

## Salida de ejemplo (real)

`make run NP=4` (TARGET=700, está en el rango de P2):

```
[  0.0000 s] P0: rango [0, 250)
[  0.0003 s] P0: MPI_Irecv publicado (ANY_SOURCE), comienza la busqueda
[  0.0000 s] P1: rango [250, 500)
[  0.0003 s] P1: MPI_Irecv publicado (ANY_SOURCE), comienza la busqueda
[  0.0000 s] P2: rango [500, 750)
[  0.0003 s] P2: MPI_Irecv publicado (ANY_SOURCE), comienza la busqueda
[  0.0000 s] P3: rango [750, 1000)
[  0.0002 s] P3: MPI_Irecv publicado (ANY_SOURCE), comienza la busqueda
[  0.2241 s] P2: ENCONTRE 700 -> MPI_Send a los 4 procesos
[  0.2319 s] P0: MPI_Test: P2 ya encontro 700 -> detengo la busqueda (intento 208 de 250)
[  0.2320 s] P0: MPI_Wait completo: llave = 700, enviada por P2 (tag 0)

Resumen: target=700 espacio=1000 procesos=4
  P0 probo 208 candidatos
  P1 probo 208 candidatos
  P2 probo 201 candidatos  <- encontro la llave
  P3 probo 208 candidatos
[  0.2319 s] P1: MPI_Test: P2 ya encontro 700 -> detengo la busqueda (intento 208 de 250)
[  0.2320 s] P3: MPI_Test: P2 ya encontro 700 -> detengo la busqueda (intento 208 de 250)
```

Cómo leerla:

- Todos publican su `MPI_Irecv` al inicio, antes de buscar.
- P2 encuentra el 700 tras 201 intentos (700 − 500 + 1) y hace `MPI_Send` a los 4.
- P0, P1 y P3 detectan el aviso con `MPI_Test` y **se detienen** sin terminar su rango
  (208 de 250 intentos).
- P0 reporta que el remitente fue P2, tomado de `status.MPI_SOURCE`.
- Las líneas pueden aparecer desordenadas porque `mpirun` reenvía la salida de cada
  proceso por separado. El tiempo entre corchetes indica el orden real.

## Casos probados

| Procesos | TARGET | Quién lo encuentra      | Resultado |
|----------|--------|-------------------------|-----------|
| 1        | 700    | P0 (se envía a sí mismo)| OK        |
| 2        | 700    | P1                      | OK        |
| 8        | 700    | P5                      | OK        |
| 4        | 0      | P0 (primer candidato)   | OK        |
| 4        | 250    | P1 (inicio de su rango) | OK        |
| 4        | 999    | P3 (último candidato); P0-P2 agotan su rango y esperan en `MPI_Wait` | OK |

En todos los casos todos los procesos terminan y ninguno se queda colgado.
