// -*- c-basic-offset: 2; tab-width: 2; indent-tabs-mode: s -*-
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <sstream>
#include <strstream>

#include "config.h"
#include "network.h"
#include "randomqueue.h"
// #include "subflow_control.h"
#include "clock.h"
#include "compositequeue.h"
#include "connection_matrix.h"
#include "eventlist.h"
#include "firstfit.h"
#include "logfile.h"
#include "loggers.h"
#include "ndp.h"
#include "pipe.h"
#include "shortflows.h"
#include "topology.h"
// #include "vl2_topology.h"

#include "fat_tree_topology.h"
// #include "oversubscribed_fat_tree_topology.h"
// #include "multihomed_fat_tree_topology.h"
// #include "star_topology.h"
// #include "bcube_topology.h"
#include <list>

// Simulation params

#define PRINT_PATHS 0

#define PERIODIC 0
#include "main.h"

// int RTT = 10; // this is per link delay; identical RTT microseconds = 0.02 ms
uint32_t RTT =
    1;  // this is per link delay in us; identical RTT microseconds = 0.02 ms
int DEFAULT_NODES = 432;

// queuesize of 8 can go up to 100Gbps, or it will be much smaller than BDP
// which can cause unfairness
#define DEFAULT_QUEUE_SIZE 16

FirstFit* ff = NULL;
unsigned int subflow_count = 1;

string ntoa(double n);
string itoa(uint64_t n);

// #define SWITCH_BUFFER (SERVICE * RTT / 1000)
#define USE_FIRST_FIT 0
#define FIRST_FIT_INTERVAL 100

EventList eventlist;

Logfile* lg;

void exit_error(char* progr) {
  cout << "Usage " << progr
       << " [UNCOUPLED(DEFAULT)|COUPLED_INC|FULLY_COUPLED|COUPLED_EPSILON] "
          "[epsilon][COUPLED_SCALABLE_TCP"
       << endl;
  exit(1);
}

void print_path(std::ofstream& paths, const Route* rt) {
  for (unsigned int i = 1; i < rt->size() - 1; i += 2) {
    RandomQueue* q = (RandomQueue*)rt->at(i);
    if (q != NULL)
      paths << q->str() << " ";
    else
      paths << "NULL ";
  }

  paths << endl;
}

struct Flow {
  string name;
  int src;
  int dst;
  int flowsize;
  double start_time_ns;

  // Runtime information
  int src_id;
  int sink_id;
};

double rand_exp(double lambda) {
  double u;
  u = rand() / (RAND_MAX + 1.0);
  return -log(1 - u) / lambda;
}

