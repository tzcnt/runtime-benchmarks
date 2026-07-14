#!/usr/bin/env python3

# Convert a RESULTS.json into a RESULTS.md summary table

import json
import sys
from collections import defaultdict

# Keep in sync with runtime_links in build_and_bench_all.py.
RUNTIME_URLS = {
    "citor": "https://github.com/Lallapallooza/citor",
    "libfork": "https://github.com/ConorWilliams/libfork",
    "TooManyCooks": "https://github.com/tzcnt/TooManyCooks",
    "tbb": "https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html",
    "taskflow": "https://github.com/taskflow/taskflow",
    "cppcoro": "https://github.com/andreasbuhr/cppcoro",
    "coros": "https://github.com/mtmucha/coros",
    "cobalt": "https://github.com/boostorg/cobalt",
    "folly": "https://github.com/facebook/folly",
    "concurrencpp": "https://github.com/David-Haim/concurrencpp",
    "HPX": "https://github.com/STEllAR-GROUP/hpx",
    "libcoro": "https://github.com/jbaldwin/libcoro",
    "userver": "https://github.com/userver-framework/userver",
    "PhotonLibOS": "https://github.com/alibaba/PhotonLibOS",
    "tokio": "https://github.com/tokio-rs/tokio",
    "go": "https://pkg.go.dev/std",
    "dotnet": "https://learn.microsoft.com/en-us/dotnet/api/",
    "java": "https://openjdk.org/jeps/444",
    "forkjoin": "https://docs.oracle.com/en/java/javase/25/docs/api/java.base/java/util/concurrent/ForkJoinPool.html",
    "kotlin": "https://github.com/Kotlin/kotlinx.coroutines",
    "weave": "https://github.com/mratsim/weave",
    "neco": "https://github.com/tidwall/neco",
    "zap": "https://github.com/kprotty/zap",
    "zigbeam": "https://github.com/eakova/zigbeam",
}

def parse_duration(duration_str):
    if duration_str == "DNF":
        return None
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
            if bench_name == "metadata":
                continue  # per-runtime compiler/timestamp block, not a benchmark
            highest = get_highest_thread_result(bench_results)
            if highest:
                params = highest["params"]
                display_name = f"{bench_name}({params})" if params else bench_name
                # A DNF has no numeric duration; keep it as a None sentinel so it
                # renders as "DNF" and is excluded from the mean-ratio calc.
                duration = None if highest["result"].get("dnf") else parse_duration(highest["result"]["duration"])
                runtime_benchmarks[runtime][display_name] = duration
                all_benchmarks.add(display_name)

    all_benchmarks = sorted(all_benchmarks)

    best_per_benchmark = {}
    for bench in all_benchmarks:
        finished = [
            runtime_benchmarks[rt][bench]
            for rt in runtime_benchmarks
            if runtime_benchmarks[rt].get(bench) is not None
        ]
        best_per_benchmark[bench] = min(finished) if finished else None

    runtime_ratios = {}
    for runtime in runtime_benchmarks:
        ratios = []
        for bench in all_benchmarks:
            value = runtime_benchmarks[runtime].get(bench)
            if value is not None and best_per_benchmark[bench] is not None:
                ratios.append(value / best_per_benchmark[bench])
        runtime_ratios[runtime] = sum(ratios) / len(ratios) if ratios else float("inf")

    sorted_runtimes = sorted(runtime_benchmarks.keys(), key=lambda r: runtime_ratios[r])

    def format_runtime(rt):
        # Result keys can carry a config suffix (e.g. TooManyCooks_st_asio,
        # tokio_flume, kotlin_fjp); fall back to the base key before the first
        # underscore for the URL lookup, matching build_and_bench_all.py.
        url = RUNTIME_URLS.get(rt) or RUNTIME_URLS.get(rt.split("_")[0], "")
        return f"[{rt}]({url})" if url else rt

    header = "| Runtime | " + " | ".join(format_runtime(rt) for rt in sorted_runtimes) + " |"
    separator = "| --- | " + " | ".join("---" for _ in sorted_runtimes) + " |"

    def format_ratio(v):
        return f"{v:.2f}x" if v != float("inf") else "DNF"

    ratio_row = "| Mean Ratio to Best<br>(lower is better) | " + " | ".join(
        format_ratio(runtime_ratios[rt]) for rt in sorted_runtimes
    ) + " |"

    def format_cell(rt, bench):
        if bench not in runtime_benchmarks[rt]:
            return "N/A"
        value = runtime_benchmarks[rt][bench]
        return "DNF" if value is None else f"{value} us"

    bench_rows = []
    for bench in all_benchmarks:
        row = f"| {bench} | " + " | ".join(
            format_cell(rt, bench) for rt in sorted_runtimes
        ) + " |"
        bench_rows.append(row)

    output_lines = [header, separator, ratio_row] + bench_rows
    output = "\n".join(output_lines)

    with open(output_file, "w") as f:
        f.write(output + "\n")

    print(output)

if __name__ == "__main__":
    main()
