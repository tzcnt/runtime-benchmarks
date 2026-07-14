// Recursive fork-join nqueens.
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/nqueens.cpp
// which was in turn adapted from libfork's benchmark.
//
// At each column, fork a child task for every valid queen placement. Each fork
// is a ThreadPool work item started via Task.Run.

using System.Diagnostics;
using BenchUtil;

namespace Nqueens;

internal static class Program
{
    private const int NqueensWork = 14; // board size
    private const int IterCount = 1;

    // Answers[k] = number of solutions to the k-queens problem.
    private static readonly int[] Answers =
    {
        0, 1, 0, 0, 2, 10, 4, 40, 92, 352, 724, 2680, 14200, 73712, 365596,
        2279184, 14772512, 95815104, 666090624,
    };

    private static void CheckAnswer(int result)
    {
        if (result != Answers[NqueensWork])
            Console.WriteLine($"error: expected {Answers[NqueensWork]}, got {result}");
    }

    private static async Task<int> Nqueens(int xMax, byte[] buf)
    {
        if (NqueensWork == xMax)
            return 1;

        // Fork one child per valid placement; each child gets its own copy of
        // the board so the parallel branches never share mutable state.
        var tasks = new List<Task<int>>(NqueensWork);
        for (int y = 0; y < NqueensWork; y++)
        {
            int q = y;
            bool valid = true;
            for (int x = 0; x < xMax; x++)
            {
                int p = buf[x];
                int d = xMax - x;
                if (q == p || q == p - d || q == p + d)
                {
                    valid = false;
                    break;
                }
            }
            if (!valid)
                continue;
            byte[] next = (byte[])buf.Clone();
            next[xMax] = (byte)y;
            tasks.Add(Task.Run(() => Nqueens(xMax + 1, next)));
        }

        int[] values = await Task.WhenAll(tasks);
        int ret = 0;
        foreach (int v in values)
            ret += v;
        return ret;
    }

    private static async Task<int> Main(string[] args)
    {
        int threadCount = Bench.ThreadCountArg(args, 0);
        Console.WriteLine($"threads: {threadCount}");

        CheckAnswer(await Nqueens(0, new byte[NqueensWork])); // warmup

        var sw = Stopwatch.StartNew();
        int result = 0;
        for (int i = 0; i < IterCount; i++)
        {
            result = await Nqueens(0, new byte[NqueensWork]);
            CheckAnswer(result);
        }
        long durUs = (long)sw.Elapsed.TotalMicroseconds;
        Console.WriteLine($"output: {result}");

        Console.WriteLine("runs:");
        Console.WriteLine($"  - iteration_count: {IterCount}");
        Console.WriteLine($"    duration: {durUs} us");
        Console.WriteLine($"    max_rss: {Bench.PeakMemoryUsageKiB()} KiB");
        return 0;
    }
}
