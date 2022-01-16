#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "tiger/codegen/assem.h"
#include "tiger/translate/tree.h"

/* lab7 GC改动说明:
 * 增加 Access::setStorePointer()函数, 用于设置Access为存储指针
*/

namespace frame {

class RegManager {
 public:
  RegManager() {}

  temp::Temp *GetRegister(int regno) { return regs_[regno]; }

  /**
   * Get general-purpose registers except RSI
   * NOTE: returned temp list should be in the order of calling convention
   * @return general-purpose registers
   */
  [[nodiscard]] virtual temp::TempList *Registers() = 0;

  /**
   * Get registers which can be used to hold arguments
   * NOTE: returned temp list must be in the order of calling convention
   * @return argument registers
   */
  [[nodiscard]] virtual temp::TempList *ArgRegs() = 0;

  /**
   * Get caller-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return caller-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CallerSaves() = 0;

  /**
   * Get callee-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return callee-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CalleeSaves() = 0;

  /**
   * Get return-sink registers
   * @return return-sink registers
   */
  [[nodiscard]] virtual temp::TempList *ReturnSink() = 0;

  /**
   * Get word size
   */
  [[nodiscard]] virtual int WordSize() = 0;

  [[nodiscard]] virtual temp::Temp *FramePointer() = 0;

  [[nodiscard]] virtual temp::Temp *StackPointer() = 0;

  [[nodiscard]] virtual temp::Temp *ReturnValue() = 0;

  [[nodiscard]] virtual temp::Temp *findByName(std::string name) = 0;

  [[nodiscard]] virtual temp::Map *getTempMap() = 0;

  [[nodiscard]] virtual int Regnumber() = 0;

 protected:
  std::vector<temp::Temp *> regs_;
};

class Access {
 public:
  virtual ~Access() = default;
  virtual tree::Exp *ToExp(tree::Exp *framePtr) = 0;
// #ifdef GC
  virtual void setStorePointer() = 0;
// #endif
};

class Frame {
 public:
  temp::Label *lable_;
  std::list<frame::Access *> *formals_;
  std::list<frame::Access *> *locals_;
  std::list<tree::Stm *> *view_shift;
  int offset;
  int maxCallargs;

  Frame() = default;
  Frame(temp::Label *lable, std::list<bool> *escapes_){};
  virtual std::list<frame::Access *> *Formals() = 0;
  virtual Access *AllocLocal(bool escape_) = 0;
  virtual std::list<tree::Stm *> *ViewShift() = 0;
};

/**
 * Fragments
 */

class Frag {
 public:
  virtual ~Frag() = default;

  enum OutputPhase {
    Proc,
    String,
  };

  /**
   *Generate assembly for main program
   * @param out FILE object for output assembly file
   */
  virtual void OutputAssem(FILE *out, OutputPhase phase,
                           bool need_ra) const = 0;
};

class StringFrag : public Frag {
 public:
  temp::Label *label_;
  std::string str_;

  StringFrag(temp::Label *label, std::string str)
      : label_(label), str_(std::move(str)) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class ProcFrag : public Frag {
 public:
  tree::Stm *body_;
  Frame *frame_;

  ProcFrag(tree::Stm *body, Frame *frame) : body_(body), frame_(frame) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class Frags {
 public:
  Frags() = default;
  void PushBack(Frag *frag) { frags_.emplace_back(frag); }
  const std::list<Frag *> &GetList() { return frags_; }

 private:
  std::list<Frag *> frags_;
};

Frame *NewFrame(temp::Label *name, std::list<bool> formals);

tree::Stm *procEntryExit1(tree::Stm *stm_, frame::Frame *frame_);

void procEntryExit2(assem::InstrList &body);

assem::Proc *procEntryExit3(frame::Frame *frame_, assem::InstrList *body);

}  // namespace frame

#endif