#!/usr/bin/env python3

import sys
import matplotlib
matplotlib.use('pdf')
matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42
# matplotlib.rc('font',**{'family':'sans-serif','sans-serif':['Helvetica']})
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.lines as mlines
import numpy as np
from functools import partial

VICTIM=int(sys.argv[1])
FLOW_END_LOG  = "flow_end_log"
FLOW_META = "flow_meta"
RANGE = (0,4000000)

flow_end_times = dict()
flow_end_log = open(FLOW_END_LOG, "r")
last_end_time = 0
for line in flow_end_log.readlines():
  data = line.split()
  time = float(data[0])
  flow_id = int(data[8])
  if (time > last_end_time):
    last_end_time = time
  flow_end_times[flow_id] = time * 1e9

flows = {
  "ur" : [],
  "incast" : []
}
flow_meta = open(FLOW_META, "r")
for line in flow_meta.readlines():
  data = line.split(",")
  name = data[0].split("_")
  flow_id = int(data[1])
  if flow_id not in flow_end_times:
    continue
  dst = int(name[2])
  if dst != VICTIM:
    continue
  start_time = float(data[3]);
  end_time = flow_end_times[flow_id]
  if (end_time < RANGE[0] or end_time > RANGE[1]):
    continue
  flow = {
    "src": int(name[1]),
    "dst": dst,
    "repeat": int(name[3]),
    "id": flow_id,
    "start_time": start_time,
    "end_time": end_time
  }
  flow["fct"] = int(end_time - start_time) 
  flows[name[0]].append(flow)

fig = plt.figure(figsize=(6.4,4.8)) # 6.4:4.8

ax = fig.add_subplot(111)
for flow_type, flows in flows.items():
  flows_sorted = sorted(flows, key=lambda flow: flow["end_time"])
  xs = list(map(lambda flow: flow["end_time"], flows_sorted))
  ys = list(map(lambda flow: flow["fct"], flows_sorted))
  ax.scatter(xs, ys, label = flow_type, s=3)

# ax.set_xscale('log')
# ax.set_xlim((4000000,10000000))
# ax.set_xticks(XTICKS)
# ax.set_xticklabels(XTICKSLABEL, fontsize=18)
# ax.set_ylim((0.55,1))
# ax.set_yticks(YTICKS)
# ax.set_yticklabels(YTICKSLABEL, fontsize=18)

ax.set_yscale('log')

ax.grid(True, axis='both')
ax.set_xlabel("Elapsed time", fontsize=18)
ax.set_ylabel("FCT", fontsize=18)

# max_lat_us = [np.max(all_data[i]) for i in range (3)]
# ax.annotate("Max (Both):\n%.1f$\mu$s" % (max_lat_us[0]), 
#             xy =(max_lat_us[0]*0.98, 0.98),  xytext=(5, 0.6), color='red', size=18,
#             arrowprops = {'width': 1,'headwidth':8, 'headlength':8, 'color':'red', 'shrink': 1})

# patches = [mlines.Line2D([], [], color=COLORS[k], label=LABELS[k], linewidth=2) \
#             for k, _ in all_data.items()]

plt.legend()
# plt.legend(handles=patches, loc ="lower center", bbox_to_anchor=(0.5, 0.95),
#          ncol=3, prop={'size':18}, columnspacing=1, frameon=False)
plt.subplots_adjust(bottom=0.21 ,top = 0.87, left= 0.12, right=0.96)
# plt.tight_layout()
plt.savefig('incast_over_uniform_random.png', format='png')
