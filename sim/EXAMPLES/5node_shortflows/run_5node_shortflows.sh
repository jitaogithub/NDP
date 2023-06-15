#!/usr/bin/env bash

LOADS=(140 160 170 180 190)
FLOWSIZES=(65536 131072 262144 524288 1048576)

for load in ${LOADS[@]} ; do
for flowsize in ${FLOWSIZES[@]} ; do
  (../../datacenter/htsim_ndp_5node_shortflows -strat perm -q 16 -cwnd 30 -load ${load} -flowsize ${flowsize} ;\
  ../../parse_output logout.dat -ascii  | grep RCV | grep LASTDATA | grep FULL > flow_end_log_${load}_${flowsize}) &
done
done

wait

echo -n "${load} ${flowsize} " >> 5node_shortflows_out_${load}_${flowsize}
./process_5node_shortflows.py >> 5node_shortflows_out_${load}_${flowsize}
