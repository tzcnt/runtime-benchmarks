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

runtimes = {
    "cpp": ["libfork", "TooManyCooks", "tbb", "taskflow", "cppcoro", "concurrencpp"]
}

runtime_links = {
    "libfork": "https://github.com/ConorWilliams/libfork",
    "TooManyCooks": "https://github.com/tzcnt/TooManyCooks",
    "tbb": "https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html",
    "taskflow": "https://github.com/taskflow/taskflow",
    "cppcoro": "https://github.com/andreasbuhr/cppcoro",
    "coros": "https://github.com/mtmucha/coros",
    "concurrencpp": "https://github.com/David-Haim/concurrencpp",
    "HPX": "https://github.com/STEllAR-GROUP/hpx",
    "libcoro": "https://github.com/jbaldwin/libcoro"
}

benchmarks_order = ["skynet", "nqueens", "fib", "matmul"]

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
}

collect_results = {
    "fib": [{"params": "39"}],
    "skynet": [{"params": ""}],
    "nqueens": [{"params": ""}],
    "matmul": [{"params": "2048"}]
}

# Fallback to a shell script for hardware core count detection if the user didn't build TMC
def get_nproc_fallback():
    try:
        return int(subprocess.run(args=f"./get_nproc.sh", shell=True, capture_output=True, text=True).stdout)
    except:
        return 8
    
def get_threads_sweep_fallback():
    proc = get_nproc_fallback()
    if len(sys.argv) == 1:
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
def get_threads_sweep():
    try:
        s = subprocess.run(args=f"./cpp/TooManyCooks/build/threads_sweep", shell=True, capture_output=True, text=True).stdout
        sweep = ast.literal_eval(s)
        if len(sys.argv) == 1:
            return [sweep[-1]] # Only test at max cores if user didn't pass "full" arg
        return sweep
    except:
        return get_threads_sweep_fallback()

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

root_dir = os.path.abspath(os.path.dirname(__file__))

md = {"start_time": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")}
full_results = {}

# Build all runtimes, all benchmarks
for language, runtime_names in runtimes.items():
    for runtime in runtime_names:
        runtime_root_dir = os.path.join(root_dir, language, runtime)
        print(f"Building {runtime}")
        build_script = os.path.join(runtime_root_dir, "build_all.sh")
        subprocess.run(args=build_script, shell=True, cwd=runtime_root_dir)

# Run sweep runtime -> benchmark -> threads
threads = get_threads_sweep()
print(f"Threads sweep: {threads}")
for language, runtime_names in runtimes.items():
    for runtime in runtime_names:
        for bench_name in benchmarks_order:
            # lowest_dur = sys.maxsize
            bench_args = benchmarks[bench_name]
            runtime_root_dir = os.path.join(root_dir, language, runtime)
            bench_exe = os.path.join(runtime_root_dir, "build", bench_name)
            for params in bench_args.setdefault("params",[""]):
                for thread_count in threads:
                    one_run = {
                        "params": params,
                        "threads": thread_count,
                    }
                    print(f"Running {bench_exe} {params} {thread_count}")
                    output_array = subprocess.run(args=f"{bench_exe} {params} {thread_count}", shell=True, capture_output=True, text=True)
                    try:
                        print(output_array.stdout)
                        raw = yaml.safe_load(output_array.stdout)
                        result = {
                            "duration": raw["runs"][0]["duration"]
                        }
                        one_run["result"] = result
                        full_results.setdefault(runtime, {}).setdefault(bench_name, []).append(one_run)
                    except yaml.YAMLError as exc:
                        print(exc)


for bench_name in benchmarks_order:
    lowest_dur = sys.maxsize
    for language, runtime_names in runtimes.items():
        for runtime in runtime_names:
            bench_args = benchmarks[bench_name]
            for params in bench_args["params"]:
                for i, thread_count in enumerate(threads):
                    dur = get_dur_in_us(full_results[runtime][bench_name][i]["result"]["duration"])
                    if (dur < lowest_dur):
                        lowest_dur = dur
    for language, runtime_names in runtimes.items():
        for runtime in runtime_names:
            bench_args = benchmarks[bench_name]
            for params in bench_args["params"]:
                for i, thread_count in enumerate(threads):
                    dur = get_dur_in_us(full_results[runtime][bench_name][i]["result"]["duration"])
                    scaled = float(dur) / float(lowest_dur)
                    scaled = round(scaled, 2)
                    full_results[runtime][bench_name][i]["result"]["scaled"] = scaled
                    if i == 0:
                        firstDur = dur
                    speedup = float(firstDur) / float(dur)
                    speedup = round(speedup, 2)
                    full_results[runtime][bench_name][i]["result"]["speedup"] = speedup
    
# full_results is used to produce the .json for charting
# collated_results is used to produce the .md summary for the README
collated_results = {}
bench_names = []
for runtime, runtime_results in full_results.items():
    for bench_name in benchmarks_order:
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
if len(sys.argv) != 1:
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
        for language, runtime_names in runtimes.items():
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


# For each benchmark, find the fastest runtime and calculate the runtime ratio of the other runtimes against that
lowest_results = {}
for runtime, runtime_results in collated_results.items():
    for bench_name, result in runtime_results.items():
        curr_lowest = lowest_results.setdefault(bench_name, int(sys.maxsize))
        us = result["us"]
        if us < curr_lowest:
            lowest_results[bench_name] = us

for runtime, runtime_results in collated_results.items():
    for bench_name, result in runtime_results.items():
        us = result["us"]
        ratio = float(us) / float(lowest_results[bench_name])
        runtime_results[bench_name]["ratio"] = ratio
        # if us == lowest_results[bench_name]:
        #     print(f"{bench_name}: {runtime} is the fastest with {us} us")

sorted = []
for runtime, runtime_results in collated_results.items():
    count = len(runtime_results)
    sum = float(0)
    for bench in runtime_results.values():
        sum += bench["ratio"]
    mean = sum / count
    sorted.append({"runtime": runtime, "mean": mean})

sorted.sort(key=lambda x: x["mean"])
output_array = [["Runtime", "Mean Ratio to Best<br>(lower is better)"] + bench_names]
for runtime in sorted:
    runtime_name = runtime["runtime"]
    runtime_mean = runtime["mean"]
    runtime_output = [
        f"[{runtime_name}]({runtime_links.get(runtime_name, '')})",
        "{:.2f}x".format(runtime_mean)
    ]
    runtime_results = collated_results[runtime_name]
    for bench in bench_names:
        runtime_output.append(runtime_results.setdefault(bench, {"raw": "N/A"})["raw"])
    output_array.append(runtime_output)

print("Generating RESULTS.md and RESULTS.csv ...", end=" ")

len_y = len(output_array[0])
len_x = len(output_array)

outMD = ""
outCSV = ""

# Create header row
for x in range(len_x):
    outMD +=f"| {output_array[x][0]} "
    outCSV +=f"{output_array[x][0]},"
outMD += "|\n"
outCSV = outCSV.rstrip(",") + "\n"
# Create markdown table header row
for x in range(len_x):
    outMD += "| --- "
outMD += "|\n"
# Create data rows
for y in range(1,len_y):
    for x in range(len_x):
        outMD += f"| {output_array[x][y]} "
        outCSV += f"{output_array[x][y]},"
    outMD += "|\n"
    outCSV = outCSV.rstrip(",") + "\n"

with open("RESULTS.md", "w") as resultsMD:
    resultsMD.write(outMD)

with open("RESULTS.csv", "w") as resultsCSV:
    resultsCSV.write(outCSV)

print("done.")
