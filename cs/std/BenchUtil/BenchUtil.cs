// Helpers shared across the C# stdlib benchmark binaries so their setup and
// reported numbers line up with the C++ (TMC), Rust (tokio) and Go suites.

using System.Diagnostics;
using System.Runtime.InteropServices;

namespace BenchUtil;

public static class Bench
{
    // --- Peak resident set size, mirroring cpp/2common/memusage.hpp and
    //     go/std/internal/benchutil so the reported numbers are directly
    //     comparable across languages. ---

    private const int RusageSelf = 0;

    // struct rusage on Linux/macOS (x86_64/arm64). Only ru_maxrss (the 5th long,
    // right after the two timevals) is read, but the full 144-byte layout is
    // declared so getrusage can safely write the whole struct.
    [StructLayout(LayoutKind.Sequential)]
    private struct Rusage
    {
        public long RuUtimeSec, RuUtimeUsec; // struct timeval ru_utime
        public long RuStimeSec, RuStimeUsec; // struct timeval ru_stime
        public long RuMaxrss;
        public long RuIxrss, RuIdrss, RuIsrss;
        public long RuMinflt, RuMajflt, RuNswap;
        public long RuInblock, RuOublock;
        public long RuMsgsnd, RuMsgrcv, RuNsignals;
        public long RuNvcsw, RuNivcsw;
    }

    [DllImport("libc", SetLastError = true)]
    private static extern int getrusage(int who, out Rusage usage);

    // Returns peak resident set size of the current process in KiB.
    public static long PeakMemoryUsageKiB()
    {
        if (OperatingSystem.IsWindows())
            return Process.GetCurrentProcess().PeakWorkingSet64 / 1024;
        if (getrusage(RusageSelf, out Rusage ru) != 0)
            return -1;
        // Linux reports ru_maxrss in KiB; macOS/BSD report it in bytes.
        return OperatingSystem.IsMacOS() ? ru.RuMaxrss / 1024 : ru.RuMaxrss;
    }

    // --- Worker-thread pinning: the .NET analogue of Go's GOMAXPROCS or an
    //     executor's fixed thread count. ---

    // Pins the ThreadPool to exactly n worker threads (min == max == n) so the
    // fork-join benchmarks honor the harness's thread-count sweep. SetMaxThreads
    // rejects a value below the current min, and SetMinThreads rejects one above
    // the current max, so the min is first dropped to 1 before the max is set;
    // this pins any n >= 1, including counts below Environment.ProcessorCount.
    public static int ConfigureThreads(int n)
    {
        if (n < 1) n = 1;
        ThreadPool.GetMinThreads(out _, out int ioMin);
        ThreadPool.GetMaxThreads(out _, out int ioMax);
        ThreadPool.SetMinThreads(1, ioMin);
        ThreadPool.SetMaxThreads(n, ioMax);
        ThreadPool.SetMinThreads(n, ioMin);
        return n;
    }

    // Default worker count, mirroring the C++/Rust/Go default of
    // hardware_concurrency() / 2.
    public static int DefaultThreads()
    {
        int n = Environment.ProcessorCount / 2;
        return n < 1 ? 1 : n;
    }

    // Parses args[idx] as a worker-thread count (falling back to DefaultThreads)
    // and pins the ThreadPool to it. The harness passes the thread count in this
    // position (see build_and_bench_all.py). Note that C# args[] excludes the
    // program name, so idx is one less than the C++/Go argv position.
    public static int ThreadCountArg(string[] args, int idx)
    {
        int n = DefaultThreads();
        if (args.Length > idx && int.TryParse(args[idx], out int v) && v > 0)
            n = v;
        return ConfigureThreads(n);
    }
}
