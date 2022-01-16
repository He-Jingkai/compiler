#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"

namespace ra {

class Result {
 public:
  temp::Map *coloring_;
  assem::InstrList *il_;
  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result() {}
};

class RegAllocator {
 public:
  RegAllocator(frame::Frame *frame__,
               std::unique_ptr<cg::AssemInstr> assemInstr__)
      : frame_(frame__), il_(assemInstr__.get()->GetInstrList()) {}
  void RegAlloc();
  std::unique_ptr<ra::Result> TransferResult() {
    std::unique_ptr<ra::Result> result =
        std::make_unique<ra::Result>(coloring_, il_);
    return std::move(result);
  }

 private:
  frame::Frame *frame_;
  temp::Map *coloring_;
  assem::InstrList *il_;
  /* 根据color的spillNodes重写Proc */
  void rewriteProc(live::INodeListPtr inodeListPtr);
  void removeUnecessaryMoves();
  bool equalMove(assem::MoveInstr *move_ins);
};

}  // namespace ra

#endif