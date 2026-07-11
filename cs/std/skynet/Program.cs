// The skynet benchmark as described here:
// https://github.com/atemerev/skynet
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/skynet.cpp
//
// Each node forks 10 children until DepthMax levels deep (10^DepthMax = 100M
// leaf tasks) and sums their results. Here each child is a ThreadPool work item
// started via Task.Run, so this measures .NET's task spawn/join throughput under
// deep nested fan-out.

using System.Diagnostics;
using BenchUtil;

namespace Skynet;

internal static class Program
{
    private const int DepthMax = 8;
    private const int IterCount = 1;

    private static async Task<ulong> SkynetOne(ulong baseNum, ulong depth)
    {
        if (depth == DepthMax)
            return baseNum;
        ulong offset = 1;
        for (ulong i = 0; i < DepthMax - depth - 1; i++)
            offset *= 10;

        // Each child writes its own slot, so the disjoint writes need no
        // synchronization beyond WhenAll's happens-before on completion.
        var tasks = new Task<ulong>[10];
        for (ulong i = 0; i < 10; i++)
        {
            ulong childBase = baseNum + offset * i;
            ulong childDepth = depth + 1;
            tasks[i] = Task.Run(() => SkynetOne(childBase, childDepth));
        }
        ulong[] results = await Task.WhenAll(tasks);

        ulong count = 0;
        foreach (ulong r in results)
            count += r;
        return count;
    }

    private static async Task Skynet(ulong expected)
    {
        ulong count = await SkynetOne(0, 0);
        if (count != expected)
            Console.WriteLine($"ERROR: wrong result - {count}");
    }

    private static async Task<int> Main(string[] args)
    {
        int threadCount = Bench.ThreadCountArg(args, 0);

        ulong leaves = 1;
        for (int i = 0; i < DepthMax; i++)
            leaves *= 10;
        ulong expected = (leaves - 1) * leaves / 2;

        Console.WriteLine($"threads: {threadCount}");

        await Skynet(expected); // warmup

        Console.WriteLine("runs:");
        var sw = Stopwatch.StartNew();
        for (int i = 0; i < IterCount; i++)
            await Skynet(expected);
        long durUs = (long)sw.Elapsed.TotalMicroseconds;
        Console.WriteLine($"  - iteration_count: {IterCount}");
        Console.WriteLine($"    duration: {durUs} us");
        Console.WriteLine($"    max_rss: {Bench.PeakMemoryUsageKiB()} KiB");
        return 0;
    }
}
