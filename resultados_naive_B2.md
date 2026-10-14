# Parte B.2 — Pruebas del naive con las llaves del enunciado

> Rol: **Persona 1**. Evidencia de ejecución del acercamiento **naive** (`bruteforce_mpi.c`)
> con **`-np 4`** y las tres llaves que pide el enunciado. Entorno: macOS, Apple clang,
> **OpenMPI 5.0.11**, `mpirun --oversubscribe`. Texto interno cifrado por self-cipher
> (`--key`), palabra clave `"es una prueba de"`, espacio `2^56`.

Recordatorio del reparto naive con `-np 4` y `bits=56`: el espacio `[0, 2^56)` se parte en
4 segmentos contiguos iguales de tamaño `2^54`:

| rank | segmento |
|------|----------|
| 0 | `[0, 2^54)` |
| 1 | `[2^54, 2·2^54)` |
| 2 | `[2·2^54, 3·2^54)` |
| 3 | `[3·2^54, 2^56)` |

## Caso a — llave `123456`

```
mpirun --oversubscribe -np 4 ./bruteforce_naive --key 123456 --bits 56 --keyword "es una prueba de"
```
```
=== Naive | procesos=4  cota=2^56 ===
Fuente        : <texto interno> (cifrado con llave 123456)
Palabra clave : "es una prueba de"
Llave hallada : 123456
Texto         : "Esta es una prueba de proyecto 2"
Tiempo (s)    : 0.2998
```
`123456` cae muy temprano dentro del segmento del **rank 0**, que la encuentra tras ~123 456
iteraciones. Termina rápido. Un secuencial recorrería las mismas ~123 456 llaves, así que el
**speedup es ≈ 1** (los otros 3 procesos no aportan: su segmento empieza mucho más allá).

## Caso b — llave `18014398509481983` (2⁵⁶/4, = 2⁵⁴−1)

```
mpirun --oversubscribe -np 4 ./bruteforce_naive --key 18014398509481983 --bits 56 --keyword "es una prueba de"
```
**No termina.** Tras **10 s** acotados el programa seguía buscando sin imprimir `Llave
hallada` (se abortó manualmente). Motivo: `2⁵⁴−1` es la **última** posición del segmento del
**rank 0** `[0, 2^54)`, así que ese proceso debe barrer **todo** su segmento (~2⁵⁴ ≈
1.8·10¹⁶ llaves) antes de hallarla, y ningún otro proceso la tiene.

Extrapolación con el rendimiento medido (~430 000 llaves/s por proceso, de
`resultados_naive.csv`): `2⁵⁴ / 430000 ≈ 4.2·10¹⁰ s ≈ ~1 300 años`. Es el **peor caso** del
naive: con `-np 4` el speedup frente al secuencial sería ≈ 1 (ambos barren ~2⁵⁴), es decir,
**la paralelización no ayuda**. (Coincide con el *spoiler* del enunciado: "se tardará mucho,
si es que termina".)

## Caso c — llave `18014398509481984` (2⁵⁶/4 +1, = 2⁵⁴)

```
mpirun --oversubscribe -np 4 ./bruteforce_naive --key 18014398509481984 --bits 56 --keyword "es una prueba de"
```
```
=== Naive | procesos=4  cota=2^56 ===
Fuente        : <texto interno> (cifrado con llave 18014398509481984)
Palabra clave : "es una prueba de"
Llave hallada : 18014398509481984
Texto         : "Esta es una prueba de proyecto 2"
Tiempo (s)    : 0.0025
```
`2⁵⁴` es **exactamente la primera** posición del segmento del **rank 1**, que la encuentra en
su **primera iteración** → `0.0025 s`. Un secuencial habría necesitado ~2⁵⁴ iteraciones, así
que el **speedup aparente es astronómico** (del orden de `2⁵⁴`), aunque el trabajo real fue
mínimo.

## Reflexión (B.2.d)

Las tres llaves son casi consecutivas (`2⁵⁴−1`, `2⁵⁴`) y sin embargo el comportamiento es
opuesto:

| llave | posición | tiempo `-np 4` | speedup aparente |
|-------|----------|----------------|------------------|
| `123456` | inicio del rank 0 | 0.2998 s | ≈ 1 |
| `2⁵⁴−1` | fin del rank 0 | **no termina** (~1 300 años) | ≈ 1 (peor caso) |
| `2⁵⁴` | inicio del rank 1 | 0.0025 s | ≈ 2⁵⁴ (enorme) |

**Conclusión:** en el naive el tiempo y el speedup **no dependen del número de procesos sino
de en qué segmento cae la llave**. Si cae al inicio del segmento de algún proceso, se halla
casi instantáneamente (speedup engañosamente alto); si cae al final del segmento del rank 0,
equivale al peor caso secuencial (sin speedup). Por eso el enunciado advierte que los
speedups del naive son **inconsistentes** y pueden dar **falsa confianza**: medir una sola
llave no dice nada del rendimiento real. Esto motiva los acercamientos alternativos
(round-robin y maestro-trabajador), que reparten el espacio de forma que ningún proceso quede
"atascado" en un bloque contiguo enorme.
