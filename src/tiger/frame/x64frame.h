#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include <map>

#include "tiger/frame/frame.h"

namespace frame {
class X64RegManager : public RegManager {
 public:
  X64RegManager();

  temp::Temp* GetRegister(int regno);

  /**
   * Get general-purpose registers except RSP
   * NOTE: returned temp list should be in the order of calling convention
   * @return general-purpose registers
   */
  temp::TempList* Registers();

  /**
   * Get registers which can be used to hold arguments
   * NOTE: returned temp list must be in the order of calling convention
   * @return argument registers
   */
  temp::TempList* ArgRegs();

  /**
   * Get caller-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return caller-saved registers
   */
  temp::TempList* CallerSaves();

  /**
   * Get callee-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return callee-saved registers
   */
  temp::TempList* CalleeSaves();

  /**
   * Get return-sink registers
   * @return return-sink registers
   */
  temp::TempList* ReturnSink();

  /**
   * Get word size
   */
  int WordSize();
  int Regnumber();

  temp::Temp* FramePointer();

  temp::Temp* StackPointer();

  temp::Temp* ReturnValue();

  temp::Temp* findByName(std::string name);

  temp::Map* getTempMap();

  temp::Map* temp_map_;

 private:
  std::map<std::string, temp::Temp*> str_map_;
};

class InFrameAccess : public Access {
 public:
  int offset;
  bool storePointer;
  explicit InFrameAccess(int offset) : offset(offset), storePointer(false) {}

// #ifdef GC
  void setStorePointer() { storePointer = true; }
// #endif

  tree::Exp* ToExp(tree::Exp* frame_ptr) {
    return new tree::MemExp(new tree::BinopExp(tree::PLUS_OP, frame_ptr,
                                               new tree::ConstExp(offset)));
  }
};

class InRegAccess : public Access {
 public:
  temp::Temp* reg;

  explicit InRegAccess(temp::Temp* reg) : reg(reg) {}
// #ifdef GC
  void setStorePointer() { reg->storePointer = true; }
// #endif
  tree::Exp* ToExp(tree::Exp* framePtr) { return new tree::TempExp(reg); }
};

class X64Frame : public Frame {
 public:
  X64Frame(temp::Label* lable, std::list<bool>* escapes_);

  std::list<frame::Access*>* Formals() { return formals_; }
  std::list<tree::Stm*>* ViewShift() { return view_shift; }
  Access* AllocLocal(bool escape_);
};

}  // namespace frame
#endif  // TIGER_COMPILER_X64FRAME_H
