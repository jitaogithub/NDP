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

flows = {
  "ur" : [],
  "incast" : []
}
flow_meta = open("flow_meta", "r")
for line in flow_meta.readlines():
  data = line.split(",")
  name = data[0].split("_")
  flow_id = int(data[1])
  if flow_id not in flow_end_times:
    continue
  flow = {
    "src": int(name[1]),
    "dst": int(name[2]),
    "repeat": int(name[3]),
    "id": flow_id,
    "start_time": float(data[3]),
    "end_time": flow_end_times[flow_id],
    "size": int(data[4])
  }
  flows[name[0]].append(flow)


fcts = dict()
for flow_type, flows in flows.items():
  flow_completion_times  = []
  completed_bytes = 0

  for flow in flows:
    flow_completion_times.append(int(flow["end_time"] - flow["start_time"]))
    completed_bytes += flow["size"]
  fcts[flow_type] = sorted(flow_completion_times)
  goodput = completed_bytes * 8 / last_end_time / (48 if flow_type == "ur" else 100) / 1e9 
  print(flow_type,goodput, fcts[flow_type][int(len(fcts[flow_type]) * 0.5)],fcts[flow_type][int(len(fcts[flow_type]) * 0.99)] )

