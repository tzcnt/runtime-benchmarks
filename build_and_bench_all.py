# build and execute every benchmark in every runtime
# collate the results and sort them according to Mean Ratio to Best
# update the README with the table of results

# this script is a total hack job... but it works.

import os
import subprocess
import yaml
import sys

benchmarks={
    "fib": {
        "params": ["30", "35", "40", "45"]
    },
    "skynet": {

    }
}
runtimes = {
    "cpp": ["TooManyCooks", "concurrencpp"]
}

results = {}

root_dir = os.path.abspath(os.path.dirname(__file__))

for language, runtime_names in runtimes.items():
    for runtime in runtime_names:
        # Build the benchmark
        runtime_root_dir = os.path.join(root_dir, language, runtime)
        print(f"Building {runtime}")
        build_script = os.path.join(runtime_root_dir, "build_all.sh")
        subprocess.run(args=build_script, shell=True, cwd=runtime_root_dir)
        for bench_name, bench_args in benchmarks.items():
            bench_exe = os.path.join(runtime_root_dir, "build", bench_name)
            for params in bench_args.setdefault("params",[""]):
                print(f"Running {bench_exe}")
                output_array = subprocess.run(args=f"{bench_exe} {params}", shell=True, capture_output=True, text=True)
                try:
                    print(output_array.stdout)
                    result = yaml.safe_load(output_array.stdout)
                    results.setdefault(runtime, {}).setdefault(bench_name, {})[params] = result
                except yaml.YAMLError as exc:
                    print(exc)

#print(results)

def get_dur_in_ns(dur_string):
    dur, unit = dur_string.split(" ")
    dur = int(dur)
    # convert all units to ns for comparison
    match unit:
        case "us":
            return dur * 1000
        case "ms":
            return dur * 1000000
        case "s":
            return dur * 1000000000
        case "sec":
            return dur * 1000000000
        case _:
            print(f"Unknown unit: {unit}")
            exit(1)

collect_results = {
    "fib": [{"params": "30", "runs": ["first"]},
            {"params": "35", "runs": ["first"]},
            {"params": "40", "runs": ["first"]},
            {"params": "45", "runs": ["first"]}
            ],
    "skynet": [{"params": "", "runs": ["first", "last"]}]
}

collated_results = {}
for runtime, runtime_results in results.items():
    for bench_name, collect in collect_results.items():
        for collect_item in collect:
            runs = collect_item["runs"]
            for run in runs:
                if run == "first":
                    runval = 0
                elif run == "last":
                    runval = len(runtime_results[bench_name][params]["runs"]) - 1
                else:
                    runval = run
                params = collect_item["params"]
                dur_string = runtime_results[bench_name][params]["runs"][runval]["duration"]
                dur_in_ns = get_dur_in_ns(dur_string)
                friendly_name = bench_name
                if params:
                    friendly_name += f"({params})"
                if len(runs) > 1:
                    friendly_name += f" ({run} run)"
                collated_results.setdefault(runtime, {})[friendly_name] = {"raw": dur_string, "ns": dur_in_ns}

lowest_results = {}
for runtime, runtime_results in collated_results.items():
    for bench_name, result in runtime_results.items():
        curr_lowest = lowest_results.setdefault(bench_name, int(sys.maxsize))
        ns = result["ns"]
        if ns < curr_lowest:
            lowest_results[bench_name] = ns

for runtime, runtime_results in collated_results.items():
    for bench_name, result in runtime_results.items():
        ns = result["ns"]
        ratio = float(ns) / float(lowest_results[bench_name])
        runtime_results[bench_name]["ratio"] = ratio
        # if ns == lowest_results[bench_name]:
        #     print(f"{bench_name}: {runtime} is the fastest with {ns} ns")

sorted = []
for runtime, runtime_results in collated_results.items():
    count = len(runtime_results)
    sum = float(0)
    for bench in runtime_results.values():
        sum += bench["ratio"]
    mean = sum / count
    sorted.append({"runtime": runtime, "mean": mean})

bench_names = ["fib(30)", "fib(35)", "fib(40)", "fib(45)", "skynet (first run)", "skynet (last run)"]
runtime_links = {
    "TooManyCooks": "https://github.com/tzcnt/TooManyCooks",
    "concurrencpp": "https://github.com/David-Haim/concurrencpp"
}

sorted.sort(key=lambda x: x["runtime"])
# print(sorted)
output_array = [["Runtime", "Mean Ratio to Best"] + bench_names]
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

print("Updating README.md embedded table...")

len_y = len(output_array[0])
len_x = len(output_array)

output = ""

# Create header row
for x in range(len_x):
    output +=f"| {output_array[x][0]} "
output += "|\n"
# Create markdown table header row
for x in range(len_x):
    output += "| --- "
output += "|\n"
# Create data rows
for y in range(1,len_y):
    for x in range(len_x):
        output += f"| {output_array[x][y]} "
    output += "|\n"

with open("README_template.md", "r") as readme_template:
  fulltext = readme_template.read()

fulltext = fulltext.replace("<<bench_results>>\n", output)

with open("README.md", "w") as readme:
  readme.write(fulltext)
