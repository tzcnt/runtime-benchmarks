#!/usr/bin/env bash
# Sweep the bounded-dispatch batch size N in uts_opt2.cpp from 8..32, rebuilding
# and benchmarking each value. Reports min/median duration (us) per N.
set -euo pipefail

cd "$(dirname "$0")"
SRC=uts_opt2.cpp
EXE=build/uts_opt2
RUNS=6
EXPECTED=16526523

# Remember the original N to restore on exit.
ORIG_N=$(grep -oP 'constexpr int N = \K[0-9]+' "$SRC")
restore() { sed -i "s/constexpr int N = [0-9]\+;/constexpr int N = ${ORIG_N};/" "$SRC"; }
trap restore EXIT

printf "%4s  %9s  %9s  %s\n" "N" "min(us)" "median" "ok"
best_n=0; best_med=99999999

for n in $(seq 8 32); do
  sed -i "s/constexpr int N = [0-9]\+;/constexpr int N = ${n};/" "$SRC"
  cmake --build ./build --target uts_opt2 >/dev/null 2>&1

  durs=(); ok="yes"
  for _ in $(seq "$RUNS"); do
    out=$("$EXE" 2>/dev/null)
    o=$(echo "$out" | awk '/^output:/{print $2}')
    [ "$o" = "$EXPECTED" ] || ok="WRONG($o)"
    durs+=("$(echo "$out" | awk '/duration/{print $2}')")
  done

  read -r mn md < <(printf "%s\n" "${durs[@]}" | sort -n | \
    awk '{a[NR]=$1} END{print a[1], a[int(NR/2)+1]}')
  printf "%4d  %9d  %9d  %s\n" "$n" "$mn" "$md" "$ok"

  if [ "$md" -lt "$best_med" ]; then best_med=$md; best_n=$n; fi
done

echo "----"
echo "best by median: N=${best_n} (${best_med} us)"
