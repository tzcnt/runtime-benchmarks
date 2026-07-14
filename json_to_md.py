#!/usr/bin/env python3

# Regenerate the markdown summary table (RESULTS.md) from an existing results
# JSON, without rebuilding or re-running anything. Useful after combining
# standalone runs with merge_results.py, which updates the .json but not the .md.
#
# The tables and the collation / table-generation code below are copied from
# build_and_bench_all.py (which can't be imported as a module - importing it
# would run the whole benchmark sweep) and must be kept in sync with it, so the
# output is byte-identical to what a full run would write.

import json
import os
import sys

runtime_links = {
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
    "smol": "https://github.com/smol-rs/smol",
    "rayon": "https://github.com/rayon-rs/rayon",
    "chili": "https://github.com/dragostis/chili",
    "forte": "https://github.com/NthTensor/Forte",
    # bevy_tasks is keyed as "bevy" because links are resolved via the runtime
    # name's prefix before the first underscore (bevy_tasks -> bevy).
    "bevy": "https://github.com/bevyengine/bevy/tree/main/crates/bevy_tasks",
    "micropool": "https://github.com/DouglasDwyer/micropool",
    "beekeeper": "https://github.com/jdidion/beekeeper",
    # monoio / glommio: thread-per-core, io_uring async runtimes. io_socket_st only.
    "monoio": "https://github.com/bytedance/monoio",
    "glommio": "https://github.com/DataDog/glommio",
    "corosio": "https://github.com/cppalliance/corosio",
    "go": "https://pkg.go.dev/std",
    "dotnet": "https://learn.microsoft.com/en-us/dotnet/api/",
    "java": "https://openjdk.org/jeps/444",
    "forkjoin": "https://docs.oracle.com/en/java/javase/25/docs/api/java.base/java/util/concurrent/ForkJoinPool.html",
    "kotlin": "https://github.com/Kotlin/kotlinx.coroutines",
    "weave": "https://github.com/mratsim/weave",
    "neco": "https://github.com/tidwall/neco",
    "zap": "https://github.com/kprotty/zap",
    "zigbeam": "https://github.com/eakova/zigbeam",
    "spice": "https://github.com/judofyr/spice"
}

benchmarks_order = ["skynet", "nqueens", "fib", "matmul", "channel", "io_socket_st"]

collect_results = {
    "fib": [{"params": "39"}],
    "skynet": [{"params": ""}],
    "nqueens": [{"params": ""}],
    "matmul": [{"params": "2048"}],
    "channel": [{"params": ""}],
    "io_socket_st": [{"params": ""}]
}

def get_dur_in_us(dur_string):
    dur, unit = dur_string.split(" ")
    dur = int(dur)
    # convert all units to microseconds for comparison
    if unit == "us":
        return dur
    elif unit == "ms":
        return dur * 1000
    elif unit == "s":
        return dur * 1000000
    elif unit == "sec":
        return dur * 1000000
    else:
        print(f"Unknown unit: {unit}")
        sys.exit(1)


def format_dnf_label(dur_us):
    # Render a DNF's recorded ceiling duration as a short label for the
    # RESULTS.md table, e.g. 600000000 us -> "DNF (10m)". Derived from the
    # recorded duration (not BENCHMARK_TIMEOUT_SECONDS) so results produced
    # under a RUNTIME_BENCHMARKS_TIMEOUT_SECONDS override label correctly.
    secs = round(dur_us / 1_000_000)
    if secs >= 60 and secs % 60 == 0:
        return f"DNF ({secs // 60}m)"
    return f"DNF ({secs}s)"


