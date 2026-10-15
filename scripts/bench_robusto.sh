#!/usr/bin/env bash
# Benchmark ROBUSTO y consistente (una sola maquina, mismos flags, mismo workload).
# Para cada config corre REPS veces y reporta la MEDIANA (resistente a outliers de WSL2).
# Baseline = secuencial PURO (bruteforce_seq, sin MPI).  np=1 no se mide en MPI.
set -uo pipefail
cd "$(dirname "$0")/.."

BITS=22
REPS=5
NPS=(2 4 6)
KW="es una prueba de"
CSV=resultados_benchmark.csv
KEYS=("460000:facil" "2300000:media" "3800000:dificil")

timefield() { sed -n 's/^CSV,[^,]*,[0-9]*,[0-9]*,[0-9-]*,//p'; }

# mediana de los argumentos numericos
median() { printf '%s\n' "$@" | sort -n | awk '{a[NR]=$1} END{print (NR%2)?a[(NR+1)/2]:(a[NR/2]+a[NR/2+1])/2}'; }

echo "estrategia,categoria,np,bits,key,tiempo_s,speedup,reps" > "$CSV"
declare -A T1

echo ">> baseline secuencial puro (bruteforce_seq, mediana de $REPS)"
for pair in "${KEYS[@]}"; do
  key=${pair%:*}; cat=${pair#*:}; times=()
  for ((r=0;r<REPS;r++)); do
    t=$(./bruteforce_seq --key "$key" --bits "$BITS" --keyword "$KW" 2>&1 >/dev/null | timefield)
    times+=("$t")
  done
  m=$(median "${times[@]}"); T1[$cat]=$m
  echo "secuencial,$cat,1,$BITS,$key,$m,1.00,$REPS" | tee -a "$CSV"
done

for sb in "naive:bruteforce_mpi" "roundrobin:bruteforce_roundrobin" "master:bruteforce_master"; do
  strat=${sb%:*}; bin=./${sb#*:}
  echo ">> $strat (mediana de $REPS)"
  for pair in "${KEYS[@]}"; do
    key=${pair%:*}; cat=${pair#*:}
    for np in "${NPS[@]}"; do
      times=()
      for ((r=0;r<REPS;r++)); do
        t=$(mpirun -np "$np" "$bin" --key "$key" --bits "$BITS" --keyword "$KW" 2>&1 >/dev/null | timefield)
        times+=("$t")
      done
      m=$(median "${times[@]}")
      sp=$(awk -v a="${T1[$cat]}" -v b="$m" 'BEGIN{printf (b>0)?"%.2f":"NA", a/b}')
      echo "$strat,$cat,$np,$BITS,$key,$m,$sp,$REPS" | tee -a "$CSV"
    done
  done
done
echo ">> listo: $CSV"
