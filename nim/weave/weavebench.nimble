# Dependency manifest for the Nim / Weave benchmarks. This is not a real
# installable package; it exists so nimble can resolve and lock dependencies.
#
# Exact dependency revisions (including transitive ones, e.g. weave's
# `synthesis`) are pinned by the committed nimble.lock. To change the weave
# version: edit the requirement below, delete nimble.lock, and re-run
# `nimble lock`.

version       = "0.1.0"
author        = "runtime-benchmarks"
description   = "Weave / Nim benchmarks for runtime-benchmarks"
license       = "MIT"

requires "nim >= 2.0.0"
requires "weave == 0.4.10"
# Nim's official MPMC channel package, used by the channel benchmark
# (weave has no public MPMC channel). It has no tagged releases; the exact
# revision is pinned by nimble.lock.
requires "threading >= 0.2.0"
