#include "tiger/liveness/liveness.h"

#include <iostream>

extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move1 : move_list_) res->move_list_.push_back(move1);
  for (auto move : list->GetList())
    if (!Contain(move.first, move.second)) res->move_list_.push_back(move);

  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second)) res->move_list_.push_back(move);
  }
  return res;
}

/* 并集 */
temp::TempList *Union(temp::TempList *list_1, temp::TempList *list_2) {
  temp::TempList *res = new temp::TempList();
  std::list<temp::Temp *> list_1_list = list_1->GetList();
  for (auto temp_1 : list_1_list) res->Append(temp_1);
  std::list<temp::Temp *> list_2_list = list_2->GetList();
  for (auto temp_ : list_2_list)
    if (std::find(list_1_list.begin(), list_1_list.end(), temp_) ==
        list_1_list.end())
      res->Append(temp_);
  return res;
}

/* 交集 */
temp::TempList *Intersect(temp::TempList *list_1, temp::TempList *list_2) {
  temp::TempList *res = new temp::TempList();
  std::list<temp::Temp *> list_2_list = list_2->GetList();
  std::list<temp::Temp *> list_1_list = list_1->GetList();
  for (auto temp_ : list_2_list)
    if (std::find(list_1_list.begin(), list_1_list.end(), temp_) !=
        list_1_list.end())
      res->Append(temp_);
  return res;
}

/* 差集，将minute中不包含在minus中的元素返回 */
temp::TempList *Difference(temp::TempList *minute, temp::TempList *minus) {
  temp::TempList *res = new temp::TempList();
  std::list<temp::Temp *> minute_list = minute->GetList();
  std::list<temp::Temp *> minus_list = minus->GetList();
  for (auto temp_ : minute_list)
    if (std::find(minus_list.begin(), minus_list.end(), temp_) ==
        minus_list.end())
      res->Append(temp_);
  return res;
}

bool Changed(temp::TempList *prev, temp::TempList *curr) {
  std::list<temp::Temp *> prev_tempList = prev->GetList();
  std::list<temp::Temp *> curr_tempList = curr->GetList();
  if (prev_tempList.size() != curr_tempList.size()) return true;
  for (const temp::Temp *prev_temp : prev_tempList)
    if (std::find(curr_tempList.begin(), curr_tempList.end(), prev_temp) ==
        curr_tempList.end())
      return true;
  return false;
}

void LiveGraphFactory::LiveMap() {
  std::list<fg::FNodePtr> flowgrapg_nodes = flowgraph_->Nodes()->GetList();
  std::map<fg::FNodePtr, temp::TempList *> in, pre_in, out, pre_out;

  // initialize in and out
  for (fg::FNodePtr node_ : flowgrapg_nodes) {
    in[node_] = new temp::TempList();
    out[node_] = new temp::TempList();
  }

  bool changed = false;
  do {
    // for(auto pair_in : pre_in)delete pair_in.second;
    // for(auto pair_out : pre_out)delete pair_out.second;
    pre_in = in;
    pre_out = out;
    changed = false;
    for (fg::FNodePtr node_ : flowgrapg_nodes) {
      assem::Instr *node_ins = node_->NodeInfo();
      temp::TempList *use_list = node_ins->Use();
      temp::TempList *def_list = node_ins->Def();

      out[node_] = new temp::TempList();
      std::list<fg::FNodePtr> succ = node_->Succ()->GetList();
      for (fg::FNodePtr succ_node : succ) {
        temp::TempList *old_out = out[node_];
        out[node_] = Union(old_out, in[succ_node]);
        delete old_out;
      }
      in[node_] = Union(Difference(out[node_], def_list), use_list);
      // if(!use_list->GetList().size())delete use_list;
      // if(!def_list->GetList().size())delete def_list;
      //判断该点的in和out有无变化
      if (Changed(in[node_], pre_in[node_]) ||
          Changed(out[node_], pre_out[node_]))
        changed = true;
    }
  } while (changed);

  for (auto in_pair : in) in_->Enter(in_pair.first, in_pair.second);
  for (auto out_pair : out) out_->Enter(out_pair.first, out_pair.second);
}

