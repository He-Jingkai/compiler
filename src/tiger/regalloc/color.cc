#include "tiger/regalloc/color.h"

#include <iostream>

extern frame::RegManager* reg_manager;

/* 寄存器分配注意事项:
  (1) call的def是全部caller-save-register, use是**实际使用的**寄存器
  (2) ret的use是ret-register和全部callee-save-register
  (3) coalesce时首先删除存在冲突的move边
  (4) 冲突图建立规则: 非move的def和out之间连边, move的def和out-use之间连边
  (5) in = out - def + use
*/

namespace col {

void Color::DoColor() {
  Build();
  MakeWorkList();
  while (true) {
    if (simplifyWorklist->GetList().size())
      Simplify();
    else if (worklistMoves->GetList().size())
      Coalesce();
    else if (freezeWorklist->GetList().size())
      Freeze();
    else if (spillWorkList->GetList().size())
      SelectSpill();
    if (!simplifyWorklist->GetList().size() &&
        !worklistMoves->GetList().size() && !freezeWorklist->GetList().size() &&
        !spillWorkList->GetList().size())
      break;
  }
  AssignColor();
}

/* 初始化degree, alias, moveList, 添加precolor */
void Color::Build() {
  std::list<live::INodePtr> inodes = liveGrapg.interf_graph->Nodes()->GetList();
  std::list<std::pair<live::INodePtr, live::INodePtr>> move_list =
      liveGrapg.moves->GetList();
  for (const live::INodePtr inode : inodes) {
    degree->Enter(inode, new int(inode->OutDegree()));
    alias->Enter(inode, inode);

    live::MoveList* inode_moveList = new live::MoveList();
    for (const std::pair<live::INodePtr, live::INodePtr>& move : move_list)
      if (move.first == inode || move.second == inode)
        inode_moveList->Append(move.first, move.second);
    moveList->Enter(inode, inode_moveList);
  }

  for (temp::Temp* temp_ : reg_manager->Registers()->GetList())
    Coloring->Enter(temp_, reg_manager->getTempMap()->Look(temp_));
}

/* 初始化spillWorkList, freezeWorklist, simplifyWorklist */
void Color::MakeWorkList() {
  std::list<live::INodePtr> inodes = liveGrapg.interf_graph->Nodes()->GetList();
  for (const live::INodePtr inode : inodes) {
    if (Precolored(inode)) continue;
    if (*(degree->Look(inode)) >= reg_manager->Regnumber())
      spillWorkList->Append(inode);
    else if (MoveRelated(inode))
      freezeWorklist->Append(inode);
    else
      simplifyWorklist->Append(inode);
  }
}

bool Color::MoveRelated(live::INodePtr inodePtr) {
  return NodeMoves(inodePtr)->GetList().size() != 0;
}

/* 返回仍可以Coalesce的moves */
live::MoveList* Color::NodeMoves(live::INodePtr inode) {
  live::MoveList* inode_moveList = moveList->Look(inode);
  return inode_moveList->Intersect(activeMoves->Union(worklistMoves));
}

void Color::Simplify() {
  live::INodePtr inode_to_be_simplified = simplifyWorklist->GetList().back();
  simplifyWorklist->DeleteNode(inode_to_be_simplified);
  selectStack.push(inode_to_be_simplified);
  inStackNode.push_back(inode_to_be_simplified);
  std::list<live::INodePtr> succ =
      Adjacent(inode_to_be_simplified->Succ()->GetList());  //在图中的每一个节点
  for (live::INodePtr inode_succ : succ) DecrementDegree(inode_succ);
}

void Color::DecrementDegree(live::INodePtr inode) {
  int pre_degree = *(degree->Look(inode));
  *(degree->Look(inode)) = pre_degree - 1;
  if (pre_degree == (reg_manager->Regnumber()) && !Precolored(inode)) {
    live::INodeListPtr adjcent_ = inode->Succ();
    live::INodeListPtr adjcent_and_inode = new live::INodeList();
    for (const live::INodePtr inode_ptr : Adjacent(adjcent_->GetList()))
      adjcent_and_inode->Append(inode_ptr);
    adjcent_and_inode->Append(inode);
    EnableMoves(adjcent_and_inode);
    // 查看activeMoveList中有无可以移除的move

    spillWorkList->DeleteNode(inode);
    if (MoveRelated(inode))
      freezeWorklist->Append(inode);
    else
      simplifyWorklist->Append(inode);
  }
}

/* 由于状态改变, 之前不满足合并条件的move可能已经满足合并条件
  将其移入worklistMoves重新判断 */
void Color::EnableMoves(live::INodeList* inodes) {
  std::list<live::INodePtr> inodes_list = inodes->GetList();
  for (const live::INodePtr& inode_ptr : inodes_list) {
    std::list<std::pair<live::INodePtr, live::INodePtr>> moveNodes =
        NodeMoves(inode_ptr)->GetList();
    for (const std::pair<live::INodePtr, live::INodePtr>& move : moveNodes)
      if (activeMoves->Contain(move.first, move.second)) {
        activeMoves->Delete(move.first, move.second);
        worklistMoves->Append(move.first, move.second);
      }
  }
}

void Color::Coalesce() {
  std::pair<live::INodePtr, live::INodePtr> move =
      worklistMoves->GetList().back();
  worklistMoves->Delete(move.first, move.second);
  const live::INodePtr x = GetAlias(move.first);
  const live::INodePtr y = GetAlias(move.second);
  live::INodePtr u = x, v = y;
  if (Precolored(move.second)) {
    u = y;
    v = x;
  }
  if (u == v) {
    coalescedMoves->Append(move.first, move.second);
    AddWorkList(u);
  } else if (Precolored(v) || v->Succ()->Contain(u)) {
    constrainedMoves->Append(move.first, move.second);
    AddWorkList(u);
    AddWorkList(v);
  } else if ((Precolored(u) && George(u, v)) ||
             (!Precolored(u) && Briggs(u, v))) {
    coalescedMoves->Append(move.first, move.second);
    Combine(u, v);
    AddWorkList(u);
  } else
    activeMoves->Append(move.first, move.second);
}

/* assignColor前Color中只有Precolor */
bool Color::Precolored(live::INodePtr inode) {
  return Coloring->Look(inode->NodeInfo()) != nullptr;
}

void Color::AddWorkList(live::INodePtr inode) {
  if (!Precolored(inode) && !MoveRelated(inode) &&
      *(degree->Look(inode)) < reg_manager->Regnumber()) {
    freezeWorklist->DeleteNode(inode);
    simplifyWorklist->Append(inode);
  }
}

bool Color::Briggs(live::INodePtr u, live::INodePtr v) {
  int moreThanK = 0;
  std::list<live::INodePtr> uSuccUnionvSucc =
      Adjacent(u->Succ()->Union(v->Succ())->GetList());
  for (const live::INodePtr& uSucc_inode : uSuccUnionvSucc)
    if (*(degree->Look(uSucc_inode)) >= reg_manager->Regnumber()) moreThanK++;
  return moreThanK < reg_manager->Regnumber();
}

bool Color::George(live::INodePtr u, live::INodePtr v) {
  std::list<live::INodePtr> adjnodes_ = Adjacent(v->Succ()->GetList());
  for (live::INodePtr inode : adjnodes_) {
    bool degreeLessThanK = *(degree->Look(inode)) < reg_manager->Regnumber();
    bool precolored = Precolored(inode);
    bool adjWithu = u->Succ()->Contain(inode);
    if (!(degreeLessThanK || precolored || adjWithu)) return false;
  }
  return true;
}

void Color::Combine(live::INodePtr u, live::INodePtr v) {
  if (freezeWorklist->Contain(v))
    freezeWorklist->DeleteNode(v);
  else
    spillWorkList->DeleteNode(v);

  coalescedNodes->Append(v);
  alias->Set(v, u);
  moveList->Set(u, moveList->Look(u)->Union(moveList->Look(v)));
  live::INodeListPtr temp = new live::INodeList();
  temp->Append(v);
  EnableMoves(temp);
  for (const live::INodePtr& inode_ptr : Adjacent(v->Succ()->GetList())) {
    AddEdge(inode_ptr, u);
    DecrementDegree(inode_ptr);
  }
  if (*(degree->Look(u)) >= reg_manager->Regnumber() &&
      freezeWorklist->Contain(u)) {
    spillWorkList->Append(u);
    freezeWorklist->DeleteNode(u);
  }
}

live::INodePtr Color::GetAlias(live::INodePtr inode_ptr) {
  if (coalescedNodes->Contain(inode_ptr))
    return GetAlias(alias->Look(inode_ptr));
  return inode_ptr;
}

void Color::AddEdge(live::INodePtr u, live::INodePtr v) {
  if (!u->Succ()->Contain(v) && u != v) {
    if (!Precolored(u)) {
      liveGrapg.interf_graph->AddEdge(u, v);
      *(degree->Look(u)) = *(degree->Look(u)) + 1;
    }
    if (!Precolored(v)) {
      liveGrapg.interf_graph->AddEdge(v, u);
      *(degree->Look(v)) = *(degree->Look(v)) + 1;
    }
  }
}

void Color::Freeze() {
  live::INodePtr inode = freezeWorklist->GetList().back();
  freezeWorklist->DeleteNode(inode);
  simplifyWorklist->Append(inode);
  FreezeMoves(inode);
}

void Color::FreezeMoves(live::INodePtr inode_u) {
  std::list<std::pair<live::INodePtr, live::INodePtr>> nodeMoves_u =
      NodeMoves(inode_u)->GetList();
  for (std::pair<live::INodePtr, live::INodePtr>& move_m : nodeMoves_u) {
    const live::INodePtr inode_x = move_m.first;
    const live::INodePtr inode_y = move_m.second;
    live::INodePtr inode_v = nullptr;
    if (GetAlias(inode_y) == GetAlias(inode_u))
      inode_v = GetAlias(inode_x);
    else
      inode_v = GetAlias(inode_y);
    activeMoves->Delete(inode_x, inode_y);

    frozenMoves->Append(inode_x, inode_y);

    if (!NodeMoves(inode_v)->GetList().size() &&
        *(degree->Look(inode_v)) < reg_manager->Regnumber()) {
      freezeWorklist->DeleteNode(inode_v);
      simplifyWorklist->Append(inode_v);
    }
  }
}

void Color::SelectSpill() {
  std::list<live::INodePtr> inodes = spillWorkList->GetList();
  live::INodePtr inode;
  int bigistSucc = 0;
  for (live::INodePtr spillWorkListNode : inodes)
    if (spillWorkListNode->Succ()->GetList().size() > bigistSucc) {
      inode = spillWorkListNode;
      bigistSucc = spillWorkListNode->Succ()->GetList().size();
    }
  spillWorkList->DeleteNode(inode);
  simplifyWorklist->Append(inode);
  FreezeMoves(inode);
}

void Color::AssignColor() {
  while (!selectStack.empty()) {
    live::INodePtr inode_tocolor = selectStack.top();
    selectStack.pop();
    std::list<temp::Temp*> regs = reg_manager->Registers()->GetList();
    std::list<std::string*> okColor;
    for (temp::Temp* oktemp : regs)
      okColor.push_back(reg_manager->getTempMap()->Look(oktemp));

    std::list<live::INodePtr> adj = inode_tocolor->Succ()->GetList();
    for (const live::INodePtr& inode_w : adj) {
      std::string* neibor_color = Coloring->Look(GetAlias(inode_w)->NodeInfo());
      if (neibor_color) okColor.remove(neibor_color);
    }

    if (okColor.empty())
      spillNodes->Append(inode_tocolor);
    else
      Coloring->Enter(inode_tocolor->NodeInfo(), okColor.front());
  }
  /* RSP */
  Coloring->Enter(reg_manager->StackPointer(), new std::string("%rsp"));
  /* 为Coalesed的寄存器分配 */
  std::list<live::INodePtr> coalescedNodes_list = coalescedNodes->GetList();
  for (live::INodePtr coalescedNode : coalescedNodes_list)
    Coloring->Enter(coalescedNode->NodeInfo(),
                    Coloring->Look(GetAlias(coalescedNode)->NodeInfo()));
}

Result Color::TransferResult() {
  Result result;
  result.spills = spillNodes;
  result.coloring = Coloring;
  return result;
}

/* 过滤出目前仍在图中(没有symplify和coalesce)的临近节点 */
std::list<live::INodePtr> Color::Adjacent(std::list<live::INodePtr> origon) {
  std::list<live::INodePtr> ret;
  std::list<live::INodePtr> coalesced_list = coalescedNodes->GetList();
  for (auto node : origon) {
    if (std::find(coalesced_list.begin(), coalesced_list.end(), node) !=
        coalesced_list.end())
      continue;
    if (std::find(inStackNode.begin(), inStackNode.end(), node) !=
        inStackNode.end())
      continue;
    ret.push_back(node);
  }
  return ret;
}

}  // namespace col