def generate_markdown(full_results):
    # full_results is used to produce the .json for charting
    # collated_results is used to produce the .md summary for the README
    collated_results = {}
    bench_names = []
    for runtime, runtime_results in full_results.items():
        for bench_name in benchmarks_order:
            if bench_name not in runtime_results:
                continue
            collect = collect_results[bench_name]
            for collect_item in collect:
                params = collect_item["params"]
                bench_results = runtime_results[bench_name]
                last_result = bench_results[len(bench_results) - 1]
                friendly_name = bench_name
                if params:
                    friendly_name += f"({params})"
                dur_string = last_result["result"]["duration"]
                dur_in_us = get_dur_in_us(dur_string)
                if last_result["result"].get("dnf"):
                    # A DNF carries the timeout-ceiling duration. Show it as an
                    # explicit "DNF (10m)" label, but keep the ceiling in `us` so
                    # the ratio math ranks the run as a worst-case result instead
                    # of dropping it from the mean.
                    collated_results.setdefault(runtime, {})[friendly_name] = {"raw": format_dnf_label(dur_in_us), "us": dur_in_us, "dnf": True}
                else:
                    collated_results.setdefault(runtime, {})[friendly_name] = {"raw": dur_string, "us": dur_in_us}
                if not friendly_name in bench_names:
                    bench_names.append(friendly_name)

    # Group benchmarks by which runtimes have results for them
    # Key: frozenset of runtime names, Value: list of benchmark names
    bench_to_runtimes = {}
    for bench_name in bench_names:
        runtimes_with_bench = frozenset(
            runtime for runtime, runtime_results in collated_results.items()
            if bench_name in runtime_results
        )
        bench_to_runtimes[bench_name] = runtimes_with_bench

    # Invert: group benchmarks by their runtime sets
    runtime_set_to_benchmarks = {}
    for bench_name, runtime_set in bench_to_runtimes.items():
        runtime_set_to_benchmarks.setdefault(runtime_set, []).append(bench_name)

    outMD = ""

    # Process each group separately
    for runtime_set, group_bench_names in runtime_set_to_benchmarks.items():
        group_runtimes = list(runtime_set)

        # Calculate lowest results for this group's benchmarks. A DNF's ceiling
        # duration never defines the best time (only real finishes can), but DNFs
        # do get a ratio against that best in the loop below. If every runtime
        # DNFs a benchmark there is no best; that benchmark is then excluded from
        # the means and only the "DNF (10m)" labels appear.
        lowest_results = {}
        for runtime in group_runtimes:
            runtime_results = collated_results[runtime]
            for bench_name in group_bench_names:
                if bench_name in runtime_results:
                    us = runtime_results[bench_name]["us"]
                    if us is None or runtime_results[bench_name].get("dnf"):
                        continue
                    curr_lowest = lowest_results.get(bench_name, sys.maxsize)
                    if us < curr_lowest:
                        lowest_results[bench_name] = us

        # Calculate ratios for this group. DNFs get a ratio from their recorded
        # ceiling duration; benches nobody finished have no best and are skipped.
        for runtime in group_runtimes:
            runtime_results = collated_results[runtime]
            for bench_name in group_bench_names:
                if bench_name in runtime_results:
                    us = runtime_results[bench_name]["us"]
                    if us is None or bench_name not in lowest_results:
                        continue
                    ratio = float(us) / float(lowest_results[bench_name])
                    runtime_results[bench_name]["ratio"] = ratio

        # Sort runtimes by mean ratio within this group. A DNF contributes its
        # ceiling-duration ratio to the mean (a worst-case penalty, not a gap);
        # benches nobody finished have no best and thus no ratio. A runtime with
        # no ratios at all has mean None and sorts last.
        sorted_runtimes = []
        for runtime in group_runtimes:
            runtime_results = collated_results[runtime]
            ratios = [runtime_results[b]["ratio"] for b in group_bench_names
                      if b in runtime_results and "ratio" in runtime_results[b]]
            mean = sum(ratios) / len(ratios) if ratios else None
            sorted_runtimes.append({"runtime": runtime, "mean": mean})
        sorted_runtimes.sort(key=lambda x: (x["mean"] is None, x["mean"] if x["mean"] is not None else 0.0))

        # Build output array for this group
        output_array = [["Runtime", "Mean Ratio to Best<br>(lower is better)"] + group_bench_names]
        for runtime in sorted_runtimes:
            runtime_name = runtime["runtime"]
            runtime_mean = runtime["mean"]
            base_runtime = runtime_name.split("_")[0]
            mean_str = "{:.2f}x".format(runtime_mean) if runtime_mean is not None else "DNF"
            runtime_output = [
                f"[{runtime_name}]({runtime_links.get(base_runtime, '')})",
                mean_str
            ]
            runtime_results = collated_results[runtime_name]
            for bench in group_bench_names:
                runtime_output.append(runtime_results[bench]["raw"])
            output_array.append(runtime_output)

        # Generate table for this group
        len_y = len(output_array[0])
        len_x = len(output_array)

        # Create header row
        for x in range(len_x):
            outMD += f"| {output_array[x][0]} "
        outMD += "|\n"
        # Create markdown table header row
        for x in range(len_x):
            outMD += "| --- "
        outMD += "|\n"
        # Create data rows
        for y in range(1, len_y):
            for x in range(len_x):
                outMD += f"| {output_array[x][y]} "
            outMD += "|\n"
        outMD += "\n"

    # --- Generate Memory Usage Table ---
    outMD += "\n\n### Peak Memory Usage (Max RSS)\n\n"
    mem_table = [["Runtime"] + bench_names]

    for runtime in collated_results.keys():
        row = [runtime]
        for bench_friendly in bench_names:
            orig_name = bench_friendly.split("(")[0]
            try:
                last_run = full_results[runtime][orig_name][-1]
                row.append(last_run["result"].get("max_rss", "N/A"))
            except:
                row.append("N/A")
        mem_table.append(row)

    # Render it
    for y in range(len(mem_table[0])):
        for x in range(len(mem_table)):
            outMD += f"| {mem_table[x][y]} "
        outMD += "|\n"
        if y == 0: # Header separator
            for _ in range(len(mem_table)):
                outMD += "| --- "
            outMD += "|\n"

    return outMD.strip() + "\n"


def print_usage():
    print("Usage:")
    print("  ./json_to_md.py <results.json> [output.md]")
    print("Regenerates the markdown summary table from a results JSON produced by")
    print("build_and_bench_all.py (or merged with merge_results.py). The output")
    print("path defaults to the input path with its extension replaced by .md")
    print("(e.g. RESULTS.json -> RESULTS.md).")


if __name__ == "__main__":
    args = sys.argv[1:]
    if len(args) not in (1, 2):
        print_usage()
        sys.exit(1)

    json_path = args[0]
    md_path = args[1] if len(args) == 2 else os.path.splitext(json_path)[0] + ".md"

    with open(json_path, "r") as f:
        full_results = json.load(f)["results"]

    print(f"Generating {md_path}...", end=" ")
    with open(md_path, "w") as resultsMD:
        resultsMD.write(generate_markdown(full_results))
    print("done.")
