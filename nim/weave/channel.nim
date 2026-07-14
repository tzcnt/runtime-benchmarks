# Test performance of an MPMC channel primitive.
#
# Canonical (TooManyCooks) implementation:
# ../../cpp/TooManyCooks/channel.cpp
#
# Weave has no public MPMC channel (its internal channels are scheduler
# plumbing), and its workers must never block, so this benchmark measures
# Nim's official MPMC channel package - threading/channels
# (https://github.com/nim-lang/threading) - with one OS thread per producer
# and consumer. The harness labels the result "weave_threading" via
# benchmark_configs in build_and_bench_all.py, like tokio_flume.
#
# N producers each post a slice of elementCount items; M consumers pull until
# the channel is drained; the total count and sum are validated. Like the Go
# port, the channel is bounded (threading/channels has no unbounded mode) with
# a generous buffer so the benchmark measures steady-state throughput.
# threading/channels also has no close(): after the producers are joined, the
# main thread sends one sentinel per consumer to terminate them. The sentinel
# value is outside the produced range of [0, elementCount).

import std/[monotimes, times]
import threading/channels
import benchutil

const elementCount = 10_000_000
const iterCount = 1
const channelBuffer = 1 shl 16
const sentinel = high(uint64)

type Counts = object
  count: uint64
  sum: uint64

type ProducerArg = object
  chan: Chan[uint64]
  count: uint64
  base: uint64

type ConsumerArg = object
  chan: Chan[uint64]
  res: ptr Counts

proc producer(arg: ProducerArg) {.thread.} =
  for i in 0'u64 ..< arg.count:
    arg.chan.send(arg.base + i)

proc consumer(arg: ConsumerArg) {.thread.} =
  var count = 0'u64
  var sum = 0'u64
  var v: uint64
  while true:
    arg.chan.recv(v)
    if v == sentinel:
      break
    inc count
    sum += v
  arg.res[] = Counts(count: count, sum: sum)

proc doBench(producerCount, consumerCount: int): uint64 =
  var chan = newChan[uint64](channelBuffer)

  # Split elementCount into per-producer (count, base) assignments.
  var producers = newSeq[Thread[ProducerArg]](producerCount)
  let per = uint64(elementCount) div uint64(producerCount)
  let rem = uint64(elementCount) mod uint64(producerCount)
  var base = 0'u64
  for i in 0 ..< producerCount:
    var count = per
    if uint64(i) < rem:
      inc count
    createThread(producers[i], producer,
                 ProducerArg(chan: chan, count: count, base: base))
    base += count

  var results = newSeq[Counts](consumerCount)
  var consumers = newSeq[Thread[ConsumerArg]](consumerCount)
  for i in 0 ..< consumerCount:
    createThread(consumers[i], consumer,
                 ConsumerArg(chan: chan, res: addr results[i]))

  # Once every producer has finished, all real values precede the sentinels in
  # the FIFO, so each consumer drains its share and stops on its sentinel.
  joinThreads(producers)
  for _ in 0 ..< consumerCount:
    chan.send(sentinel)
  joinThreads(consumers)

  var total = Counts()
  for r in results:
    total.count += r.count
    total.sum += r.sum

  let expectedSum = uint64(elementCount) * (uint64(elementCount) - 1) div 2
  if total.count != uint64(elementCount):
    echo "FAIL: Expected ", elementCount, " elements but consumed ",
         total.count, " elements"
  if total.sum != expectedSum:
    echo "FAIL: Expected ", expectedSum, " sum but got ", total.sum, " sum"
  result = total.sum

proc main() =
  let threadCount = threadCountArg(1)

  var per = threadCount div 2
  if per < 1:
    per = 1
  let producerCount = per
  let consumerCount = per

  echo "threads: ", threadCount
  echo "producers: ", producerCount
  echo "consumers: ", consumerCount

  let res = doBench(producerCount, consumerCount) # warmup
  echo "output: ", res

  let start = getMonoTime()
  for _ in 0 ..< iterCount:
    discard doBench(producerCount, consumerCount)
  var durUs = (getMonoTime() - start).inMicroseconds
  if durUs < 1:
    durUs = 1
  let elementsPerSec = int64(elementCount) * 1_000_000 div durUs

  echo "runs:"
  echo "  - iteration_count: ", iterCount
  echo "    elements: ", elementCount
  echo "    duration: ", durUs, " us"
  echo "    elements/sec: ", elementsPerSec
  echo "    max_rss: ", peakMemoryUsageKiB(), " KiB"

when isMainModule:
  main()
