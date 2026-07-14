// Test performance of an MPMC channel primitive.
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/channel.cpp
//
// N producers each post a slice of elementCount items; M consumers pull until
// the channel is drained; the total count and sum are validated.
//
// The .NET stdlib's native async MPMC primitive is System.Threading.Channels.
// An unbounded channel is used to mirror TMC's unbounded queue (producers never
// park on a full buffer); like TMC's chan.post, Writer.TryWrite enqueues
// synchronously without allocating a Task. AllowSynchronousContinuations stays
// off so a producer never runs consumer code inline.

using System.Diagnostics;
using System.Threading.Channels;
using BenchUtil;

namespace Channel;

internal static class Program
{
    private const long ElementCount = 10_000_000;
    private const int IterCount = 1;

    private readonly record struct Counts(ulong Count, ulong Sum);
    private readonly record struct Assignment(ulong Count, ulong Base);

    // Splits ElementCount into per-producer (count, base) work.
    private static Assignment[] ProducerAssignments(int producerCount)
    {
        ulong per = (ulong)ElementCount / (ulong)producerCount;
        ulong rem = (ulong)ElementCount % (ulong)producerCount;
        var outp = new Assignment[producerCount];
        ulong baseVal = 0;
        for (int i = 0; i < producerCount; i++)
        {
            ulong count = per + ((ulong)i < rem ? 1UL : 0UL);
            outp[i] = new Assignment(count, baseVal);
            baseVal += count;
        }
        return outp;
    }

    private static async Task<ulong> DoBench(int producerCount, int consumerCount)
    {
        var ch = System.Threading.Channels.Channel.CreateUnbounded<ulong>(
            new UnboundedChannelOptions
            {
                SingleReader = false,
                SingleWriter = false,
                AllowSynchronousContinuations = false,
            });
        ChannelWriter<ulong> writer = ch.Writer;
        ChannelReader<ulong> reader = ch.Reader;

        // Fork the consumers first; they park until data arrives.
        var consumers = new Task<Counts>[consumerCount];
        for (int i = 0; i < consumerCount; i++)
        {
            consumers[i] = Task.Run(async () =>
            {
                ulong count = 0, sum = 0;
                while (await reader.WaitToReadAsync())
                    while (reader.TryRead(out ulong v))
                    {
                        count++;
                        sum += v;
                    }
                return new Counts(count, sum);
            });
        }

        // Run the producers.
        Assignment[] assignments = ProducerAssignments(producerCount);
        var producers = new Task[producerCount];
        for (int i = 0; i < producerCount; i++)
        {
            Assignment a = assignments[i];
            producers[i] = Task.Run(() =>
            {
                for (ulong k = 0; k < a.Count; k++)
                {
                    bool ok = writer.TryWrite(a.Base + k); // unbounded: always true
                    if (!ok)
                        Console.WriteLine("FAIL: TryWrite rejected on unbounded channel");
                }
            });
        }

        // Once every producer has finished, complete the writer so the consumers
        // finish draining what remains and their loops terminate.
        await Task.WhenAll(producers);
        writer.Complete();
        Counts[] results = await Task.WhenAll(consumers);

        ulong totalCount = 0, totalSum = 0;
        foreach (Counts r in results)
        {
            totalCount += r.Count;
            totalSum += r.Sum;
        }

        ulong expectedSum = (ulong)ElementCount * ((ulong)ElementCount - 1) / 2;
        if (totalCount != (ulong)ElementCount)
            Console.WriteLine($"FAIL: Expected {ElementCount} elements but consumed {totalCount} elements");
        if (totalSum != expectedSum)
            Console.WriteLine($"FAIL: Expected {expectedSum} sum but got {totalSum} sum");
        return totalSum;
    }

    private static async Task<int> Main(string[] args)
    {
        int threadCount = Bench.ThreadCountArg(args, 0);

        int per = threadCount / 2;
        if (per < 1)
            per = 1;
        int producerCount = per;
        int consumerCount = per;

        Console.WriteLine($"threads: {threadCount}");
        Console.WriteLine($"producers: {producerCount}");
        Console.WriteLine($"consumers: {consumerCount}");

        ulong result = await DoBench(producerCount, consumerCount); // warmup
        Console.WriteLine($"output: {result}");

        var sw = Stopwatch.StartNew();
        for (int i = 0; i < IterCount; i++)
            await DoBench(producerCount, consumerCount);
        long durUs = (long)sw.Elapsed.TotalMicroseconds;
        if (durUs < 1)
            durUs = 1;
        long elementsPerSec = ElementCount * 1_000_000 / durUs;

        Console.WriteLine("runs:");
        Console.WriteLine($"  - iteration_count: {IterCount}");
        Console.WriteLine($"    elements: {ElementCount}");
        Console.WriteLine($"    duration: {durUs} us");
        Console.WriteLine($"    elements/sec: {elementsPerSec}");
        Console.WriteLine($"    max_rss: {Bench.PeakMemoryUsageKiB()} KiB");
        return 0;
    }
}
