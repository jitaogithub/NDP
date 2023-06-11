#!/usr/bin/env python3

flow_end_times = dict()
flow_end_log = open("flow_end_log", "r")
last_end_time = 0
for line in flow_end_log.readlines():
  data = line.split()
  time = float(data[0])
  flow_id = int(data[8])
  if (time > last_end_time):
    last_end_time = time
  flow_end_times[flow_id] = time * 1e9

flows = []
flow_meta = open("flow_meta", "r")
for line in flow_meta.readlines():
  data = line.split(",")
  name = data[0].split("_")
  flow_id = int(data[1])
  if flow_id not in flow_end_times:
    continue
  flow = {
    "src": int(name[0]),
    "dst": int(name[1]),
    "repeat": int(name[2]),
    "id": flow_id,
    "start_time": float(data[3]),
    "end_time": flow_end_times[flow_id]
  }
  flows.append(flow)



interested_pairs = [
  {"src": 0, "dst": 12},
  {"src": 24, "dst": 12},
  {"src": 24, "dst": 36},
  {"src": 24, "dst": 48},
]


for pair in interested_pairs:
  fcts = []
  flow_completion_times  = []
  completed_count = 0
  for flow in flows:
    if flow["src"] != pair["src"] or flow["dst"] != pair["dst"]:
      continue
    flow_completion_times.append(int(flow["end_time"] - flow["start_time"]))
    completed_count += 1
  fcts = sorted(flow_completion_times)
  goodput = completed_count * 524288 * 8 / last_end_time / 1e9 
  print(goodput, fcts[int(len(fcts) * 0.5)],fcts[int(len(fcts) * 0.99)] )

