#include "tiger/liveness/flowgraph.h"

extern frame::RegManager *reg_manager;

namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  std::list<assem::Instr *> ins_list = instr_list_->GetList();
  std::map<assem::Instr *, FNodePtr> ins_node_map;
  auto ins_list_iter = ins_list.begin();
  auto pre_list_iter = ins_list.end();

  while (ins_list_iter != ins_list.end()) {
    FNode *node = flowgraph_->NewNode(*ins_list_iter);
    ins_node_map[*ins_list_iter] = node;
    if (pre_list_iter != ins_list.end())
      flowgraph_->AddEdge(ins_node_map[*pre_list_iter], node);
    if (typeid(*(*ins_list_iter)) == typeid(assem::OperInstr)) {
      std::string ass = static_cast<assem::OperInstr *>(*ins_list_iter)->assem_;
      if (ass.find("jmp") != ass.npos) {
        // jmpq的下一指令和jmpq不构成flow，但cjmp的下一指令则和jmpq构成flow
        pre_list_iter = ins_list.end();
        ins_list_iter++;
        continue;
      }
    }

    if (typeid(*(*ins_list_iter)) == typeid(assem::LabelInstr))
      label_map_->Enter(
          static_cast<assem::LabelInstr *>(*ins_list_iter)->label_, node);

    pre_list_iter = ins_list_iter;
    ins_list_iter++;
  }

  //为jmp&cjmp指令补全flow
  for (ins_list_iter = ins_list.begin(); ins_list_iter != ins_list.end();
       ins_list_iter++)
    if (typeid(*(*ins_list_iter)) == typeid(assem::OperInstr)) {
      assem::OperInstr *jmp_ins =
          static_cast<assem::OperInstr *>(*ins_list_iter);
      if (jmp_ins->jumps_) {
        std::vector<temp::Label *> *target_list = jmp_ins->jumps_->labels_;
        temp::Label *label = (*target_list).front();
        flowgraph_->AddEdge(ins_node_map[*ins_list_iter],
                            label_map_->Look(label));
      }
    }
}

}  // namespace fg

namespace assem {

/* 修改codegen保证%rsp不放在dst_或src_中 */
temp::TempList *LabelInstr::Def() const { return new temp::TempList(); }

temp::TempList *MoveInstr::Def() const {
  return (dst_ == nullptr) ? new temp::TempList() : dst_;
}

temp::TempList *OperInstr::Def() const {
  return (dst_ == nullptr) ? new temp::TempList() : dst_;
}

temp::TempList *LabelInstr::Use() const { return new temp::TempList(); }

temp::TempList *MoveInstr::Use() const {
  return (src_ == nullptr) ? new temp::TempList() : src_;
}

temp::TempList *OperInstr::Use() const {
  return (src_ == nullptr) ? new temp::TempList() : src_;
}

}  // namespace assem
