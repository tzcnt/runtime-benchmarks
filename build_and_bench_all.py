#!/usr/bin/env python3

# build and execute every benchmark in every runtime
# collate the results and sort them according to Mean Ratio to Best
# update the README with the table of results

import datetime
import json
import os
import subprocess
import yaml
import sys
import ast
import platform
import shutil

runtimes = {
    "cpp": ["citor", "libfork", "TooManyCooks", "tbb", "taskflow", "cppcoro", "coros", "cobalt",
            # these 4 are quite slow - you can remove them to speed up total runtime
            "folly", "concurrencpp", "HPX", "libcoro"]
}

LIBRARY_REF_ENV_VAR = "RUNTIME_BENCHMARKS_LIBRARY_REF"

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
    "libcoro": "https://github.com/jbaldwin/libcoro"
}

benchmarks_order = ["skynet", "nqueens", "fib", "matmul", "channel", "io_socket_st"]

benchmarks={
    "skynet": {

    },
    "fib": {
        "params": ["39"]
    },
    "nqueens": {

    },
    "matmul": {
        "params": ["2048"]
    },
    "channel": {

    },
    "io_socket_st": {

    }
}

# Defines which runtime+benchmark combos support multi-config execution
# If a runtime+benchmark is listed, each config will be appended as a command argument
# The runtime name will be suffixed with "_<config>" in output (e.g., "cobalt_st_asio")
# Format: { "runtime": { "benchmark": ["config1", "config2", ...], ... }, ... }
benchmark_configs = {
    "cobalt": {
        "channel": ["st_asio"]
    },
    "libcoro": {
        "channel": ["mt"]
    },
    "TooManyCooks": {
        "channel": ["st_asio", "mt"]
    },
}

def print_usage():
    runtime_list = ", ".join(runtime for runtime_names in runtimes.values() for runtime in runtime_names)
    print("Usage:")
    print("Benchmark all runtimes:")
    print("  ./build_and_bench_all.py")
    print("  ./build_and_bench_all.py full")
    print("Benchmark a specific runtime (git-ref can be a SHA, tag, or branch):")
    print("  ./build_and_bench_all.py <runtime> [git-ref]")
    print("  ./build_and_bench_all.py compare <runtime> <new-git-ref> [baseline-git-ref]")
    print(f"\nRuntimes: {runtime_list}")

def parse_args():
    args = sys.argv[1:]
    all_runtimes = [runtime for runtime_names in runtimes.values() for runtime in runtime_names]

    if not args:
        return {
            "full_sweep": False,
            "compare_runtime": None,
            "compare_new_ref": None,
            "compare_baseline_ref": None,
            "single_runtime": None,
            "single_ref": None,
        }

    if args == ["full"]:
        return {
            "full_sweep": True,
            "compare_runtime": None,
            "compare_new_ref": None,
            "compare_baseline_ref": None,
            "single_runtime": None,
            "single_ref": None,
        }

    if args[0] == "compare":
        args = args[1:]

        if len(args) not in (2, 3):
            print_usage()
            sys.exit(1)

        runtime = args[0]
        if runtime not in all_runtimes:
            print(f"Unknown runtime: {runtime}\n")
            print_usage()
            sys.exit(1)

        return {
            "full_sweep": True,
            "compare_runtime": runtime,
            "compare_new_ref": args[1],
            "compare_baseline_ref": args[2] if len(args) == 3 else None,
            "single_runtime": None,
            "single_ref": None,
        }

    if len(args) not in (1, 2):
        print_usage()
        sys.exit(1)

    runtime = args[0]
    if runtime not in all_runtimes:
        print(f"Unknown runtime: {runtime}\n")
        print_usage()
        sys.exit(1)

    return {
        "full_sweep": True,
        "compare_runtime": None,
        "compare_new_ref": None,
        "compare_baseline_ref": None,
        "single_runtime": runtime,
        "single_ref": args[1] if len(args) == 2 else None,
    }

collect_results = {
    "fib": [{"params": "39"}],
    "skynet": [{"params": ""}],
    "nqueens": [{"params": ""}],
    "matmul": [{"params": "2048"}],
    "channel": [{"params": ""}],
    "io_socket_st": [{"params": ""}]
}

# Fallback to a shell script for hardware core count detection if the user didn't build TMC
def get_nproc_fallback():
    try:
        return int(subprocess.run(args=f"./get_nproc.sh", shell=True, capture_output=True, text=True).stdout)
    except:
        return 8

