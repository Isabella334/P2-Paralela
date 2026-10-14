#!/usr/bin/env bash
# Benchmark de la estrategia Round-robin (reparto ciclico)
#
# Mide el tiempo de romper una llave plantada en tres posiciones del espacio
# (facil / media / dificil) variando el numero de procesos, y calcula el
# speedup contra la linea base secuencial (-np 1).

set -euo pipefail
cd "$(dirname "$0")/.."

BITS="${1:-24}"                 # espacio = 2^BITS (24 -> termina en segundos)
NPS=(2 4 8)                     # configuraciones paralelas a medir
KEYWORD="es una prueba de"
BIN=./bruteforce_roundrobin
CSV=resultados_roundrobin.csv
# OpenMPI necesita --oversubscribe si pide mas ranks que cores
MPIRUN_FLAGS="${MPIRUN_FLAGS:-}"

command -v mpirun >/dev/null || { echo "falta mpirun (instala openmpi)"; exit 1; }
make -s bruteforce_roundrobin

UPPER=$(( 1 << BITS ))
# Posiciones de la llave: temprana, media y tardia (todas dentro del espacio).
declare -A KEY=( [facil]=$(( UPPER/16 )) [media]=$(( UPPER/2 )) [dificil]=$(( UPPER*13/16 )) )

run_time() { # np key -> imprime segundos (del CSV en stderr del binario)
    local np=$1 key=$2
    mpirun $MPIRUN_FLAGS -np "$np" "$BIN" --key "$key" --bits "$BITS" \
        --keyword "$KEYWORD" 2>&1 >/dev/null | sed -n 's/^CSV,roundrobin,[0-9]*,[0-9]*,[0-9-]*,//p'
}

echo "estrategia,categoria,np,bits,key,tiempo_s,speedup" > "$CSV"
for cat in facil media dificil; do
    k=${KEY[$cat]}
    t1=$(run_time 1 "$k")       # base secuencial para esta llave
    echo "roundrobin,$cat,1,$BITS,$k,$t1,1.00" | tee -a "$CSV"
    for np in "${NPS[@]}"; do
        tn=$(run_time "$np" "$k")
        sp=$(awk -v a="$t1" -v b="$tn" 'BEGIN{printf (b>0)?"%.2f":"NA", a/b}')
        echo "roundrobin,$cat,$np,$BITS,$k,$tn,$sp" | tee -a "$CSV"
    done
done
echo "-> $CSV"
