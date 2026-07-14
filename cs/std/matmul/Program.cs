// Recursive divide-and-conquer matrix multiplication.
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/matmul.cpp
// base case: ../../../cpp/2common/matmul.hpp
//
// The matrices are split into quadrants recursively down to a 32x32 base case.
// The 8 sub-multiplications are run as two sequential groups of 4 parallel
// tasks (Task.Run onto the ThreadPool) so that no output tile is written by two
// tasks at once.
//
// A, B and C share three flat backing arrays; each Mat value carries base
// offsets (Ao/Bo/Co) and the full-matrix stride (Nn) into them. The parallel
// tasks in a group write disjoint sub-tiles of C, so no locking is required.

using System.Diagnostics;
using BenchUtil;

namespace Matmul;

internal readonly struct Mat
{
    public readonly int[] A, B, C;
    public readonly int Ao, Bo, Co; // base offsets into A/B/C
    public readonly int N, Nn;      // submatrix size, and stride (full dim N)

    public Mat(int[] a, int[] b, int[] c, int ao, int bo, int co, int n, int nn)
    {
        A = a; B = b; C = c;
        Ao = ao; Bo = bo; Co = co;
        N = n; Nn = nn;
    }
}

internal static class Program
{
    private static void MatmulSmall(in Mat m)
    {
        for (int i = 0; i < m.N; i++)
            for (int k = 0; k < m.N; k++)
                for (int j = 0; j < m.N; j++)
                    m.C[m.Co + i * m.Nn + j] += m.A[m.Ao + i * m.Nn + k] * m.B[m.Bo + k * m.Nn + j];
    }

    private static async Task Matmul(Mat m)
    {
        if (m.N <= 32)
        {
            MatmulSmall(m);
            return;
        }
        int k = m.N / 2;
        int nn = m.Nn;
        int[] a = m.A, b = m.B, c = m.C;
        int ao = m.Ao, bo = m.Bo, co = m.Co;

        // Group 1: the four products that only touch the "upper-left" halves.
        await RunGroup(
            new Mat(a, b, c, ao, bo, co, k, nn),
            new Mat(a, b, c, ao, bo + k, co + k, k, nn),
            new Mat(a, b, c, ao + k * nn, bo, co + k * nn, k, nn),
            new Mat(a, b, c, ao + k * nn, bo + k, co + k * nn + k, k, nn));
        // Group 2: the accumulating products into the same tiles.
        await RunGroup(
            new Mat(a, b, c, ao + k, bo + k * nn, co, k, nn),
            new Mat(a, b, c, ao + k, bo + k * nn + k, co + k, k, nn),
            new Mat(a, b, c, ao + k * nn + k, bo + k * nn, co + k * nn, k, nn),
            new Mat(a, b, c, ao + k * nn + k, bo + k * nn + k, co + k * nn + k, k, nn));
    }

    private static async Task RunGroup(Mat m0, Mat m1, Mat m2, Mat m3)
    {
        Task t0 = Task.Run(() => Matmul(m0));
        Task t1 = Task.Run(() => Matmul(m1));
        Task t2 = Task.Run(() => Matmul(m2));
        Task t3 = Task.Run(() => Matmul(m3));
        await Task.WhenAll(t0, t1, t2, t3);
    }

    private static int[] RunMatmul(int n)
    {
        var a = new int[n * n];
        var b = new int[n * n];
        var c = new int[n * n];
        for (int i = 0; i < a.Length; i++)
        {
            a[i] = 1;
            b[i] = 1;
        }
        Matmul(new Mat(a, b, c, 0, 0, 0, n, n)).GetAwaiter().GetResult();
        return c;
    }

    private static void Validate(int[] c, int n)
    {
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
            {
                int res = c[i * n + j];
                if (res != n)
                {
                    Console.WriteLine($"Wrong result at ({i},{j}) : {res}. expected {n}");
                    Environment.Exit(1);
                }
            }
    }

    private static int Main(string[] args)
    {
        if (args.Length < 1 || !int.TryParse(args[0], out int n))
        {
            Console.WriteLine("Usage: matmul <matrix size (power of 2)>");
            return 0;
        }
        int threadCount = Bench.ThreadCountArg(args, 1);
        Console.WriteLine($"threads: {threadCount}");

        _ = RunMatmul(n); // warmup

        Console.WriteLine("runs:");
        var sw = Stopwatch.StartNew();
        int[] result = RunMatmul(n);
        long durUs = (long)sw.Elapsed.TotalMicroseconds;
        Validate(result, n);

        Console.WriteLine($"  - matrix_size: {n}");
        Console.WriteLine($"    duration: {durUs} us");
        Console.WriteLine($"    max_rss: {Bench.PeakMemoryUsageKiB()} KiB");
        return 0;
    }
}