def get_threads_sweep_fallback(full_sweep):
    proc = get_nproc_fallback()
    if not full_sweep:
        return [proc] # Only test at max cores if user didn't pass "full" arg
    result = []
    for t in [1,2,4,8,16,32,64]:
        if t < proc:
            result.append(t)
        if t >= proc:
            result.append(proc)
            break
    return result

# This executable produces a sweep from 1 to NCORES, but inserts breakpoints at any relevant breakpoints,
# e.g. at the number of P-cores. It uses TooManyCooks's hardware detection capabilities but isn't part of the benchmark suite.
def get_threads_sweep(full_sweep):
    try:
        s = subprocess.run(args=f"./cpp/TooManyCooks/build/threads_sweep", shell=True, capture_output=True, text=True).stdout
        sweep = ast.literal_eval(s)
        if not full_sweep:
            return [sweep[-1]] # Only test at max cores if user didn't pass "full" arg
        return sweep
    except:
        return get_threads_sweep_fallback(full_sweep)

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
        exit(1)


def format_mem(kib_string):
    # kib_string looks like "1234 KiB"
    ki = int(kib_string.split(" ")[0])
    mi = ki / 1024
    if mi >= 1024:
        return f"{round(mi / 1024, 2)} GB"
    return f"{round(mi, 2)} MB"

root_dir = os.path.abspath(os.path.dirname(__file__))

