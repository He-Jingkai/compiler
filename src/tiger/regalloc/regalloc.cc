#include "tiger/regalloc/regalloc.h"

#include <iostream>

#include "tiger/output/logger.h"

extern frame::RegManager* reg_manager;

namespace ra {
void printGraph(live::LiveGraph liveGrapg_) {
  for (auto node : liveGrapg_.interf_graph->Nodes()->GetList()) {
    std::cout << "t" << node->NodeInfo()->Int() << "'s succ:\n";
    for (auto succ : node->Succ()->GetList())
      std::cout << "t" << succ->NodeInfo()->Int() << ",";
    std::cout << "\n";
  }
  for (auto temp : reg_manager->Registers()->GetList()) {
    std::cout << "t" << temp->Int() << "is"
              << *(reg_manager->getTempMap()->Look(temp)) << std::endl;
  }
}
void RegAllocator::RegAlloc() {
  while (true) {
    fg::FlowGraphFactory* flowGraphFacPtr = new fg::FlowGraphFactory(il_);
    flowGraphFacPtr->AssemFlowGraph();
    fg::FGraphPtr fgraph = flowGraphFacPtr->GetFlowGraph();

    live::LiveGraphFactory* liveGraphFacPtr =
        new live::LiveGraphFactory(fgraph);
    liveGraphFacPtr->Liveness();
    live::LiveGraph liveGrapg_ = liveGraphFacPtr->GetLiveGraph();

    col::Color* color_ = new col::Color(liveGrapg_);
    color_->DoColor();
    col::Result colorResult = color_->TransferResult();

    if (colorResult.spills->GetList().size())
      rewriteProc(colorResult.spills);
    else {
      coloring_ = colorResult.coloring;
      break;
    }
  }
  removeUnecessaryMoves();
}

void RegAllocator::rewriteProc(live::INodeListPtr inodeListPtr) {
  std::list<live::INodePtr> spills = inodeListPtr->GetList();
  std::list<assem::Instr*> ins_list = il_->GetList();
  for (const live::INodePtr& inode_spill : spills) {
    //在frame中为spillTEMP分配空间
    int offset_ = frame_->offset;
    frame::Access* inStackAccess = frame_->AllocLocal(true);

    // For GC
    if (inode_spill->NodeInfo()->storePointer) inStackAccess->setStorePointer();


    //开始修改
    auto iter_ = ins_list.begin();
    while (iter_ != ins_list.end()) {
      temp::TempList* src_ = nullptr;
      temp::TempList* dst_ = nullptr;
      if (typeid(*(*iter_)) == typeid(assem::OperInstr)) {
        src_ = (static_cast<assem::OperInstr*>(*iter_))->src_;
        dst_ = (static_cast<assem::OperInstr*>(*iter_))->dst_;
      } else if (typeid(*(*iter_)) == typeid(assem::MoveInstr)) {
        src_ = (static_cast<assem::MoveInstr*>(*iter_))->src_;
        dst_ = (static_cast<assem::MoveInstr*>(*iter_))->dst_;
      } else {
        iter_++;
        continue;
      }

      if (src_) {
        temp::Temp* new_temp_reg = temp::TempFactory::NewTemp();
        // For GC
        if (inode_spill->NodeInfo()->storePointer)
          new_temp_reg->storePointer = true;

        bool replace = src_->replaceTempFromTempList(inode_spill->NodeInfo(),
                                                     new_temp_reg);

        //若需要替换
        if (replace) {
          std::string ass_load = "movq (" + frame_->lable_->Name() +
                                 "_framesize-" + std::to_string(-offset_) +
                                 ")(%rsp), `d0";  // get framepointer
          assem::OperInstr* ins_load = new assem::OperInstr(
              ass_load, new temp::TempList({new_temp_reg}), nullptr, nullptr);
          iter_ = ins_list.insert(iter_, ins_load);  // iter指向load
          iter_++;  // iter指向被改写指令
        }
      }
      if (dst_) {
        temp::Temp* new_temp_reg = temp::TempFactory::NewTemp();
        // For GC
        if (inode_spill->NodeInfo()->storePointer)
          new_temp_reg->storePointer = true;

        bool replace = dst_->replaceTempFromTempList(inode_spill->NodeInfo(),
                                                     new_temp_reg);
        //若需要替换
        if (replace) {
          std::string ass_store = "movq `s0, (" + frame_->lable_->Name() +
                                  "_framesize-" + std::to_string(-offset_) +
                                  ")(%rsp)";  // store
          assem::OperInstr* ins_store = new assem::OperInstr(
              ass_store, nullptr, new temp::TempList({new_temp_reg}), nullptr);
          iter_++;  // iter指向下一条指令
          iter_ = ins_list.insert(iter_, ins_store);  // iter指向store
        }
      }
      iter_++;  // iter_执行原列表中该指令的下一条指令
    }  //修改list结束
  }

  //将对list的操作同步到il_
  il_ = new assem::InstrList();
  for (auto ins_ : ins_list) il_->Append(ins_);
}

void RegAllocator::removeUnecessaryMoves() {
  //删除无用的move(src_ == dst_)
  std::list<assem::Instr*> ins_list = il_->GetList();
  auto iter_ = ins_list.begin();
  for (auto iter_ = ins_list.begin(); iter_ != ins_list.end(); iter_++)
    if (typeid(*(*iter_)) == typeid(assem::MoveInstr))
      if (equalMove(static_cast<assem::MoveInstr*>(*iter_))) {
        iter_ = ins_list.erase(iter_);
        continue;
      }

  il_ = new assem::InstrList();
  for (auto ins_ : ins_list) il_->Append(ins_);
}

bool RegAllocator::equalMove(assem::MoveInstr* move_ins) {
  temp::Temp* src = move_ins->src_->GetList().front();
  temp::Temp* dst = move_ins->dst_->GetList().front();
  return coloring_->Look(src) == coloring_->Look(dst);
}
}  // namespace ra
