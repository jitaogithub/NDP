#!/usr/bin/env bash
../../datacenter/htsim_ndp_incast_over_uniform_randon -strat perm -q 16 -cwnd 30
../../parse_output logout.dat -ascii  | grep RCV | grep LASTDATA | grep FULL > flow_end_log
./process_incast_over_uniform_random.py
