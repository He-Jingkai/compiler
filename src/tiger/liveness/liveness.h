#ifndef TIGER_LIVENESS_LIVENESS_H_
#define TIGER_LIVENESS_LIVENESS_H_

#include <map>

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/x64frame.h"
#include "tiger/liveness/flowgraph.h"
#include "tiger/util/graph.h"

namespace live {

using INode = graph::Node<temp::Temp>;
using INodePtr = graph::Node<temp::Temp> *;
using INodeList = graph::NodeList<temp::Temp>;
using INodeListPtr = graph::NodeList<temp::Temp> *;
using IGraph = graph::Graph<temp::Temp>;
using IGraphPtr = graph::Graph<temp::Temp> *;

class MoveList {
 public:
  MoveList() = default;

  [[nodiscard]] const std::list<std::pair<INodePtr, INodePtr>> &GetList()
      const {
    return move_list_;
  }
  void Append(INodePtr src, INodePtr dst) { move_list_.emplace_back(src, dst); }
  bool Contain(INodePtr src, INodePtr dst);
  void Delete(INodePtr src, INodePtr dst);
  void Prepend(INodePtr src, INodePtr dst) {
    move_list_.emplace_front(src, dst);
  }
  MoveList *Union(MoveList *list);
  MoveList *Intersect(MoveList *list);

 private:
  std::list<std::pair<INodePtr, INodePtr>> move_list_;
};

struct LiveGraph {
  IGraphPtr interf_graph;
  MoveList *moves;

  LiveGraph(IGraphPtr interf_graph, MoveList *moves)
      : interf_graph(interf_graph), moves(moves) {}
};

class LiveGraphFactory {
 public:
  explicit LiveGraphFactory(fg::FGraphPtr flowgraph)
      : flowgraph_(flowgraph),
        live_graph_(new IGraph(), new MoveList()),
        in_(std::make_unique<graph::Table<assem::Instr, temp::TempList>>()),
        out_(std::make_unique<graph::Table<assem::Instr, temp::TempList>>()),
        temp_node_map_(new tab::Table<temp::Temp, INode>()) {}
  void Liveness();
  LiveGraph GetLiveGraph() { return live_graph_; }
  tab::Table<temp::Temp, INode> *GetTempNodeMap() { return temp_node_map_; }
  void LiveMap();
  std::unique_ptr<graph::Table<assem::Instr, temp::TempList>> in_;
  std::unique_ptr<graph::Table<assem::Instr, temp::TempList>> out_;

 private:
  fg::FGraphPtr flowgraph_;
  LiveGraph live_graph_;

  tab::Table<temp::Temp, INode> *temp_node_map_;

  void InterfGraph();
};

bool Contain(temp::TempList *temp_list, temp::Temp *temp);

/* 并集 */
temp::TempList *Union(temp::TempList *list_1, temp::TempList *list_2);

/* 交集 */
temp::TempList *Intersect(temp::TempList *list_1, temp::TempList *list_2);

/* 差集，将minute中不包含在minus中的元素返回 */
temp::TempList *Difference(temp::TempList *minute, temp::TempList *minus);

bool Changed(temp::TempList *prev, temp::TempList *curr);
}  // namespace live

#endif