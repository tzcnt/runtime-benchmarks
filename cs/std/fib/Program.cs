// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/fib.cpp
//
// Fork a task that runs fib(n-1) in parallel with the current task, then
// continue the other leg (fib(n-2)) inline. Each fork is a ThreadPool work item
// started via Task.Run, so this measures .NET's task spawn/join throughput on
// the stdlib work-stealing ThreadPool.

using System.Diagnostics;
using BenchUtil;

namespace Fib;

internal static class Program
{
    private const int IterCount = 1;

    private static async Task<ulong> Fib(ulong n)
    {
        if (n < 2)
            return n;
        // Fork fib(n-1) onto the ThreadPool; run fib(n-2) on the current thread.
        Task<ulong> x = Task.Run(() => Fib(n - 1));
        ulong y = await Fib(n - 2);
        ulong xr = await x;
        return xr + y;
    }

    private static async Task<int> Main(string[] args)
    {
        if (args.Length < 1 || !ulong.TryParse(args[0], out ulong n))
        {
            Console.WriteLine("Usage: fib <n-th fibonacci number requested>");
            return 0;
        }
        int threadCount = Bench.ThreadCountArg(args, 1);
        Console.WriteLine($"threads: {threadCount}");

        _ = await Fib(30); // warmup

        var sw = Stopwatch.StartNew();
        ulong result = 0;
        for (int i = 0; i < IterCount; i++)
            result = await Fib(n);
        long durUs = (long)sw.Elapsed.TotalMicroseconds;
        Console.WriteLine($"output: {result}");

        Console.WriteLine("runs:");
        Console.WriteLine($"  - iteration_count: {IterCount}");
        Console.WriteLine($"    duration: {durUs} us");
        Console.WriteLine($"    max_rss: {Bench.PeakMemoryUsageKiB()} KiB");
        return 0;
    }
}
