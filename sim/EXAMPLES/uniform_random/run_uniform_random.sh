#!/usr/bin/env bash

LOADS=(0.33 6.67 13.33 26.67 40 46.67 53.33 56.66 60 63.33)
FLOWSIZES=(65536 131072 262144 524288 1048576)

for load in ${LOADS[@]} ; do
for flowsize in ${FLOWSIZES[@]} ; do
  ../../datacenter/htsim_ndp_uniform_random -strat perm -q 16 -cwnd 30 -load ${load} -flowsize ${flowsize}
  ../../parse_output logout.dat -ascii  | grep RCV | grep LASTDATA | grep FULL > flow_end_log
  echo -n "${load} ${flowsize} " >> uniform_random_out
  ./process_uniform_random.py >> uniform_random_out
done
done
