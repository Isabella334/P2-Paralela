#!/usr/bin/env bash
# Benchmark del acercamiento NAIVE (reparto estatico en segmentos contiguos)
#
# Mide el tiempo de romper una llave plantada en tres posiciones del espacio
# (facil / media / dificil) variando el numero de procesos, y calcula el
# speedup contra la linea base secuencial (-np 1).
#
# Usa las MISMAS llaves plantadas que bench_master.sh / bench_roundrobin.sh para
# que las tres estrategias sean comparables (Parte B.5).

set -euo pipefail
cd "$(dirname "$0")/.."

BITS="${1:-24}"                 # espacio = 2^BITS (24 -> termina en segundos)
NPS=(2 4 8)                     # configuraciones paralelas a medir
KEYWORD="es una prueba de"
BIN=./bruteforce_naive
CSV=resultados_naive.csv
# OpenMPI necesita --oversubscribe si pide mas ranks que cores
MPIRUN_FLAGS="${MPIRUN_FLAGS:-}"

command -v mpirun >/dev/null || { echo "falta mpirun (instala openmpi)"; exit 1; }
make -s bruteforce_naive

UPPER=$(( 1 << BITS ))
# Posiciones de la llave: temprana, media y tardia (todas dentro del espacio).
# Sin arreglos asociativos para ser compatible con el bash 3.2 de macOS.
key_for() { # categoria -> posicion de la llave en el espacio
    case "$1" in
        facil)   echo $(( UPPER/16 ))     ;;
        media)   echo $(( UPPER/2 ))      ;;
        dificil) echo $(( UPPER*13/16 ))  ;;
    esac
}

run_time() { # np key -> imprime segundos (del CSV en stderr del binario)
    local np=$1 key=$2
    mpirun $MPIRUN_FLAGS -np "$np" "$BIN" --key "$key" --bits "$BITS" \
        --keyword "$KEYWORD" 2>&1 >/dev/null | sed -n 's/^CSV,naive,[0-9]*,[0-9]*,[0-9-]*,//p'
}

echo "estrategia,categoria,np,bits,key,tiempo_s,speedup" > "$CSV"
for cat in facil media dificil; do
    k=$(key_for "$cat")
    t1=$(run_time 1 "$k")       # base secuencial para esta llave
    echo "naive,$cat,1,$BITS,$k,$t1,1.00" | tee -a "$CSV"
    for np in "${NPS[@]}"; do
        tn=$(run_time "$np" "$k")
        sp=$(awk -v a="$t1" -v b="$tn" 'BEGIN{printf (b>0)?"%.2f":"NA", a/b}')
        echo "naive,$cat,$np,$BITS,$k,$tn,$sp" | tee -a "$CSV"
    done
done
echo "-> $CSV"
