#!/usr/bin/env bash
# Low-end sweep of bounded-dispatch batch size N in uts_opt2.cpp (N=4..10).
# Uses iter_count=4 to reduce per-run variance (each reported duration is the
# sum of 4 timed traversals). Leaves the file at the best N and restores the
# original iter_count.
set -euo pipefail

cd "$(dirname "$0")"
SRC=uts_opt2.cpp
EXE=build/uts_opt2
RUNS=5
EXPECTED=16526523

ORIG_N=$(grep -oP 'constexpr int N = \K[0-9]+' "$SRC")
ORIG_IT=$(grep -oP 'iter_count = \K[0-9]+' "$SRC")
set_n()  { sed -i "s/constexpr int N = [0-9]\+;/constexpr int N = ${1};/" "$SRC"; }
set_it() { sed -i "s/iter_count = [0-9]\+;/iter_count = ${1};/" "$SRC"; }

# Reduce per-run variance: 4 timed iterations per run.
set_it 4

printf "%4s  %10s  %10s  %s\n" "N" "min(us)" "median" "ok"
best_n=0; best_med=999999999
for n in $(seq 4 10); do
  set_n "$n"
  cmake --build ./build --target uts_opt2 >/dev/null 2>&1

  durs=(); ok="yes"
  for _ in $(seq "$RUNS"); do
    out=$("$EXE" 2>/dev/null)
    bad=$(echo "$out" | awk -v e="$EXPECTED" '/^output:/ && $2!=e{c++} END{print c+0}')
    [ "$bad" = "0" ] || ok="WRONG"
    durs+=("$(echo "$out" | awk '/duration/{print $2}')")
  done

  read -r mn md < <(printf "%s\n" "${durs[@]}" | sort -n | \
    awk '{a[NR]=$1} END{print a[1], a[int(NR/2)+1]}')
  printf "%4d  %10d  %10d  %s\n" "$n" "$mn" "$md" "$ok"
  if [ "$md" -lt "$best_med" ]; then best_med=$md; best_n=$n; fi
done

echo "----"
echo "best by median: N=${best_n} (${best_med} us over 4 iters)"

# Leave the file at the best N; restore iter_count for cross-variant parity.
set_n "$best_n"
set_it "$ORIG_IT"
cmake --build ./build --target uts_opt2 >/dev/null 2>&1
echo "left uts_opt2.cpp at N=${best_n}, iter_count=${ORIG_IT}"