int main(int argc, char** argv) {
  Packet::set_packet_size(9000);
  double end_time = 0.1;
  eventlist.setEndtime(timeFromSec(end_time));
  Clock c(timeFromSec(5 / 100.), eventlist);
  mem_b queuesize = memFromPkt(DEFAULT_QUEUE_SIZE);
  // cwnd of 15 can go up to 100 Gbps, or it will be smaller than BDP
  int no_of_conns = 0, cwnd = 30, no_of_nodes = DEFAULT_NODES,
      flowsize = 524288;
  stringstream filename(ios_base::out);
  RouteStrategy route_strategy = NOT_SET;

  double ur_tput_gbps = 160;

  int i = 1;
  filename << "logout.dat";

  while (i < argc) {
    if (!strcmp(argv[i], "-o")) {
      filename.str(std::string());
      filename << argv[i + 1];
      i++;
    } else if (!strcmp(argv[i], "-sub")) {
      subflow_count = atoi(argv[i + 1]);
      i++;
      // } else if (!strcmp(argv[i], "-conns")) {
      //   no_of_conns = atoi(argv[i + 1]);
      //   cout << "no_of_conns " << no_of_conns << endl;
      //   i++;
    } else if (!strcmp(argv[i], "-nodes")) {
      no_of_nodes = atoi(argv[i + 1]);
      cout << "no_of_nodes " << no_of_nodes << endl;
      i++;
    } else if (!strcmp(argv[i], "-cwnd")) {
      cwnd = atoi(argv[i + 1]);
      cout << "cwnd " << cwnd << endl;
      i++;
      // } else if (!strcmp(argv[i], "-flowsize")) {
      //   flowsize = atoi(argv[i + 1]);
      //   cout << "flowsize " << flowsize << endl;
      //   i++;
    } else if (!strcmp(argv[i], "-q")) {
      queuesize = memFromPkt(atoi(argv[i + 1]));
      i++;
    } else if (!strcmp(argv[i], "-strat")) {
      if (!strcmp(argv[i + 1], "perm")) {
        route_strategy = SCATTER_PERMUTE;
      } else if (!strcmp(argv[i + 1], "rand")) {
        route_strategy = SCATTER_RANDOM;
      } else if (!strcmp(argv[i + 1], "pull")) {
        route_strategy = PULL_BASED;
      } else if (!strcmp(argv[i + 1], "single")) {
        route_strategy = SINGLE_PATH;
      }
      i++;
    } else
      exit_error(argv[0]);

    i++;
  }
  srand(13);
  // std::mt19937 rand_gen(13);

  if (route_strategy == NOT_SET) {
    fprintf(stderr,
            "Route Strategy not set.  Use the -strat param.  \nValid values "
            "are perm, rand, pull, rg and single\n");
    exit(1);
  }
  cout << "Using subflow count " << subflow_count << endl;

  // prepare the loggers

  cout << "Logging to " << filename.str() << endl;
  // Logfile
  Logfile logfile(filename.str(), eventlist);

#if PRINT_PATHS
  filename << ".paths";
  cout << "Logging path choices to " << filename.str() << endl;
  std::ofstream paths(filename.str().c_str());
  if (!paths) {
    cout << "Can't open for writing paths file!" << endl;
    exit(1);
  }
#endif

  lg = &logfile;

  logfile.setStartTime(timeFromSec(0));

  NdpSinkLoggerSampling sinkLogger =
      NdpSinkLoggerSampling(timeFromMs(1), eventlist);
  logfile.addLogger(sinkLogger);
  NdpTrafficLogger traffic_logger = NdpTrafficLogger();
  logfile.addLogger(traffic_logger);
  NdpSrc::setMinRTO(50000);  // increase RTO to avoid spurious retransmits
  NdpSrc::setRouteStrategy(route_strategy);
  NdpSink::setRouteStrategy(route_strategy);

  NdpSrc* ndpSrc;
  NdpSink* ndpSnk;

  Route *fwd_route, *rev_route;
  double extrastarttime;

  // scanner interval must be less than min RTO
  NdpRtxTimerScanner ndpRtxScanner(timeFromUs((uint32_t)9), eventlist);

  int dest;

#if USE_FIRST_FIT
  if (subflow_count == 1) {
    ff = new FirstFit(timeFromMs(FIRST_FIT_INTERVAL), eventlist);
  }
#endif

#ifdef FAT_TREE
  FatTreeTopology* top = new FatTreeTopology(no_of_nodes, queuesize, &logfile,
                                             &eventlist, ff, COMPOSITE, 0);
#endif

#ifdef OV_FAT_TREE
  OversubscribedFatTreeTopology* top =
      new OversubscribedFatTreeTopology(&logfile, &eventlist, ff);
#endif

#ifdef MH_FAT_TREE
  MultihomedFatTreeTopology* top =
      new MultihomedFatTreeTopology(&logfile, &eventlist, ff);
#endif

#ifdef STAR
  StarTopology* top = new StarTopology(&logfile, &eventlist, ff);
#endif

#ifdef BCUBE
  BCubeTopology* top = new BCubeTopology(&logfile, &eventlist, ff);
  cout << "BCUBE " << K << endl;
#endif

#ifdef VL2
  VL2Topology* top = new VL2Topology(&logfile, &eventlist, ff);
#endif

  vector<const Route*>*** net_paths;
  net_paths = new vector<const Route*>**[no_of_nodes];

  int* is_dest = new int[no_of_nodes];

  for (int i = 0; i < no_of_nodes; i++) {
    is_dest[i] = 0;
    net_paths[i] = new vector<const Route*>*[no_of_nodes];
    for (int j = 0; j < no_of_nodes; j++) net_paths[i][j] = NULL;
  }

#ifdef USE_FIRST_FIT
  if (ff) ff->net_paths = net_paths;
#endif

  // used just to print out stats data at the end
  list<const Route*> routes;

  vector<Flow*> flows;
  map<int, NdpPullPacer*> dst2pacer;

  int ** flow_count = new int*[432];
  for (int i = 0; i < 432; i++) {
    flow_count[i] = new int[432];
    for (int j = 0; j < 432; j++) {
      flow_count[i][j] = 0;
    }
  }

  map<int, vector<int> > explicit_src_dst_pairs = {{0, {12}},
                                                   {24, {12, 36, 48}}};

  double end_time_ns = end_time * 1e9;
  // Uniform random
  // Left rack to right rack
  double mean_gap_ns = flowsize * 8 / ur_tput_gbps;
  // std::exponential_distribution<> rand_exp(1 / mean_gap_ns);
  for (auto iter = explicit_src_dst_pairs.begin();
       iter != explicit_src_dst_pairs.end(); iter++) {
    int src = iter->first;
    double cumulative_time_ns = 0;
    while (cumulative_time_ns < end_time_ns) {
      int num_dsts = iter->second.size();
      int dst = iter->second[rand() % num_dsts];
      // double gap_ns = rand_exp(rand_gen);
      double gap_ns = rand_exp(1 / mean_gap_ns);
      cumulative_time_ns += gap_ns;
      Flow* flow = new Flow();
      flow->name = std::to_string(src) + "_" + std::to_string(dst) + "_" +
                   std::to_string(flow_count[src][dst]++);
      flow->src = src;
      flow->dst = dst;
      flow->flowsize = flowsize;
      flow->start_time_ns = cumulative_time_ns;
      flows.push_back(flow);
      // cout << flow->name << " " << flow->start_time_ns << endl;
    }
    for (auto iter_ = iter->second.begin(); iter_ != iter->second.end();
         iter_++) {
      if (dst2pacer.find(*iter_) == dst2pacer.end()) {
        dst2pacer[*iter_] =
            new NdpPullPacer(eventlist, 20 /*pull pacer base rate is 10 Gbps*/);
      }
    }
  }

  cout << "Flow count: " << flows.size() << endl;

  for (int i = 0; i < flows.size(); i++) {
    Flow* flow = flows[i];
    NdpSrc* src = new NdpSrc(NULL, NULL, eventlist);
    src->setCwnd(cwnd * Packet::data_packet_size());
    src->set_flowsize(flow->flowsize);
    NdpSink* sink = new NdpSink(dst2pacer[flow->dst]);
    src->setName("src_" + flow->name);
    logfile.writeName(*src);
    sink->setName("sink_" + flow->name);
    logfile.writeName(*sink);
    ndpRtxScanner.registerNdp(*src);

    vector<const Route*>* fwd_paths = top->get_paths(flow->src, flow->dst);
    fwd_route = new Route(*(fwd_paths->at(0)));
    fwd_route->push_back(sink);
    for (unsigned int i = 0; i < fwd_paths->size(); i++) {
      routes.push_back((*fwd_paths)[i]);
    }

    vector<const Route*>* rev_paths = top->get_paths(flow->dst, flow->src);
    rev_route = new Route(*(rev_paths->at(0)));
    rev_route->push_back(src);

    src->connect(*fwd_route, *rev_route, *sink,
                 timeFromNs(flow->start_time_ns));
    src->set_paths(fwd_paths);
    sink->set_paths(rev_paths);
    src->set_traffic_logger(&traffic_logger);
    sinkLogger.monitorSink(sink);

    flow->src_id = src->get_id();
    flow->sink_id = sink->get_id();
  }

  // Record the setup
  int pktsize = Packet::data_packet_size();
  logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
  logfile.write("# subflows=" + ntoa(subflow_count));
  logfile.write("# hostnicrate = " + ntoa(HOST_NIC) + " pkt/sec");
  logfile.write("# corelinkrate = " + ntoa(HOST_NIC * CORE_TO_HOST) +
                " pkt/sec");
  double rtt = timeAsSec(timeFromUs(RTT));
  logfile.write("# rtt =" + ntoa(rtt));

  // GO!
  while (eventlist.doNextEvent()) {
  }

  cout << "Done" << endl;

  list<const Route*>::iterator rt_i;
  uint64_t counts[10];
  int hop;
  for (int i = 0; i < 10; i++) counts[i] = 0;
  for (rt_i = routes.begin(); rt_i != routes.end(); rt_i++) {
    const Route* r = (*rt_i);
    // print_route(*r);
#ifdef PRINTPATHS
    cout << "Path:" << endl;
#endif
    hop = 0;
    for (int i = 0; i < r->size(); i++) {
      PacketSink* ps = r->at(i);
      CompositeQueue* q = dynamic_cast<CompositeQueue*>(ps);
      if (q == 0) {
#ifdef PRINTPATHS
        cout << ps->nodename() << endl;
#endif
      } else {
#ifdef PRINTPATHS
        cout << q->nodename() << " id=" << q->id << " " << q->num_packets()
             << "pkts " << q->num_headers() << "hdrs " << q->num_acks()
             << "acks " << q->num_nacks() << "nacks " << q->num_stripped()
             << "stripped" << endl;
#endif
        counts[hop] += q->num_stripped();
        hop++;
      }
    }
#ifdef PRINTPATHS
    cout << endl;
#endif
  }
  for (int i = 0; i < 10; i++)
    cout << "Hop " << i << " Count " << counts[i] << endl;

  FILE* fd = fopen("flow_meta", "w");
  if (!fd) {
    cout << "Failed to write to flow_meta" << endl;
    return 1;
  }
  for (int i = 0; i < flows.size(); i++) {
    fprintf(fd, "%s,%d,%d,%.3lf\n", flows[i]->name.c_str(), flows[i]->src_id,
            flows[i]->sink_id, flows[i]->start_time_ns);
  }
  fclose(fd);
  return 0;
}

string ntoa(double n) {
  stringstream s;
  s << n;
  return s.str();
}

string itoa(uint64_t n) {
  stringstream s;
  s << n;
  return s.str();
}
