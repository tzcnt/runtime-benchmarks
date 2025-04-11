# build and execute every benchmark in every runtime
# collate the results and sort them according to Mean Ratio to Best
# update the README with the table of results

# this script is a total hack job... but it works.

import datetime
import json
import os
import subprocess
import yaml
import sys

runtimes = {
    "cpp": ["TooManyCooks", "libfork", "tbb", "coros", "concurrencpp", "taskflow"]
}

runtime_links = {
    "TooManyCooks": "https://github.com/tzcnt/TooManyCooks",
    "libfork": "https://github.com/ConorWilliams/libfork",
    "tbb": "https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html",
    "coros": "https://github.com/mtmucha/coros",
    "taskflow": "https://github.com/taskflow/taskflow",
    "concurrencpp": "https://github.com/David-Haim/concurrencpp",
}

benchmarks_order = ["skynet", "nqueens", "fib", "matmul"]

benchmarks={
    "skynet": {

    },
    "fib": {
        "params": ["40"]
    },
    "nqueens": {

    },
    "matmul": {
        "params": ["2048"]
    },
}

collect_results = {
    "fib": [{"params": "40"}],
    "skynet": [{"params": ""}],
    "nqueens": [{"params": ""}],
    "matmul": [{"params": "2048"}]
}

root_dir = os.path.abspath(os.path.dirname(__file__))

md = {"start_time": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")}
results = {}
for language, runtime_names in runtimes.items():
    for runtime in runtime_names:
        # Build the benchmark
        runtime_root_dir = os.path.join(root_dir, language, runtime)
        print(f"Building {runtime}")
        build_script = os.path.join(runtime_root_dir, "build_all.sh")
        subprocess.run(args=build_script, shell=True, cwd=runtime_root_dir)
        for bench_name in benchmarks_order:
            bench_args = benchmarks[bench_name]
            bench_exe = os.path.join(runtime_root_dir, "build", bench_name)
            for params in bench_args.setdefault("params",[""]):
                print(f"Running {bench_exe} {params}")
                output_array = subprocess.run(args=f"{bench_exe} {params}", shell=True, capture_output=True, text=True)
                try:
                    print(output_array.stdout)
                    result = yaml.safe_load(output_array.stdout)
                    results.setdefault(runtime, {}).setdefault(bench_name, {})[params] = result
                except yaml.YAMLError as exc:
                    print(exc)

#print(results)

def get_dur_in_us(dur_string):
    dur, unit = dur_string.split(" ")
    dur = int(dur)
    # convert all units to microseconds for comparison
    match unit:
        case "us":
            return dur
        case "ms":
            return dur * 1000
        case "s":
            return dur * 1000000
        case "sec":
            return dur * 1000000
        case _:
            print(f"Unknown unit: {unit}")
            exit(1)

collated_results = {}
bench_names = []
for runtime, runtime_results in results.items():
    for bench_name in benchmarks_order:
        collect = collect_results[bench_name]
        for collect_item in collect:
            which_run = 0
            params = collect_item["params"]
            dur_string = runtime_results[bench_name][params]["runs"][which_run]["duration"]
            dur_in_us = get_dur_in_us(dur_string)
            friendly_name = bench_name
            if params:
                friendly_name += f"({params})"
            collated_results.setdefault(runtime, {})[friendly_name] = {"raw": dur_string, "us": dur_in_us}
            if not friendly_name in bench_names:
                bench_names.append(friendly_name)

# Get system information and attach it as metadata to the JSON file only
# TODO use 'sysctl -a | grep machdep.cpu' on Darwin
try:
    model_name_raw = subprocess.run(args=f"lscpu | grep \"Model name:\"", shell=True, capture_output=True, text=True)
    model_name = model_name_raw.stdout.split(":")[1].strip()
    md["cpu"] = model_name
except:
    md["cpu"] = "unknown"
try:
    model_name_raw = subprocess.run(args=f"lscpu | grep \"per socket:\"", shell=True, capture_output=True, text=True)
    model_name = model_name_raw.stdout.split(":")[1].strip()
    md["cores"] = model_name
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
            if "compiler" in md:
                continue
            # Build the benchmark
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
    "results": collated_results,
}
outJson = json.dumps(tagged)
with open("RESULTS.json", "w") as resultsJSON:
    resultsJSON.write(outJson)


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
# print(sorted)
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

# print(output_array)

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
