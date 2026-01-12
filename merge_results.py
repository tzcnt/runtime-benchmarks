# Merge results from one dataset JSON into another.
# Typically I prefer to run all benchmarks for a machine at once, but for some very slow
# benchmarks, I don't want to wait hours for them all to run. So I may run one by itself and update it
# into the main dataset (assuming none of the other parameters have changed).

import json
import sys

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

def merge_results(dest_file, source_file):
    with open(source_file, 'r') as f:
        source = json.load(f)
    
    with open(dest_file, 'r') as f:
        dest = json.load(f)
    
    for key, value in source.get('results', {}).items():
        dest['results'][key] = value
    
    # Recalculate scaled and speedup values
    for bench_name in dest['results'].get(list(dest['results'].keys())[0], {}).keys():
        lowest_dur = sys.maxsize
        
        # Find the lowest duration for this benchmark across all runtimes
        for runtime, runtime_results in dest['results'].items():
            if bench_name in runtime_results:
                for run in runtime_results[bench_name]:
                    dur = get_dur_in_us(run['result']['duration'])
                    if dur < lowest_dur:
                        lowest_dur = dur
        
        # Update scaled and speedup for all runs of this benchmark
        for runtime, runtime_results in dest['results'].items():
            if bench_name in runtime_results:
                first_dur = None
                for i, run in enumerate(runtime_results[bench_name]):
                    dur = get_dur_in_us(run['result']['duration'])
                    scaled = round(float(dur) / float(lowest_dur), 2)
                    run['result']['scaled'] = scaled
                    
                    if i == 0:
                        first_dur = dur
                    speedup = round(float(first_dur) / float(dur), 2)
                    run['result']['speedup'] = speedup
    
    with open(dest_file, 'w') as f:
        json.dump(dest, f, indent=2)
        f.write('\n')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <dest_file> <source_file>")
        sys.exit(1)
    
    merge_results(sys.argv[1], sys.argv[2])
