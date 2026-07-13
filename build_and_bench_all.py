#!/usr/bin/env python3

# build and execute every benchmark in every runtime
# collate the results and sort them according to Mean Ratio to Best
# update the README with the table of results

import datetime
import json
import os
import signal
import subprocess
import yaml
import sys
import ast
import platform
import shutil

import merge_results  # local module: reused to combine per-runtime result files

runtimes = {
    "cpp": ["citor", "libfork", "TooManyCooks", "tbb", "taskflow", "cppcoro", "coros", "cobalt",
            "PhotonLibOS",
            # these 5 are quite slow - you can remove them to speed up total runtime
            "folly", "concurrencpp", "HPX", "libcoro", "userver"],
    "rust": ["tokio", "smol", "rayon", "chili", "forte", "bevy_tasks", "micropool", "beekeeper"],
    "go": ["go"],
    "cs": ["dotnet"],
    "java": ["java", "forkjoin"],
    "kotlin": ["kotlin_fjp", "kotlin_default"],
    "nim": ["weave"],
    "c": ["neco"],
    "zig": ["zap", "zigbeam", "spice"]
}

# The result/display key for a runtime may differ from its subfolder name so that
# two languages can both ship a "std" implementation without their result keys
# colliding (e.g. go/std and cs/std). runtime_folders maps such keys to their
# subfolder; a key not listed here uses its own name as the folder.
runtime_folders = {
    "go": "std",     # go/std (the Go standard library)
    "dotnet": "std",  # cs/std (the C# / .NET base class library)
    "java": "std",    # java/std (the Java standard library, via Virtual Threads)
    # Kotlin's coroutines (kotlinx.coroutines) on two different schedulers:
    "kotlin_fjp": "fjp",          # kotlin/fjp (a JDK ForkJoinPool, LIFO-local)
    "kotlin_default": "default",  # kotlin/default (Dispatchers.Default)
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
    "libcoro": "https://github.com/jbaldwin/libcoro",
    "userver": "https://github.com/userver-framework/userver",
    "PhotonLibOS": "https://github.com/alibaba/PhotonLibOS",
    "tokio": "https://github.com/tokio-rs/tokio",
    # smol: a facade over the smol-rs runtime crates (async-executor, async-io,
    # async-net, async-channel). Runs all 6 benchmarks: nested fork-join on a
    # fixed pool of Executor worker threads, channel via its native MPMC
    # smol::channel, and io_socket_st via smol::net on per-thread LocalExecutors.
    "smol": "https://github.com/smol-rs/smol",
    "rayon": "https://github.com/rayon-rs/rayon",
    # chili: the Rust port of Spice (heartbeat-scheduled fork-join). Like rayon,
    # synchronous-only, so just the four fork-join benchmarks.
    "chili": "https://github.com/dragostis/chili",
    # forte: heartbeat-scheduled fork-join scheduler with a rayon-style join/scope
    # (a sibling of chili/spice). Fork-join-only, so just the four fork-join
    # benchmarks. Pinned to 1.0.0-alpha.4 (the newest release that builds on Rust
    # 1.90; alpha.5 needs 1.96); see rust/forte/Cargo.toml.
    "forte": "https://github.com/NthTensor/Forte",
    # bevy_tasks: Bevy's async task pool (built on async-executor). Tasks can
    # spawn and await further tasks on the same pool, so it runs the four
    # fork-join benchmarks (no async socket or MPMC channel API). Keyed as
    # "bevy" because the README generator resolves links via the runtime name's
    # prefix before the first underscore (bevy_tasks -> bevy).
    "bevy": "https://github.com/bevyengine/bevy/tree/main/crates/bevy_tasks",
    # micropool: a fixed-size, low-latency work-stealing pool (rayon-style, aimed
    # at games) with a free `join` and external-thread participation. All workers
    # are created at pool build time and reused - nothing is spawned while a
    # benchmark runs - so it runs the four fork-join benchmarks (no async socket
    # or MPMC channel API).
    "micropool": "https://github.com/DouglasDwyer/micropool",
    # beekeeper: a worker-pool ("hive") library. Workers can fork subtasks from
    # inside a task (Context::submit) but cannot join them, so the fork-join
    # benchmarks propagate results through the hive's outcome channel (and
    # matmul's group barrier via continuation nodes); see rust/beekeeper. Its
    # task->outcome pipeline is an MPMC queue, so channel runs too (no async
    # socket API, so no io_socket_st).
    "beekeeper": "https://github.com/jdidion/beekeeper",
    "go": "https://pkg.go.dev/std",
    "dotnet": "https://learn.microsoft.com/en-us/dotnet/api/",
    "java": "https://openjdk.org/jeps/444",
    # java/forkjoin: raw JDK ForkJoinPool (RecursiveTask/RecursiveAction). Keyed
    # without an underscore so it resolves to its own link rather than sharing the
    # virtual-thread "java" entry above (folder defaults to "forkjoin").
    "forkjoin": "https://docs.oracle.com/en/java/javase/25/docs/api/java.base/java/util/concurrent/ForkJoinPool.html",
    "kotlin": "https://github.com/Kotlin/kotlinx.coroutines",
    "weave": "https://github.com/mratsim/weave",
    "neco": "https://github.com/tidwall/neco",
    # zig/zap: a vendored port of the "blog" branch's thread pool (upstream
    # targets a pre-0.10 Zig); see zig/zap/src/thread_pool.zig.
    "zap": "https://github.com/kprotty/zap",
    "zigbeam": "https://github.com/eakova/zigbeam",
    # spice: heartbeat-scheduled fork-join library. Fork-join-only, so just the
    # four fork-join benchmarks (no channel / io).
    "spice": "https://github.com/judofyr/spice"
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
    "tokio": {
        # tokio has no native async MPMC queue; its channel benchmark is backed by
        # flume. The label is passed for result naming (tokio_flume) and ignored by
        # the binary.
        "channel": ["flume"]
    },
    "weave": {
        # weave has no public MPMC channel; its channel benchmark is backed by
        # Nim's threading/channels package. The label is passed for result naming
        # (weave_threading) and ignored by the binary.
        "channel": ["threading"]
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


def format_dnf_label(dur_us):
    # Render a DNF's recorded ceiling duration as a short label for the
    # RESULTS.md table, e.g. 600000000 us -> "DNF (10m)". Derived from the
    # recorded duration (not BENCHMARK_TIMEOUT_SECONDS) so results produced
    # under a RUNTIME_BENCHMARKS_TIMEOUT_SECONDS override label correctly.
    secs = round(dur_us / 1_000_000)
    if secs >= 60 and secs % 60 == 0:
        return f"DNF ({secs // 60}m)"
    return f"DNF ({secs}s)"


def format_mem(kib_string):
    # kib_string looks like "1234 KiB"
    ki = int(kib_string.split(" ")[0])
    mi = ki / 1024
    if mi >= 1024:
        return f"{round(mi / 1024, 2)} GB"
    return f"{round(mi, 2)} MB"

root_dir = os.path.abspath(os.path.dirname(__file__))

def runtime_dir(language, runtime):
    return os.path.join(root_dir, language, runtime_folders.get(runtime, runtime))

md = {"start_time": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")}
full_results = {}

def get_language_for_runtime(runtime):
    for language, runtime_names in runtimes.items():
        if runtime in runtime_names:
            return language
    return None

def build_runtime(language, runtime, library_ref=None, clean_build=False):
    runtime_root_dir = runtime_dir(language, runtime)
    display_ref = f" ({library_ref})" if library_ref else ""
    print(f"Building {runtime}{display_ref}", end="", flush=True)

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
        # Capture the failing build's output to a log in the repo root. The ref is
        # folded into the filename (sanitized) so compare mode's two builds of the
        # same runtime don't clobber each other's logs.
        safe_ref = "".join(c if c.isalnum() else "-" for c in library_ref) if library_ref else ""
        log_name = f"build-failed-{runtime}{'-' + safe_ref if safe_ref else ''}.log"
        log_path = os.path.join(root_dir, log_name)
        with open(log_path, "w") as log_file:
            log_file.write(f"Build failed for {runtime}{display_ref} (exit code {result.returncode})\n")
            log_file.write(f"Command: {build_script}\n")
            log_file.write(f"Working directory: {runtime_root_dir}\n\n")
            log_file.write("=== stdout ===\n")
            log_file.write(result.stdout)
            log_file.write("\n=== stderr ===\n")
            log_file.write(result.stderr)
        print(f" - failed (see {log_name})")
        return False
    print(" - success")
    return True

# Per-benchmark wall-clock ceiling (10 min). A run that exceeds it is killed and
# recorded as a DNF. The same ceiling is also the duration recorded for *every*
# DNF (see DNF_DURATION below), so a timeout, an OOM/crash, and unparseable output
# all count equivalently as one worst-case result instead of being excluded from
# the performance comparison. A run that legitimately needs more than 10 min is
# intentionally treated as a performance failure too. This also catches
# pathological hangs like java's `skynet 1`, which pins ~10^8 virtual threads onto
# a single carrier and GC-thrashes effectively forever while ignoring SIGTERM.
# Override via the RUNTIME_BENCHMARKS_TIMEOUT_SECONDS environment variable.
BENCHMARK_TIMEOUT_SECONDS = int(os.environ.get("RUNTIME_BENCHMARKS_TIMEOUT_SECONDS", "600"))

# Duration recorded for any DNF, equal to the timeout ceiling (in microseconds).
# Recording the ceiling rather than excluding the run makes timeout- and
# OOM/crash-induced failures a single, comparable worst-case number: the
# RESULTS.md table renders the run as an explicit "DNF (10m)" label while its
# ratio-to-best is still computed from this ceiling, so a consistently DNFing
# runtime ranks as a worst-case performer instead of dropping out of the
# comparison. The charts (scaled/speedup in RESULTS.json) instead show a gap
# for DNF points; see compute_scaled_speedup.
DNF_DURATION = f"{BENCHMARK_TIMEOUT_SECONDS * 1_000_000} us"

# --- OOM containment -------------------------------------------------------
# A benchmark that exhausts system memory must not take this script down with
# it. Without containment it does exactly that: the kernel OOM killer fires
# inside the terminal session's cgroup, and systemd then tears down the whole
# scope - this script, the shell, and the tmux pane included (journal:
# "tmux-spawn-*.scope: Failed with result 'oom-kill'"). Each benchmark is
# therefore launched in its own transient systemd scope, capped just below
# currently-available memory, so the OOM kill lands inside the benchmark's
# scope only. The benchmark dies by SIGKILL, which run_benchmark_process
# already records as a DNF, and the sweep moves on to the next benchmark.
_scope_isolation_works = None

def _scope_isolation_props():
    # MemoryMax: 90% of MemAvailable (re-read per run), so the benchmark hits
    # its cgroup limit while the system still has headroom and the global OOM
    # killer never gets involved. MemorySwapMax=0 keeps a runaway benchmark
    # from thrashing swap for minutes before dying. OOMPolicy=kill makes
    # systemd SIGKILL the whole scope as soon as any process in it is
    # OOM-killed, so no orphaned children keep eating memory.
    props = "-p MemorySwapMax=0 -p OOMPolicy=kill"
    try:
        with open("/proc/meminfo") as f:
            for line in f:
                if line.startswith("MemAvailable:"):
                    avail_bytes = int(line.split()[1]) * 1024
                    props += f" -p MemoryMax={max(avail_bytes * 9 // 10, 1 << 30)}"
                    break
    except OSError:
        pass
    return props

def isolate_cmd(cmd):
    # Wrap a benchmark command in its own transient scope. Probed once with
    # the same property set as a real run, so an unsupported property or a
    # missing user manager falls back to running unwrapped instead of turning
    # every benchmark into a spurious DNF.
    global _scope_isolation_works
    if _scope_isolation_works is None:
        probe = f"systemd-run --user --scope --quiet --collect {_scope_isolation_props()} -- true"
        _scope_isolation_works = subprocess.run(
            args=probe, shell=True, capture_output=True).returncode == 0
        if not _scope_isolation_works:
            print("Warning: systemd-run scope isolation unavailable; "
                  "an OOM'd benchmark may take this script down with it")
    if not _scope_isolation_works:
        return cmd
    return f"systemd-run --user --scope --quiet --collect {_scope_isolation_props()} -- {cmd}"

def _kill_process_group(proc):
    # Kill the whole process group, not just the shell we spawned. A crashing
    # benchmark (e.g. the JVM) can leave a child alive that keeps the stdout/
    # stderr pipes open, which would otherwise re-hang communicate() even after
    # the timeout fired. Fall back to the direct child if the group is already gone.
    try:
        os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
    except (ProcessLookupError, PermissionError):
        try:
            proc.kill()
        except ProcessLookupError:
            pass

def run_benchmark_process(cmd, timeout=BENCHMARK_TIMEOUT_SECONDS):
    # Run a single benchmark invocation, returning (stdout, dnf_reason). On a
    # clean finish dnf_reason is None; otherwise it is a short description of why
    # the run did not finish (timeout, killing signal, or nonzero exit).
    # start_new_session puts the benchmark and any children in their own process
    # group so a timeout can reap the whole tree via _kill_process_group.
    proc = subprocess.Popen(
        isolate_cmd(cmd), shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True, start_new_session=True,
    )
    timed_out = False
    try:
        stdout, _ = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        _kill_process_group(proc)
        # The group is dead now, so the pipes reach EOF; drain them. The second
        # timeout is a backstop against a genuinely unkillable process.
        try:
            stdout, _ = proc.communicate(timeout=30)
        except subprocess.TimeoutExpired:
            stdout = ""
        timed_out = True

    rc = proc.returncode
    if timed_out:
        return stdout, f"timed out after {timeout}s"
    if rc is not None and rc < 0:
        return stdout, f"killed by signal {-rc}"
    if rc:
        return stdout, f"exited with code {rc}"
    return stdout, None

def run_runtime_benchmarks(language, runtime, result_runtime_name, threads):
    for bench_name in benchmarks_order:
        # lowest_dur = sys.maxsize
        bench_args = benchmarks[bench_name]
        runtime_root_dir = runtime_dir(language, runtime)
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
                     stdout, dnf_reason = run_benchmark_process(cmd)
                     print(stdout)

                     # Use config-suffixed runtime name if config is specified
                     result_runtime = result_runtime_name if not config else f"{result_runtime_name}_{config}"

                     if dnf_reason is not None:
                         # The benchmark timed out, crashed, or was killed. Record it
                         # with the timeout-ceiling duration (DNF_DURATION) so a hang
                         # and an OOM/crash count equivalently as the same worst-case
                         # performance result instead of being excluded from the sweep.
                         # dnf_reason is kept as inert provenance (no consumer reads it).
                         print(f"DNF: {cmd} ({dnf_reason})")
                         one_run["result"] = {"duration": DNF_DURATION, "dnf": True, "dnf_reason": dnf_reason}
                         full_results.setdefault(result_runtime, {}).setdefault(bench_name, []).append(one_run)
                         continue

                     try:
                         raw = yaml.safe_load(stdout)
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
                         full_results.setdefault(result_runtime, {}).setdefault(bench_name, []).append(one_run)
                     except (yaml.YAMLError, Exception) as exc:
                         # The executable exists (guaranteed by the os.path.exists guard
                         # above) and exited cleanly, but produced no parseable run
                         # result - e.g. a fork-join benchmark that aborted partway after
                         # printing a partial "runs:" block, or exited 0 with truncated
                         # output. Record it with the timeout-ceiling duration
                         # (DNF_DURATION), same as any other DNF, so it counts as a
                         # worst-case result instead of silently vanishing. A benchmark
                         # that failed to build is skipped earlier (missing executable),
                         # so it never reaches here.
                         reason = f"no parseable result ({exc})"
                         print(f"DNF: {cmd} ({reason})")
                         one_run["result"] = {"duration": DNF_DURATION, "dnf": True, "dnf_reason": reason}
                         full_results.setdefault(result_runtime, {}).setdefault(bench_name, []).append(one_run)
                         continue

def compute_scaled_speedup(results):
    # For every benchmark, add to each run: `scaled` (its duration relative to
    # the fastest run in `results`) and `speedup` (relative to the first thread
    # count that finished). Mutates the run dicts in place; DNF runs get null.
    # This is the exact normalization used for both the combined RESULTS.json and
    # a standalone single-runtime run, so a per-runtime slice passed through here
    # matches what benchmarking that runtime alone would produce.
    for bench_name in benchmarks_order:
        lowest_dur = sys.maxsize
        for runtime, runtime_results in results.items():
            if bench_name not in runtime_results:
                continue
            for run in runtime_results[bench_name]:
                if run["result"].get("dnf"):
                    continue
                dur = get_dur_in_us(run["result"]["duration"])
                if dur < lowest_dur:
                    lowest_dur = dur
        if lowest_dur == sys.maxsize:
            continue
        for runtime, runtime_results in results.items():
            if bench_name not in runtime_results:
                continue
            firstDur = None
            for run in runtime_results[bench_name]:
                if run["result"].get("dnf"):
                    # No duration to scale; leave null points so charts show a gap.
                    run["result"]["scaled"] = None
                    run["result"]["speedup"] = None
                    continue
                dur = get_dur_in_us(run["result"]["duration"])
                run["result"]["scaled"] = round(float(dur) / float(lowest_dur), 2)
                if firstDur is None:
                    firstDur = dur
                run["result"]["speedup"] = round(float(firstDur) / float(dur), 2)

def populate_system_metadata(md):
    # Fill md with machine-wide CPU / core-count / kernel info. The compiler
    # version is intentionally NOT here: it is per-runtime (toolchains differ by
    # language) and lives in each runtime section's own metadata via
    # tag_runtime_metadata(). Idempotent: a second call is a no-op.
    if "cpu" in md:
        return
    try:
        # Linux
        model_name_raw = subprocess.run(args=f"lscpu | grep \"Model name:\"", shell=True, capture_output=True, text=True)
        md["cpu"] = model_name_raw.stdout.split(":")[1].strip()
    except:
        try:
            # MacOS
            md["cpu"] = subprocess.run(args=f"sysctl -n machdep.cpu.brand_string", shell=True, capture_output=True, text=True).stdout
        except:
            md["cpu"] = "unknown"
    try:
        # Linux
        model_name_raw = subprocess.run(args=f"lscpu | grep \"per socket:\"", shell=True, capture_output=True, text=True)
        md["cores"] = model_name_raw.stdout.split(":")[1].strip()
    except:
        try:
            # MacOS
            md["cores"] = subprocess.run(args=f"sysctl -n machdep.cpu.core_count", shell=True, capture_output=True, text=True).stdout
        except:
            md["cores"] = "unknown"
    try:
        kernel_raw = subprocess.run(args=f"uname -v", shell=True, capture_output=True, text=True)
        md["kernel"] = kernel_raw.stdout.strip()
    except:
        md["kernel"] = "unknown"

def write_results_json(path, metadata, results):
    # Serialize a {metadata, results} dataset to `path`; return the JSON string
    # (reused to inline into the HTML template).
    out = json.dumps({"metadata": metadata, "results": results})
    with open(path, "w") as f:
        f.write(out)
    return out

def combine_runtime_results(runtime_json_paths, output_path):
    # Fold each per-runtime RESULTS-<runtime>.json into a single dataset using the
    # merge script, then return the combined results dict. This is the same
    # operation used to recover an aborted run from its per-runtime files, so a
    # normal full run exercises that recovery path every time. merge_results
    # preserves each runtime section's leading `metadata` element.
    shutil.copyfile(runtime_json_paths[0], output_path)
    for path in runtime_json_paths[1:]:
        merge_results.merge_results(output_path, path)
    with open(output_path, "r") as f:
        return json.load(f)["results"]

_compiler_version_cache = {}

def get_compiler_version(language):
    # Run <language>/compiler-version.sh (e.g. cpp/compiler-version.sh) to get
    # that toolchain's version string. Cached per language; "unknown" on failure.
    if language in _compiler_version_cache:
        return _compiler_version_cache[language]
    version = "unknown"
    script = os.path.join(root_dir, language, "compiler-version.sh")
    try:
        result = subprocess.run(args=script, shell=True, capture_output=True, text=True)
        out = result.stdout.strip()
        if out:
            version = out
    except Exception:
        pass
    _compiler_version_cache[language] = version
    return version

def tag_runtime_metadata(result_keys, language):
    # Prepend a per-runtime `metadata` section (compiler version + the timestamp
    # this runtime finished) as the first key of each result key's section, so
    # every framework's .json is self-describing and the merge preserves it. The
    # machine-wide cpu / cores / kernel info stays in the top-level metadata.
    meta = {
        "compiler": get_compiler_version(language),
        "timestamp": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }
    for key in result_keys:
        if key in full_results:
            full_results[key] = {"metadata": meta, **full_results[key]}

args = parse_args()
compare_mode = args["compare_runtime"] is not None
single_runtime_mode = args["single_runtime"] is not None

# When benchmarking a single runtime, suffix the output files with its name
# (e.g. RESULTS-<runtime>.{json,html,md}) so a targeted run doesn't clobber
# the full-sweep RESULTS.* files.
results_basename = f"RESULTS-{args['single_runtime']}" if single_runtime_mode else "RESULTS"

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
            existing_keys = set(full_results.keys())
            run_runtime_benchmarks(language, compare_runtime, result_runtime_name, threads)
            tag_runtime_metadata([k for k in full_results if k not in existing_keys], language)
elif single_runtime_mode:
    single_runtime = args["single_runtime"]
    single_ref = args["single_ref"]
    language = get_language_for_runtime(single_runtime)
    threads = get_threads_sweep(args["full_sweep"])
    print(f"Threads sweep: {threads}")

    if build_runtime(language, single_runtime, library_ref=single_ref, clean_build=single_ref is not None):
        run_runtime_benchmarks(language, single_runtime, single_runtime, threads)
        tag_runtime_metadata(list(full_results.keys()), language)
else:
    for language, runtime_names in active_runtimes.items():
        for runtime in runtime_names:
            build_runtime(language, runtime, library_ref=os.environ.get(LIBRARY_REF_ENV_VAR))

    # Run sweep runtime -> benchmark -> threads
    threads = get_threads_sweep(args["full_sweep"])
    print(f"Threads sweep: {threads}")

    # On a full sweep, emit a per-runtime RESULTS-<runtime>.json as each runtime
    # finishes, then merge them all at the end. Each file is equivalent to what
    # benchmarking that runtime alone would produce, so an aborted run stays
    # partially recoverable: the completed runtimes' data is already on disk and
    # can be merged manually with merge_results.py.
    write_per_runtime = args["full_sweep"]
    if write_per_runtime:
        populate_system_metadata(md)

    per_runtime_paths = []
    for language, runtime_names in active_runtimes.items():
        for runtime in runtime_names:
            existing_keys = set(full_results.keys())
            run_runtime_benchmarks(language, runtime, runtime, threads)
            # A runtime may contribute several result keys (config variants such
            # as TooManyCooks_st_asio); grab whatever this run just added and tag
            # each section with its compiler version + finish timestamp.
            new_keys = [k for k in full_results if k not in existing_keys]
            tag_runtime_metadata(new_keys, language)
            if not write_per_runtime or not new_keys:
                continue  # quick run (no JSON), or build produced nothing to save
            runtime_slice = {k: full_results[k] for k in new_keys}
            compute_scaled_speedup(runtime_slice)
            path = f"RESULTS-{runtime}.json"
            write_results_json(path, md, runtime_slice)
            per_runtime_paths.append(path)
            print(f"Wrote {path}")

    # Combine the per-runtime files into the final dataset via the merge script.
    if per_runtime_paths:
        print("Merging per-runtime results into RESULTS.json...")
        full_results = combine_runtime_results(per_runtime_paths, "RESULTS.json")


# Normalize the combined dataset (relative to the global fastest per benchmark).
compute_scaled_speedup(full_results)

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

# Get system information and attach it as metadata to the JSON file only
if args["full_sweep"] or compare_mode or single_runtime_mode:
    json_path = f"{results_basename}.json"
    html_path = f"{results_basename}.html"
    print(f"Generating {json_path}...")
    populate_system_metadata(md)
    outJson = write_results_json(json_path, md, full_results)

    # Generate the HTML chart from the template for local viewing
    print(f"Generating {html_path}...")
    with open("results.html.tmpl", "r") as tmpl_file:
        html_content = tmpl_file.read()
    html_content = html_content.replace("{{ Script Will Substitute Latest Run Data Here }}", outJson)
    with open(html_path, "w") as html_file:
        html_file.write(html_content)
    print("View benchmark charts in your browser at: file:///"+os.path.abspath(html_path).replace("\\", "/"))


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

md_path = f"{results_basename}.md"
print(f"Generating {md_path}...", end=" ")
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

with open(md_path, "w") as resultsMD:
    resultsMD.write(outMD.strip() + "\n")

print("done.")
