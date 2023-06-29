#!/usr/bin/env bash

LOADS=(160)
# FLOWSIZES=(131072 262144 393216 524288 1048576)
FLOWSIZES=(1048576)
Q=64
CWND=32

for load in ${LOADS[@]} ; do
for flowsize in ${FLOWSIZES[@]} ; do
  (../../datacenter/htsim_ndp_5node_shortflows -strat perm -q ${Q} -cwnd ${CWND} -load ${load} -flowsize ${flowsize} -o logout_${load}_${flowsize} -m flow_meta_${load}_${flowsize};\
  ../../parse_output logout_${load}_${flowsize} -ascii  | grep RCV | grep LASTDATA | grep FULL > flow_end_log_${load}_${flowsize}) &
done
done

wait


for load in ${LOADS[@]} ; do
for flowsize in ${FLOWSIZES[@]} ; do
echo -n "${load} ${flowsize} " >> 5node_shortflows_out_q${Q}_cwnd${CWND}
./process_5node_shortflows.py flow_end_log_${load}_${flowsize} flow_meta_${load}_${flowsize} >> 5node_shortflows_out_q${Q}_cwnd${CWND}
done
done