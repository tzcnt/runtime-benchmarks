#!/usr/bin/env python3

# Convert a RESULTS.json into a RESULTS.md summary table

import json
import sys
from collections import defaultdict

RUNTIME_URLS = {
    "libfork": "https://github.com/ConorWilliams/libfork",
    "TooManyCooks": "https://github.com/tzcnt/TooManyCooks",
    "tbb": "https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html",
    "cppcoro": "https://github.com/andreasbuhr/cppcoro",
    "taskflow": "https://github.com/taskflow/taskflow",
    "coros": "https://github.com/mtmucha/coros",
    "HPX": "https://github.com/STEllAR-GROUP/hpx",
    "concurrencpp": "https://github.com/David-Haim/concurrencpp",
    "libcoro": "https://github.com/jbaldwin/libcoro",
}

def parse_duration(duration_str):
    return int(duration_str.replace(" us", ""))

def get_highest_thread_result(benchmark_results):
    max_threads = max(r["threads"] for r in benchmark_results)
    for r in benchmark_results:
        if r["threads"] == max_threads:
            return r
    return None

def main():
    input_file = sys.argv[1] if len(sys.argv) > 1 else "RESULTS.json"
    output_file = sys.argv[2] if len(sys.argv) > 2 else "RESULTS.md"

    with open(input_file) as f:
        data = json.load(f)

    results = data["results"]
    
    runtime_benchmarks = defaultdict(dict)
    all_benchmarks = set()

    for runtime, benchmarks in results.items():
        for bench_name, bench_results in benchmarks.items():
            highest = get_highest_thread_result(bench_results)
            if highest:
                duration = parse_duration(highest["result"]["duration"])
                params = highest["params"]
                display_name = f"{bench_name}({params})" if params else bench_name
                runtime_benchmarks[runtime][display_name] = duration
                all_benchmarks.add(display_name)

    all_benchmarks = sorted(all_benchmarks)

    best_per_benchmark = {}
    for bench in all_benchmarks:
        best = min(
            runtime_benchmarks[rt].get(bench, float("inf"))
            for rt in runtime_benchmarks
        )
        best_per_benchmark[bench] = best

    runtime_ratios = {}
    for runtime in runtime_benchmarks:
        ratios = []
        for bench in all_benchmarks:
            if bench in runtime_benchmarks[runtime]:
                ratio = runtime_benchmarks[runtime][bench] / best_per_benchmark[bench]
                ratios.append(ratio)
        runtime_ratios[runtime] = sum(ratios) / len(ratios) if ratios else float("inf")

    sorted_runtimes = sorted(runtime_benchmarks.keys(), key=lambda r: runtime_ratios[r])

    def format_runtime(rt):
        url = RUNTIME_URLS.get(rt, "")
        return f"[{rt}]({url})" if url else rt

    header = "| Runtime | " + " | ".join(format_runtime(rt) for rt in sorted_runtimes) + " |"
    separator = "| --- | " + " | ".join("---" for _ in sorted_runtimes) + " |"

    ratio_row = "| Mean Ratio to Best<br>(lower is better) | " + " | ".join(
        f"{runtime_ratios[rt]:.2f}x" for rt in sorted_runtimes
    ) + " |"

    bench_rows = []
    for bench in all_benchmarks:
        row = f"| {bench} | " + " | ".join(
            f"{runtime_benchmarks[rt].get(bench, 'N/A')} us"
            if bench in runtime_benchmarks[rt] else "N/A"
            for rt in sorted_runtimes
        ) + " |"
        bench_rows.append(row)

    output_lines = [header, separator, ratio_row] + bench_rows
    output = "\n".join(output_lines)

    with open(output_file, "w") as f:
        f.write(output + "\n")

    print(output)

if __name__ == "__main__":
    main()
