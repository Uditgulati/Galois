/** Single source shortest paths -*- C++ -*-
 * @file
 * @section License
 *
 * Galois, a framework to exploit amorphous data-parallelism in irregular
 * programs.
 *
 * Copyright (C) 2012, The University of Texas at Austin. All rights reserved.
 * UNIVERSITY EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES CONCERNING THIS
 * SOFTWARE AND DOCUMENTATION, INCLUDING ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR ANY PARTICULAR PURPOSE, NON-INFRINGEMENT AND WARRANTIES OF
 * PERFORMANCE, AND ANY WARRANTY THAT MIGHT OTHERWISE ARISE FROM COURSE OF
 * DEALING OR USAGE OF TRADE.  NO WARRANTY IS EITHER EXPRESS OR IMPLIED WITH
 * RESPECT TO THE USE OF THE SOFTWARE OR DOCUMENTATION. Under no circumstances
 * shall University be liable for incidental, special, indirect, direct or
 * consequential damages or loss of profits, interruption of business, or
 * related expenses which may arise from use of Software or Documentation,
 * including but not limited to those resulting from defects in Software and/or
 * Documentation, or loss or inaccuracy of data of any kind.
 *
 * @section Description
 *
 * Single source shortest paths.
 *
 * @author Andrew Lenharth <andrewl@lenharth.org>
 */

#include "Galois/Statistic.h"
#include "Galois/Galois.h"
#include "Galois/Graphs/LCGraph.h"
#include "llvm/Support/CommandLine.h"
#include "Lonestar/BoilerPlate.h"

//! [Define LC Graph]
typedef Galois::Graph::LC_Linear_Graph<unsigned int, unsigned int> Graph;
//! [Define LC Graph]
typedef Graph::GraphNode GNode;
typedef std::pair<unsigned, GNode> UpdateRequest;

static const unsigned int DIST_INFINITY =
  std::numeric_limits<unsigned int>::max();

unsigned stepShift = 11;
Graph graph;

namespace cll = llvm::cl;
static cll::opt<std::string> filename(cll::Positional, cll::desc("<input file>"), cll::Required);

void relax_edge(unsigned src_data, Graph::edge_iterator ii, 
		Galois::UserContext<UpdateRequest>& ctx) {
  GNode dst = graph.getEdgeDst(ii);
    //![get edge and node data] 
  unsigned int edge_data = graph.getEdgeData(ii);
  unsigned& dst_data = graph.getData(dst);
    //![get edge and node data] 
  unsigned int newDist = src_data + edge_data;
  if (newDist < dst_data) {
    dst_data = newDist;
    ctx.push(std::make_pair(newDist, dst));
  }
}

//! [Operator in SSSPsimple]
struct SSSP {
  void operator()(UpdateRequest& req, Galois::UserContext<UpdateRequest>& ctx) const {
    GNode active_node = req.second;
    unsigned& data = graph.getData(active_node);
    if (req.first > data) return;
   
    //![loop over neighbors] 
    for (Graph::edge_iterator ii = graph.edge_begin(active_node),
           ee = graph.edge_end(active_node); ii != ee; ++ii)
      relax_edge(data, ii, ctx);
    //![loop over neighbors] 
  }
};
//! [Operator in SSSPsimple]

struct Init {
  void operator()(GNode& n, Galois::UserContext<GNode>& ctx) const {
    graph.getData(n) = DIST_INFINITY;
  }
};


int main(int argc, char **argv) {
  Galois::StatManager statManager;
  LonestarStart(argc, argv, 0,0,0);

//! [ReadGraph]
  Galois::Graph::readGraph(graph, filename);
//! [ReadGraph]

  Galois::for_each(graph.begin(), graph.end(), Init());

  //! [OrderedByIntegerMetic in SSSPsimple]
  struct UpdateRequestIndexer: public std::unary_function<UpdateRequest, unsigned int> {
    unsigned int operator() (const UpdateRequest& val) const {
      return val.first >> stepShift;
    }
  };
  using namespace Galois::WorkList;
  typedef dChunkedLIFO<16> dChunk;
  typedef OrderedByIntegerMetric<UpdateRequestIndexer,dChunk> OBIM;
//! [OrderedByIntegerMetic in SSSPsimple]

  Galois::StatTimer T;
  T.start();
  graph.getData(*graph.begin()) = 0;
  //! [for_each in SSSPsimple]
  UpdateRequest init[] = { std::make_pair(0U, *graph.begin()) };
  Galois::for_each(&init[0], &init[1], SSSP(), Galois::wl<OBIM>(), Galois::loopname("sssp_run_loop"));
  //! [for_each in SSSPsimple]
  T.stop();
  return 0;
}
