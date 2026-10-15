#!/usr/bin/env python3
"""Grafica de speedup por estrategia (matplotlib). Small multiples: un panel por
dificultad; lineas de speedup vs np por estrategia, con referencia secuencial=1.
"""
import csv, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import MultipleLocator

SRC = sys.argv[1] if len(sys.argv) > 1 else "resultados_benchmark.csv"
OUT = sys.argv[2] if len(sys.argv) > 2 else "speedup_comparativa.png"

# Paleta categorica validada (dataviz skill, slots 1-3, light mode)
COLOR = {"naive": "#2a78d6", "roundrobin": "#eb6834", "master": "#1baf7a"}
INK, INK2 = "#0b0b0b", "#52514e"
STRATS = ["naive", "roundrobin", "master"]
CATS   = ["facil", "media", "dificil"]
CATLBL = {"facil": "Llave facil", "media": "Llave media", "dificil": "Llave dificil"}

data = {c: {s: [] for s in STRATS} for c in CATS}
bits = "?"
for r in csv.DictReader(open(SRC)):
    s, c = r["estrategia"], r["categoria"]
    if s in STRATS and c in CATS:
        data[c][s].append((int(r["np"]), float(r["speedup"])))
        bits = r["bits"]
for c in CATS:
    for s in STRATS:
        data[c][s].sort()

nps = sorted({np for c in CATS for s in STRATS for np, _ in data[c][s]})
ymax = max(1.0, max(sp for c in CATS for s in STRATS for _, sp in data[c][s]))

fig, axes = plt.subplots(1, 3, figsize=(12, 4.2), sharey=True)
fig.suptitle("Speedup por estrategia vs. numero de procesos", fontsize=15,
             fontweight="bold", color=INK, x=0.065, ha="left", y=0.98)
fig.text(0.065, 0.905,
         f"bits=2^{bits} | baseline = secuencial puro | mediana de 5 corridas | i5-10400, 6 cores",
         fontsize=10, color=INK2, ha="left")

for ax, c in zip(axes, CATS):
    ax.axhline(1, color=INK2, lw=1.2, ls=(0, (5, 4)), zorder=1)
    for s in STRATS:
        pts = data[c][s]
        if not pts:
            continue
        xs = [np for np, _ in pts]; ys = [sp for _, sp in pts]
        ax.plot(xs, ys, color=COLOR[s], lw=2.3, marker="o", ms=6,
                mec="white", mew=1.3, label=s, zorder=3)
        # etiqueta selectiva: solo el ultimo punto (np maximo).
        # master suele quedar justo debajo de roundrobin: su etiqueta va abajo.
        np_last, sp_last = pts[-1]
        dy = -15 if s == "master" else 8
        ax.annotate(f"{sp_last:.1f}", (np_last, sp_last), textcoords="offset points",
                    xytext=(0, dy), ha="center", fontsize=10, fontweight="bold", color=INK)
    ax.set_title(CATLBL[c], fontsize=12, fontweight="600", color=INK)
    ax.set_xlabel("procesos (np)", fontsize=10.5, color=INK2)
    ax.set_xticks(nps)
    ax.set_xlim(min(nps) - 0.3, max(nps) + 0.3)
    ax.set_ylim(0, ymax * 1.08)
    ax.yaxis.set_major_locator(MultipleLocator(1))
    ax.grid(axis="y", color="#e5e4e0", lw=0.9)
    ax.tick_params(colors=INK2, labelsize=10)
    for sp in ("top", "right"):
        ax.spines[sp].set_visible(False)
    for sp in ("left", "bottom"):
        ax.spines[sp].set_color("#d8d7d2")

axes[0].set_ylabel("speedup (T_sec / T_par)", fontsize=10.5, color=INK2)
axes[2].annotate("secuencial = 1", (max(nps) + 0.28, 1), textcoords="offset points",
                 xytext=(0, 4), ha="right", fontsize=9.5, color=INK2)
axes[0].legend(frameon=False, fontsize=10.5, loc="upper left",
               bbox_to_anchor=(0.0, 1.0), labelcolor=INK)

fig.subplots_adjust(left=0.065, right=0.985, top=0.80, bottom=0.12, wspace=0.08)
fig.savefig(OUT, dpi=150)
print("escrito", OUT)
