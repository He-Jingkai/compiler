#include "tiger/output/output.h"

#include <cstdio>

#include "tiger/frame/x64frame.h"
#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;
extern frame::Frags *frags;
extern std::vector<gc::PointerMap> globalRoots;

namespace output {

/***************** For GC *****************/

void outPutPointerMap(FILE *out_) {
  //最后一个entry
  if (globalRoots.size()) globalRoots.back().nextPointerMapLable = "0";
  // Output pointerMap
  fprintf(out_, ".global GLOBAL_GC_ROOTS\n");
  fprintf(out_, ".data\n");
  fprintf(out_, "GLOBAL_GC_ROOTS:\n");
  for (gc::PointerMap map : globalRoots) {
    std::string lable = map.label + ":\n";
    std::string returnAddressLable = ".quad " + map.returnAddressLabel + "\n";
    std::string nextLable = ".quad " + map.nextPointerMapLable + "\n";
    std::string frameSize = ".quad " + map.frameSize + "\n";
    std::string isMain = ".quad " + map.isMain + "\n";
    std::string map_str =
        lable + returnAddressLable + nextLable + frameSize + isMain;
    for (std::string &offset : map.offsets) map_str += ".quad " + offset + "\n";
    map_str += ".quad " + map.endLabel + "\n";
    fprintf(out_, "%s", map_str.c_str());
  }
}

std::vector<int> getEscapePointers(frame::Frame *frame_) {
  /* 在寄存器分配之前放入frame中的全部变量均为escape,
   * 这些位置的生命周期直到函数结束, 保存这些变量用于分析pointer的活跃性 */
  std::vector<int> escapePointerOffsets;
  std::list<frame::Access *> *InframeFormals = frame_->Formals();
  for (frame::Access *access : *InframeFormals)
    if (typeid(*access) == typeid(frame::InFrameAccess) &&
        static_cast<frame::InFrameAccess *>(access)->storePointer)
      escapePointerOffsets.push_back(
          static_cast<frame::InFrameAccess *>(access)->offset);
  return escapePointerOffsets;
}

/*添加在寄存器分配之后, 产生pointerMap*/
assem::InstrList *emitPointerMap(assem::InstrList *il, frame::Frame *frame_,
                                 std::vector<int> escapePointerOffsets,
                                 temp::Map *color) {
  fg::FlowGraphFactory *flowGraphForGC = new fg::FlowGraphFactory(il);
  flowGraphForGC->AssemFlowGraph();
  fg::FGraphPtr fpForGC = flowGraphForGC->GetFlowGraph();
  gc::Roots *addressLiveForGC =
      new gc::Roots(il, frame_, fpForGC, escapePointerOffsets, color);
  std::vector<gc::PointerMap> newMaps = addressLiveForGC->GetPointerMaps();
  il = addressLiveForGC->GetInstrList();

  if (globalRoots.size() && newMaps.size())
    globalRoots.back().nextPointerMapLable = newMaps.front().label;
  globalRoots.insert(globalRoots.end(), newMaps.begin(), newMaps.end());
  return il;
}

/***************** End For GC *****************/

void AssemGen::GenAssem(bool need_ra) {
  frame::Frag::OutputPhase phase;
  // Output proc
  phase = frame::Frag::Proc;
  fprintf(out_, ".text\n");
  for (auto &&frag : frags->GetList()) frag->OutputAssem(out_, phase, need_ra);

  // Output string
  phase = frame::Frag::String;
  fprintf(out_, ".section .rodata\n");
  for (auto &&frag : frags->GetList()) frag->OutputAssem(out_, phase, need_ra);

  // output pointerMap
  outPutPointerMap(out_);
}

}  // namespace output

namespace frame {

void ProcFrag::OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const {
  std::unique_ptr<canon::Traces> traces;
  std::unique_ptr<cg::AssemInstr> assem_instr;
  std::unique_ptr<ra::Result> allocation;

  // When generating proc fragment, do not output string assembly
  if (phase != Proc) return;

  TigerLog("-------====IR tree=====-----\n");
  TigerLog(body_);

  {
    // Canonicalize
    TigerLog("-------====Canonicalize=====-----\n");
    canon::Canon canon(body_);

    // Linearize to generate canonical trees
    TigerLog("-------====Linearlize=====-----\n");
    tree::StmList *stm_linearized = canon.Linearize();
    TigerLog(stm_linearized);

    // Group list into basic blocks
    TigerLog("------====Basic block_=====-------\n");
    canon::StmListList *stm_lists = canon.BasicBlocks();
    TigerLog(stm_lists);

    // Order basic blocks into traces_
    TigerLog("-------====Trace=====-----\n");
    tree::StmList *stm_traces = canon.TraceSchedule();
    TigerLog(stm_traces);

    traces = canon.TransferTraces();
  }

  temp::Map *color =
      temp::Map::LayerMap(reg_manager->getTempMap(), temp::Map::Name());
  {
    // Lab 5: code generation
    TigerLog("-------====Code generate=====-----\n");
    cg::CodeGen code_gen(frame_, std::move(traces));
    code_gen.Codegen();
    assem_instr = code_gen.TransferAssemInstr();
    TigerLog(assem_instr.get(), color);
  }

  assem::InstrList *il = assem_instr.get()->GetInstrList();

  std::vector<int> escapePointerOffsets = output::getEscapePointers(frame_);

  if (need_ra) {
    // Lab 6: register allocation
    TigerLog("----====Register allocate====-----\n");
    ra::RegAllocator reg_allocator(frame_, std::move(assem_instr));
    reg_allocator.RegAlloc();
    allocation = reg_allocator.TransferResult();
    il = allocation->il_;
    color =
        temp::Map::LayerMap(reg_manager->getTempMap(), allocation->coloring_);
  }

  il = output::emitPointerMap(il, frame_, escapePointerOffsets, color);

  TigerLog("-------====Output assembly for %s=====-----\n",
           frame_->lable_->Name().data());

  assem::Proc *proc = frame::procEntryExit3(frame_, il);

  std::string proc_name = frame_->lable_->Name();

  fprintf(out, ".globl %s\n", proc_name.data());
  fprintf(out, ".type %s, @function\n", proc_name.data());
  // prologue
  fprintf(out, "%s", proc->prolog_.data());
  // body
  proc->body_->Print(out, color);
  // epilog_
  fprintf(out, "%s", proc->epilog_.data());
  fprintf(out, ".size %s, .-%s\n", proc_name.data(), proc_name.data());
}

void StringFrag::OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const {
  // When generating string fragment, do not output proc assembly
  if (phase != String) return;

  fprintf(out, "%s:\n", label_->Name().data());
  int length = static_cast<int>(str_.size());
  // It may contain zeros in the middle of string. To keep this work, we need
  // to print all the charactors instead of using fprintf(str)
  fprintf(out, ".long %d\n", length);
  fprintf(out, ".string \"");
  for (int i = 0; i < length; i++) {
    if (str_[i] == '\n') {
      fprintf(out, "\\n");
    } else if (str_[i] == '\t') {
      fprintf(out, "\\t");
    } else if (str_[i] == '\"') {
      fprintf(out, "\\\"");
    } else {
      fprintf(out, "%c", str_[i]);
    }
  }
  fprintf(out, "\"\n");
}
}  // namespace frame
