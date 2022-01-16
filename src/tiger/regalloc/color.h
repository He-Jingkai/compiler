#ifndef TIGER_COMPILER_COLOR_H
#define TIGER_COMPILER_COLOR_H

#include <stack>

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/util/graph.h"

namespace col {
struct Result {
  Result() : coloring(nullptr), spills(nullptr) {}
  Result(temp::Map* coloring, live::INodeListPtr spills)
      : coloring(coloring), spills(spills) {}
  temp::Map* coloring;
  live::INodeListPtr spills;
};

class Color {
 public:
  Color(live::LiveGraph liveGrapg_)
      : liveGrapg(liveGrapg_),
        worklistMoves(liveGrapg_.moves),
        Coloring(new temp::Map()),
        spills(new live::INodeList()),
        alias(new tab::Table<live::INode, live::INode>()),
        degree(new tab::Table<live::INode, int>()),
        moveList(new tab::Table<live::INode, live::MoveList>()),
        spillWorkList(new live::INodeList()),
        simplifyWorklist(new live::INodeList()),
        freezeWorklist(new live::INodeList()),
        spillNodes(new live::INodeList()),
        activeMoves(new live::MoveList()),
        coalescedNodes(new live::INodeList()),
        coalescedMoves(new live::MoveList()),
        constrainedMoves(new live::MoveList()),
        frozenMoves(new live::MoveList()) {}
  void DoColor();
  Result TransferResult();

 private:
  live::LiveGraph liveGrapg;

  temp::Map* Coloring;

  live::INodeList* spills;

  tab::Table<live::INode, live::INode>* alias;

  tab::Table<live::INode, int>* degree;

  tab::Table<live::INode, live::MoveList>* moveList;

  /* high-degree nodes */
  live::INodeListPtr spillWorkList;

  /* low-degree non-move-related nodes */
  live::INodeListPtr simplifyWorklist;

  /* low-degree move-related nodes */
  live::INodeListPtr freezeWorklist;

  /* Nodes marked for spilling */
  live::INodeListPtr spillNodes;

  live::INodeListPtr coalescedNodes;

  /* Moves not yet ready for coalescing */
  live::MoveList* activeMoves;

  /* Moves enabled for coalescing */
  live::MoveList* worklistMoves;

  /* Moves has been coalesced */
  live::MoveList* coalescedMoves;

  /* Moves whose source and target intefere */
  live::MoveList* constrainedMoves;

  /* Moves that will no longer been considered for coalescing */
  live::MoveList* frozenMoves;

  /* Containing temporaries removed from the graph */
  std::stack<live::INodePtr> selectStack;

  std::vector<live::INodePtr> inStackNode;
  
  std::list<live::INodePtr> Adjacent(std::list<live::INodePtr> origon);

  void Build();

  void MakeWorkList();

  void Simplify();

  void Coalesce();

  void Freeze();

  void SelectSpill();

  void AssignColor();

  bool MoveRelated(live::INodePtr inodePtr);

  live::MoveList* NodeMoves(live::INodePtr inode);

  void DecrementDegree(live::INodePtr inode);

  void AddWorkList(live::INodePtr inode);

  void EnableMoves(live::INodeList* inodes);

  bool Precolored(live::INodePtr inode);

  void Combine(live::INodePtr u, live::INodePtr v);

  bool Briggs(live::INodePtr u, live::INodePtr v);

  bool George(live::INodePtr u, live::INodePtr v);

  void AddEdge(live::INodePtr u, live::INodePtr v);

  live::INodePtr GetAlias(live::INodePtr inode_ptr);

  void FreezeMoves(live::INodePtr inode);

};
}  // namespace col

#endif  // TIGER_COMPILER_COLOR_H