md = {"start_time": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")}
full_results = {}

def get_language_for_runtime(runtime):
    for language, runtime_names in runtimes.items():
        if runtime in runtime_names:
            return language
    return None

def build_runtime(language, runtime, library_ref=None, clean_build=False):
    runtime_root_dir = os.path.join(root_dir, language, runtime)
    display_ref = f" ({library_ref})" if library_ref else ""
    print(f"Building {runtime}{display_ref}")

    if clean_build:
        build_dir = os.path.join(runtime_root_dir, "build")
        if os.path.exists(build_dir):
            shutil.rmtree(build_dir)

    build_script = os.path.join(runtime_root_dir, "build_all.sh")
    if platform.system() == "Darwin":
        build_script += " clang-macos-release"
    elif platform.system() == "Windows":
        build_script += " clang-win-release"
    #else Linux is the default

    env = os.environ.copy()
    if library_ref is None:
        env.pop(LIBRARY_REF_ENV_VAR, None)
    else:
        env[LIBRARY_REF_ENV_VAR] = library_ref

    result = subprocess.run(args=build_script, shell=True, cwd=runtime_root_dir, capture_output=True, text=True, env=env)
    if result.returncode != 0:
        print(f"Build failed for {runtime}{display_ref}:")
        print(result.stdout)
        print(result.stderr)
        return False
    return True

def run_runtime_benchmarks(language, runtime, result_runtime_name, threads):
    for bench_name in benchmarks_order:
        # lowest_dur = sys.maxsize
        bench_args = benchmarks[bench_name]
        runtime_root_dir = os.path.join(root_dir, language, runtime)
        bench_exe = os.path.join(runtime_root_dir, "build", bench_name)

        # Get configs for this runtime+benchmark combo, or use a single empty config
        configs = benchmark_configs.get(runtime, {}).get(bench_name, [""])

        # Skip if benchmark executable doesn't exist
        if not os.path.exists(bench_exe):
            continue

        for config in configs:
             for params in bench_args.setdefault("params",[""]):
                 for thread_count in threads:
                     one_run = {
                         "params": params,
                         "threads": thread_count,
                         "config": config,
                     }
                     # Build command: exe params threads [config]
                     cmd = f"{bench_exe} {params} {thread_count}"
                     if config:
                         cmd += f" {config}"

                     print(f"Running {cmd}")
                     output_array = subprocess.run(args=cmd, shell=True, capture_output=True, text=True)
                     try:
                         print(output_array.stdout)
                         raw = yaml.safe_load(output_array.stdout)
                         run_data = raw["runs"][0]

                         result = {
                             "duration": run_data["duration"]
                         }
                         # Extract max_rss
                         if "max_rss" in run_data:
                             result["max_rss"] = format_mem(run_data["max_rss"])

                         # Extract throughput (any field ending in /sec)
                         for key, value in run_data.items():
                             if key.endswith("/sec"):
                                 result["throughput"] = value
                                 break
                         one_run["result"] = result
                         # Use config-suffixed runtime name if config is specified
                         result_runtime = result_runtime_name if not config else f"{result_runtime_name}_{config}"
                         full_results.setdefault(result_runtime, {}).setdefault(bench_name, []).append(one_run)
                     except (yaml.YAMLError, Exception) as exc:
                         print(f"Skipping result: {exc}")
                         continue

args = parse_args()
compare_mode = args["compare_runtime"] is not None
single_runtime_mode = args["single_runtime"] is not None
active_runtimes = runtimes
if compare_mode:
    active_runtimes = {get_language_for_runtime(args["compare_runtime"]): [args["compare_runtime"]]}
elif single_runtime_mode:
    active_runtimes = {get_language_for_runtime(args["single_runtime"]): [args["single_runtime"]]}

# Build all runtimes, all benchmarks
if compare_mode:
    compare_runtime = args["compare_runtime"]
    language = get_language_for_runtime(compare_runtime)
    threads = get_threads_sweep(args["full_sweep"])
    print(f"Threads sweep: {threads}")

    compare_runs = [
        (f"{compare_runtime}_{args['compare_new_ref']}", args["compare_new_ref"]),
    ]
    if args["compare_baseline_ref"] is None:
        compare_runs.append((f"{compare_runtime}_baseline", None))
    else:
        compare_runs.append((f"{compare_runtime}_{args['compare_baseline_ref']}", args["compare_baseline_ref"]))

    for result_runtime_name, library_ref in compare_runs:
        if build_runtime(language, compare_runtime, library_ref=library_ref, clean_build=True):
            run_runtime_benchmarks(language, compare_runtime, result_runtime_name, threads)
elif single_runtime_mode:
    single_runtime = args["single_runtime"]
    single_ref = args["single_ref"]
    language = get_language_for_runtime(single_runtime)
    threads = get_threads_sweep(args["full_sweep"])
    print(f"Threads sweep: {threads}")

    if build_runtime(language, single_runtime, library_ref=single_ref, clean_build=single_ref is not None):
        run_runtime_benchmarks(language, single_runtime, single_runtime, threads)
else:
    for language, runtime_names in active_runtimes.items():
        for runtime in runtime_names:
            build_runtime(language, runtime, library_ref=os.environ.get(LIBRARY_REF_ENV_VAR))

    # Run sweep runtime -> benchmark -> threads
    threads = get_threads_sweep(args["full_sweep"])
    print(f"Threads sweep: {threads}")
    for language, runtime_names in active_runtimes.items():
        for runtime in runtime_names:
            run_runtime_benchmarks(language, runtime, runtime, threads)


for bench_name in benchmarks_order:
    lowest_dur = sys.maxsize
    for runtime, runtime_results in full_results.items():
        if bench_name not in runtime_results:
            continue
        for run in runtime_results[bench_name]:
            dur = get_dur_in_us(run["result"]["duration"])
            if (dur < lowest_dur):
                lowest_dur = dur
    if lowest_dur == sys.maxsize:
        continue
    for runtime, runtime_results in full_results.items():
        if bench_name not in runtime_results:
            continue
        firstDur = None
        for i, run in enumerate(runtime_results[bench_name]):
            dur = get_dur_in_us(run["result"]["duration"])
            scaled = float(dur) / float(lowest_dur)
            scaled = round(scaled, 2)
            run["result"]["scaled"] = scaled
            if i == 0:
                firstDur = dur
            speedup = float(firstDur) / float(dur)
            speedup = round(speedup, 2)
            run["result"]["speedup"] = speedup

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
            which_run = 0
            params = collect_item["params"]
            bench_results = runtime_results[bench_name]
            last_result = bench_results[len(bench_results) - 1]
            dur_string = last_result["result"]["duration"]
            dur_in_us = get_dur_in_us(dur_string)
            friendly_name = bench_name
            if params:
                friendly_name += f"({params})"
            collated_results.setdefault(runtime, {})[friendly_name] = {"raw": dur_string, "us": dur_in_us}
            if not friendly_name in bench_names:
                bench_names.append(friendly_name)

# Get system information and attach it as metadata to the JSON file only
if args["full_sweep"] or compare_mode or single_runtime_mode:
    print("Generating RESULTS.json...")
    try:
        # Linux
        model_name_raw = subprocess.run(args=f"lscpu | grep \"Model name:\"", shell=True, capture_output=True, text=True)
        model_name = model_name_raw.stdout.split(":")[1].strip()
        md["cpu"] = model_name
    except:
        try:
            # MacOS
            md["cpu"] = subprocess.run(args=f"sysctl -n machdep.cpu.brand_string", shell=True, capture_output=True, text=True).stdout
        except:
            md["cpu"] = "unknown"
    try:
        # Linux
        model_name_raw = subprocess.run(args=f"lscpu | grep \"per socket:\"", shell=True, capture_output=True, text=True)
        model_name = model_name_raw.stdout.split(":")[1].strip()
        md["cores"] = model_name
    except:
        try:
            # MacOS
            md["cores"] = subprocess.run(args=f"sysctl -n machdep.cpu.core_count", shell=True, capture_output=True, text=True).stdout
        except:
            md["cores"] = "unknown"
    try:
        kernel_raw = subprocess.run(args=f"uname -v", shell=True, capture_output=True, text=True)
        kernel = kernel_raw.stdout.strip()
        md["kernel"] = kernel
    except:
        md["kernel"] = "unknown"
    try:
        for language, runtime_names in active_runtimes.items():
            for runtime in runtime_names:
                # find the compiler exe from compile_commands.json and call it to get the version
                if "compiler" in md:
                    continue
                runtime_root_dir = os.path.join(root_dir, language, runtime)
                ccj = os.path.join(runtime_root_dir, "build", "compile_commands.json")
                compiler_bin = ""
                with open(ccj, "r") as ccf:
                    cc = json.load(ccf)
                    compiler_bin = cc[0]["command"].split(" ")[0]
                compiler_info = subprocess.run(args=f"{compiler_bin} --version", shell=True, capture_output=True, text=True)
                compiler_line = compiler_info.stdout.splitlines()[0].strip()
                md["compiler"] = compiler_line
    except:
        md["compiler"] = "unknown"

    tagged = {
        "metadata": md,
        "results": full_results,
    }
    outJson = json.dumps(tagged)
    with open("RESULTS.json", "w") as resultsJSON:
        resultsJSON.write(outJson)

    # Generate RESULTS.html from the template for local viewing
    print("Generating RESULTS.html...")
    with open("results.html.tmpl", "r") as tmpl_file:
        html_content = tmpl_file.read()
    html_content = html_content.replace("{{ Script Will Substitute Latest Run Data Here }}", outJson)
    with open("RESULTS.html", "w") as html_file:
        html_file.write(html_content)
    print("View benchmark charts in your browser at: file:///"+os.path.abspath("RESULTS.html").replace("\\", "/"))


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

print("Generating RESULTS.md...", end=" ")
outMD = ""

# Process each group separately
for runtime_set, group_bench_names in runtime_set_to_benchmarks.items():
    group_runtimes = list(runtime_set)

    # Calculate lowest results for this group's benchmarks
    lowest_results = {}
    for runtime in group_runtimes:
        runtime_results = collated_results[runtime]
        for bench_name in group_bench_names:
            if bench_name in runtime_results:
                us = runtime_results[bench_name]["us"]
                curr_lowest = lowest_results.get(bench_name, sys.maxsize)
                if us < curr_lowest:
                    lowest_results[bench_name] = us

    # Calculate ratios for this group
    for runtime in group_runtimes:
        runtime_results = collated_results[runtime]
        for bench_name in group_bench_names:
            if bench_name in runtime_results:
                us = runtime_results[bench_name]["us"]
                ratio = float(us) / float(lowest_results[bench_name])
                runtime_results[bench_name]["ratio"] = ratio

    # Sort runtimes by mean ratio within this group
    sorted_runtimes = []
    for runtime in group_runtimes:
        runtime_results = collated_results[runtime]
        count = len(group_bench_names)
        total = sum(runtime_results[b]["ratio"] for b in group_bench_names if b in runtime_results)
        mean = total / count
        sorted_runtimes.append({"runtime": runtime, "mean": mean})
    sorted_runtimes.sort(key=lambda x: x["mean"])

    # Build output array for this group
    output_array = [["Runtime", "Mean Ratio to Best<br>(lower is better)"] + group_bench_names]
    for runtime in sorted_runtimes:
        runtime_name = runtime["runtime"]
        runtime_mean = runtime["mean"]
        base_runtime = runtime_name.split("_")[0]
        runtime_output = [
            f"[{runtime_name}]({runtime_links.get(base_runtime, '')})",
            "{:.2f}x".format(runtime_mean)
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

with open("RESULTS.md", "w") as resultsMD:
    resultsMD.write(outMD.strip() + "\n")

print("done.")