void LiveGraphFactory::InterfGraph() {
  /* STEP1: 将全部temp加入table */
  const std::list<temp::Temp *> precolored_regs =
      reg_manager->Registers()->GetList();
  for (temp::Temp *temp_ : precolored_regs) {
    INodePtr inode_ptr = live_graph_.interf_graph->NewNode(temp_);
    temp_node_map_->Enter(temp_, inode_ptr);
  }

  const std::list<fg::FNodePtr> flowgrapg_nodes =
      flowgraph_->Nodes()->GetList();
  for (fg::FNodePtr node_ : flowgrapg_nodes) {
    assem::Instr *node_ins = node_->NodeInfo();
    std::list<temp::Temp *> out_list = out_->Look(node_)->GetList();
    std::list<temp::Temp *> def_list = node_ins->Def()->GetList();
    for (temp::Temp *temp_ : out_list) {
      INodePtr inode_ptr = temp_node_map_->Look(temp_);
      if (!inode_ptr) {
        INodePtr inode_ptr = live_graph_.interf_graph->NewNode(temp_);
        temp_node_map_->Enter(temp_, inode_ptr);
      }
    }
    for (temp::Temp *temp_ : def_list) {
      INodePtr inode_ptr = temp_node_map_->Look(temp_);
      if (!inode_ptr) {
        INodePtr inode_ptr = live_graph_.interf_graph->NewNode(temp_);
        temp_node_map_->Enter(temp_, inode_ptr);
      }
    }
  }

  /* STEP2: precolored构成完全图 */
  for (temp::Temp *temp_from : precolored_regs)
    for (temp::Temp *temp_to : precolored_regs)
      if (temp_from != temp_to)
        live_graph_.interf_graph->AddEdge(temp_node_map_->Look(temp_from),
                                          temp_node_map_->Look(temp_to));

  /* STEP3:
   * For an instruction that defines a variable a, where the live-out variables
   * are b1, …, bj, the way to add interference edges for it is
   * - If it is a nonmove instruction, add (a, b1), …, (a, bj)
   * - If it is a move instruction a ← c, add (a, b1), …, (a, bj), for any bj
   * that is not the same as c */
  for (fg::FNodePtr node_ : flowgrapg_nodes) {
    assem::Instr *node_ins = node_->NodeInfo();
    const std::list<temp::Temp *> out_list = out_->Look(node_)->GetList();
    const std::list<temp::Temp *> def_list = node_ins->Def()->GetList();
    if (typeid(*node_ins) == typeid(assem::MoveInstr)) {
      const std::list<temp::Temp *> out_dif_use_list =
          Difference(out_->Look(node_), node_ins->Use())->GetList();
      const std::list<temp::Temp *> use_list = node_ins->Use()->GetList();

      for (temp::Temp *temp_def : def_list)
        for (temp::Temp *temp_out_dif_use : out_dif_use_list)
          if (temp_def != temp_out_dif_use) {
            INodePtr inode_from = temp_node_map_->Look(temp_def);
            INodePtr inode_to = temp_node_map_->Look(temp_out_dif_use);
            if (inode_from->Succ()->Contain(inode_to)) continue;
            live_graph_.interf_graph->AddEdge(inode_from, inode_to);
            live_graph_.interf_graph->AddEdge(inode_to, inode_from);
          }

      for (temp::Temp *use_temp : use_list)
        for (temp::Temp *def_temp : def_list)
          if (use_temp != def_temp) {
            INodePtr inode_use = temp_node_map_->Look(use_temp);
            INodePtr inode_def = temp_node_map_->Look(def_temp);
            if (!live_graph_.moves->Contain(inode_use, inode_def) &&
                !live_graph_.moves->Contain(inode_def, inode_use))
              live_graph_.moves->Append(inode_use, inode_def);
          }
    }  // end for instr's is move
    else
      for (temp::Temp *temp_def : def_list)
        for (temp::Temp *temp_out : out_list)
          if (temp_def != temp_out) {
            INodePtr inode_from = temp_node_map_->Look(temp_def);
            INodePtr inode_to = temp_node_map_->Look(temp_out);
            if (inode_from->Succ()->Contain(inode_to)) continue;
            live_graph_.interf_graph->AddEdge(inode_from, inode_to);
            live_graph_.interf_graph->AddEdge(inode_to, inode_from);
          }  // end for instr's not move
  }          // end for instr's
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

}  // namespace live