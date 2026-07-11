# Merge results from one dataset JSON into another.
# Typically I prefer to run all benchmarks for a machine at once, but for some very slow
# benchmarks, I don't want to wait hours for them all to run. So I may run one by itself and update it
# into the main dataset (assuming none of the other parameters have changed).

import json
import sys

def get_dur_in_us(dur_string):
    if dur_string == "DNF":
        return None
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
    
    # Recalculate scaled and speedup values across every benchmark present in
    # any runtime (not just the first). Merged files often have different
    # benchmark sets per runtime, so keying off the first runtime alone would
    # leave the others' benchmarks with stale, pre-merge scaling.
    all_bench_names = []
    for runtime_results in dest['results'].values():
        for bench_name in runtime_results:
            # "metadata" is the runtime's per-section compiler/timestamp block,
            # preserved as-is (copied above), not a benchmark to renormalize.
            if bench_name == "metadata":
                continue
            if bench_name not in all_bench_names:
                all_bench_names.append(bench_name)
    for bench_name in all_bench_names:
        lowest_dur = sys.maxsize
        
        # Find the lowest duration for this benchmark across all runtimes
        # (DNF runs have no duration and are skipped).
        for runtime, runtime_results in dest['results'].items():
            if bench_name in runtime_results:
                for run in runtime_results[bench_name]:
                    if run['result'].get('dnf'):
                        continue
                    dur = get_dur_in_us(run['result']['duration'])
                    if dur < lowest_dur:
                        lowest_dur = dur
        if lowest_dur == sys.maxsize:
            continue

        # Update scaled and speedup for all runs of this benchmark
        for runtime, runtime_results in dest['results'].items():
            if bench_name in runtime_results:
                first_dur = None
                for run in runtime_results[bench_name]:
                    if run['result'].get('dnf'):
                        run['result']['scaled'] = None
                        run['result']['speedup'] = None
                        continue
                    dur = get_dur_in_us(run['result']['duration'])
                    run['result']['scaled'] = round(float(dur) / float(lowest_dur), 2)
                    if first_dur is None:
                        first_dur = dur
                    run['result']['speedup'] = round(float(first_dur) / float(dur), 2)
    
    with open(dest_file, 'w') as f:
        json.dump(dest, f, indent=2)
        f.write('\n')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <dest_file> <source_file>")
        sys.exit(1)
    
    merge_results(sys.argv[1], sys.argv[2])
