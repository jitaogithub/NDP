#!/usr/bin/env bash
../../datacenter/htsim_ndp_5node_shortflows -strat perm -q 16 -cwnd 30
../../parse_output logout.dat -ascii  | grep RCV | grep LASTDATA | grep FULL > flow_end_log
./process_5node_shortflows.py
