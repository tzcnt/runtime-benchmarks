#!/bin/sh

# from https://stackoverflow.com/a/23378780/19260728
# macOS:           Use `sysctl -n hw.*cpu_max`, which returns the values of 
#                  interest directly.
#                  CAVEAT: Using the "_max" key suffixes means that the *maximum*
#                          available number of CPUs is reported, whereas the
#                          current power-management mode could make *fewer* CPUs 
#                          available; dropping the "_max" suffix would report the
#                          number of *currently* available ones; see [1] below.
#
# Linux:           Parse output from `lscpu -p`, where each output line represents
#                  a distinct (logical) CPU.
#                  Note: Newer versions of `lscpu` support more flexible output
#                        formats, but we stick with the parseable legacy format 
#                        generated by `-p` to support older distros, too.
#                        `-p` reports *online* CPUs only - i.e., on hot-pluggable 
#                        systems, currently disabled (offline) CPUs are NOT
#                        reported.

# Number of LOGICAL CPUs (includes those reported by hyper-threading cores)
  # Linux: Simply count the number of (non-comment) output lines from `lscpu -p`, 
  # which tells us the number of *logical* CPUs.
logicalCpuCount=$([ $(uname) = 'Darwin' ] && 
                       sysctl -n hw.logicalcpu_max || 
                       lscpu -p | egrep -v '^#' | wc -l)

# Number of PHYSICAL CPUs (cores).
  # Linux: The 2nd column contains the core ID, with each core ID having 1 or
  #        - in the case of hyperthreading - more logical CPUs.
  #        Counting the *unique* cores across lines tells us the
  #        number of *physical* CPUs (cores).
physicalCpuCount=$([ $(uname) = 'Darwin' ] && 
                       sysctl -n hw.physicalcpu_max ||
                       lscpu -p | egrep -v '^#' | sort -u -t, -k 2,4 | wc -l)
# Print the values.
# cat <<EOF
# # of logical CPUs:  $logicalCpuCount
# # of physical CPUS: $physicalCpuCount
# EOF

# For this benchmark suite, just use the number of physical CPUs, as hyperthreading
# makes it more difficult to assess the speedup relative to the number of cores on different machines.
echo $physicalCpuCount
