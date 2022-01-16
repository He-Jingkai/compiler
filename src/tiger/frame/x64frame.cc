#include "tiger/frame/x64frame.h"

extern frame::RegManager* reg_manager;

namespace frame {
/******************** X64RegManager Functions ********************/

X64RegManager::X64RegManager() : temp_map_(temp::Map::Empty()) {
  temp::Temp* rax = temp::TempFactory::NewTemp();
  temp::Temp* rdi = temp::TempFactory::NewTemp();
  temp::Temp* rsi = temp::TempFactory::NewTemp();
  temp::Temp* rdx = temp::TempFactory::NewTemp();
  temp::Temp* rcx = temp::TempFactory::NewTemp();
  temp::Temp* rbx = temp::TempFactory::NewTemp();
  temp::Temp* rbp = temp::TempFactory::NewTemp();
  temp::Temp* rsp = temp::TempFactory::NewTemp();
  temp::Temp* r8 = temp::TempFactory::NewTemp();
  temp::Temp* r9 = temp::TempFactory::NewTemp();
  temp::Temp* r10 = temp::TempFactory::NewTemp();
  temp::Temp* r11 = temp::TempFactory::NewTemp();
  temp::Temp* r12 = temp::TempFactory::NewTemp();
  temp::Temp* r13 = temp::TempFactory::NewTemp();
  temp::Temp* r14 = temp::TempFactory::NewTemp();
  temp::Temp* r15 = temp::TempFactory::NewTemp();
  std::string rax_str = "%rax";
  std::string rdi_str = "%rdi";
  std::string rsi_str = "%rsi";
  std::string rdx_str = "%rdx";
  std::string rcx_str = "%rcx";
  std::string rbx_str = "%rbx";
  std::string rbp_str = "%rbp";
  std::string rsp_str = "%rsp";
  std::string r8_str = "%r8";
  std::string r9_str = "%r9";
  std::string r10_str = "%r10";
  std::string r11_str = "%r11";
  std::string r12_str = "%r12";
  std::string r13_str = "%r13";
  std::string r14_str = "%r14";
  std::string r15_str = "%r15";
  regs_.push_back(rax);
  regs_.push_back(rbx);
  regs_.push_back(rcx);
  regs_.push_back(rdx);
  regs_.push_back(rsi);
  regs_.push_back(rdi);
  regs_.push_back(rbp);
  regs_.push_back(rsp);
  regs_.push_back(r8);
  regs_.push_back(r9);
  regs_.push_back(r10);
  regs_.push_back(r11);
  regs_.push_back(r12);
  regs_.push_back(r13);
  regs_.push_back(r14);
  regs_.push_back(r15);
  str_map_[rax_str] = rax;
  str_map_[rdi_str] = rdi;
  str_map_[rsi_str] = rsi;
  str_map_[rdx_str] = rdx;
  str_map_[rcx_str] = rcx;
  str_map_[rbx_str] = rbx;
  str_map_[rbp_str] = rbp;
  str_map_[rsp_str] = rsp;
  str_map_[r8_str] = r8;
  str_map_[r9_str] = r9;
  str_map_[r10_str] = r10;
  str_map_[r11_str] = r11;
  str_map_[r12_str] = r12;
  str_map_[r13_str] = r13;
  str_map_[r14_str] = r14;
  str_map_[r15_str] = r15;

  temp_map_->Enter(rax, new std::string("%rax"));
  temp_map_->Enter(rdi, new std::string("%rdi"));
  temp_map_->Enter(rsi, new std::string("%rsi"));
  temp_map_->Enter(rdx, new std::string("%rdx"));
  temp_map_->Enter(rcx, new std::string("%rcx"));
  temp_map_->Enter(rbx, new std::string("%rbx"));
  temp_map_->Enter(rbp, new std::string("%rbp"));
  temp_map_->Enter(rsp, new std::string("%rsp"));

  temp_map_->Enter(r8, new std::string("%r8"));
  temp_map_->Enter(r9, new std::string("%r9"));
  temp_map_->Enter(r10, new std::string("%r10"));
  temp_map_->Enter(r11, new std::string("%r11"));
  temp_map_->Enter(r12, new std::string("%r12"));
  temp_map_->Enter(r13, new std::string("%r13"));
  temp_map_->Enter(r14, new std::string("%r14"));
  temp_map_->Enter(r15, new std::string("%r15"));
}

temp::Temp* X64RegManager::GetRegister(int regno) { return regs_[regno]; }

temp::TempList* X64RegManager::Registers() {
  temp::TempList* tempList = new temp::TempList();
  tempList->Append(str_map_["%rax"]);
  tempList->Append(str_map_["%rdi"]);
  tempList->Append(str_map_["%rsi"]);
  tempList->Append(str_map_["%rdx"]);
  tempList->Append(str_map_["%rcx"]);
  tempList->Append(str_map_["%rbx"]);
  tempList->Append(str_map_["%rbp"]);

  tempList->Append(str_map_["%r8"]);
  tempList->Append(str_map_["%r9"]);
  tempList->Append(str_map_["%r10"]);
  tempList->Append(str_map_["%r11"]);
  tempList->Append(str_map_["%r12"]);
  tempList->Append(str_map_["%r13"]);
  tempList->Append(str_map_["%r14"]);
  tempList->Append(str_map_["%r15"]);
  return tempList;
}

temp::TempList* X64RegManager::ArgRegs() {
  temp::TempList* tempList = new temp::TempList();
  tempList->Append(str_map_["%rdi"]);
  tempList->Append(str_map_["%rsi"]);
  tempList->Append(str_map_["%rdx"]);
  tempList->Append(str_map_["%rcx"]);
  tempList->Append(str_map_["%r8"]);
  tempList->Append(str_map_["%r9"]);
  return tempList;
}

temp::TempList* X64RegManager::CallerSaves() {
  temp::TempList* tempList = new temp::TempList();
  tempList->Append(str_map_["%rax"]);
  tempList->Append(str_map_["%rdi"]);
  tempList->Append(str_map_["%rsi"]);
  tempList->Append(str_map_["%rdx"]);
  tempList->Append(str_map_["%rcx"]);
  tempList->Append(str_map_["%r8"]);
  tempList->Append(str_map_["%r9"]);
  tempList->Append(str_map_["%r10"]);
  tempList->Append(str_map_["%r11"]);
  return tempList;
}

temp::TempList* X64RegManager::CalleeSaves() {
  temp::TempList* tempList = new temp::TempList();
  tempList->Append(str_map_["%rbx"]);
  tempList->Append(str_map_["%rbp"]);
  tempList->Append(str_map_["%r12"]);
  tempList->Append(str_map_["%r13"]);
  tempList->Append(str_map_["%r14"]);
  tempList->Append(str_map_["%r15"]);
  return tempList;
}

temp::TempList* X64RegManager::ReturnSink() {
  temp::TempList* tempList = new temp::TempList();
  tempList->Append(str_map_["%rax"]);
  return tempList;
}

int X64RegManager::WordSize() { return 8; }
int X64RegManager::Regnumber() { return 15; }

temp::Temp* X64RegManager::FramePointer() { return str_map_["%rbp"]; }

temp::Temp* X64RegManager::StackPointer() { return str_map_["%rsp"]; }

temp::Temp* X64RegManager::ReturnValue() { return str_map_["%rax"]; }

temp::Temp* X64RegManager::findByName(std::string name) {
  return str_map_[name];
}

temp::Map* X64RegManager::getTempMap() { return temp_map_; }

/******************** X64Frame Functions ********************/

X64Frame::X64Frame(temp::Label* lable, std::list<bool>* escapes_) {
  lable_ = lable;
  formals_ = new std::list<frame::Access*>();
  view_shift = new std::list<tree::Stm*>();
  maxCallargs = 0;

  this->offset = -reg_manager->WordSize();  // the first offset is frame_pointer's address -8
  int index = 1;
  for (const bool escape : *escapes_) {
    if (escape) { // escape variables
      if (index <= 6) {  // register parameters, 存入栈中
        formals_->push_back(new InFrameAccess(offset));
        view_shift->push_back(new tree::MoveStm(
            new tree::MemExp(new tree::BinopExp(
                tree::PLUS_OP, new tree::TempExp(reg_manager->FramePointer()),
                new tree::ConstExp(offset))),
            new tree::TempExp(reg_manager->ArgRegs()->NthTemp(index - 1))));
        offset -= reg_manager->WordSize();
      } else {  // stack parameters, 无需存入栈中, 在frame中存入访问方式即可
        formals_->push_back(
            new InFrameAccess(reg_manager->WordSize() * (index - 6)));
      /* 前6个参数在寄存器中传递，第七个参数在栈中传递，其地址为framePointer的地址+8
           *  -------------------------------- <-- WORDSIZE * framePointer(arg_index - 5)
           *  | arg7                          |
           *  -------------------------------- <-- starting from framePointer + 8 * 1
           *  | return address                |
           *  -------------------------------- <--framePointer points to return address's address
           *  | arg1(parents's) staticLink    |
           *  -------------------------------- <-- starting from framePointer - 8
           *  | locals...                     |
           *  -------------------------------- <-- %rsp framePointer = (%rsp +frameSize)
           */
      }
    }  // end if escape
    else {
      temp::Temp* temp = temp::TempFactory::NewTemp();
      formals_->push_back(new InRegAccess(temp));
      if (index <= 6) {
        view_shift->push_back(new tree::MoveStm(
            new tree::TempExp(temp),
            new tree::TempExp(reg_manager->ArgRegs()->NthTemp(index - 1))));
      } else {
        view_shift->push_back(new tree::MoveStm(
            new tree::TempExp(temp),
            new tree::MemExp(new tree::BinopExp(
                tree::PLUS_OP, new tree::TempExp(reg_manager->FramePointer()),
                new tree::ConstExp(reg_manager->WordSize() * (index - 6))))));
      }
    }
    index++;
  }
}

Access* X64Frame::AllocLocal(bool escape_) {
  Access* local_ptr;
  if (escape_) {
    local_ptr = new InFrameAccess(offset);
    offset -= reg_manager->WordSize();
// #ifdef GC
  // 复用formal, 将栈中存储的指针放入formal中供垃圾回收使用
    formals_->push_back(local_ptr);
// #endif
  } else
    local_ptr = new InRegAccess(temp::TempFactory::NewTemp());
  return local_ptr;
}

/******************** NewFrame Function ********************/

frame::Frame* NewFrame(temp::Label* name, std::list<bool> formals) {
  return new X64Frame(name, &formals);
}

/******************** procEntryExit Functions ********************/

/* 添加view shift相关指令 */
tree::Stm* procEntryExit1(tree::Stm* stm_, frame::Frame* frame_) {
  std::list<tree::Stm*>* view_shift_stms = frame_->ViewShift();
  tree::Stm* function_code = stm_;
  for (tree::Stm* view_shift : *view_shift_stms)
    function_code = new tree::SeqStm(view_shift, function_code);
  return function_code;
}

/* 函数结束时保证returnSink活跃 */
void procEntryExit2(assem::InstrList& body) {
  body.Append(new assem::OperInstr(
      "", new temp::TempList(),
      reg_manager->ReturnSink(), nullptr));
}

/* 添加prelog ret 和 epilog，以及分配帧和回收帧的指令 
   并为栈传参分配空间 */
assem::Proc* procEntryExit3(frame::Frame* frame_, assem::InstrList* body) {
  std::string prelog;
  std::string epilog;
  std::string function_name = frame_->lable_->Name();
  int spaceForArg = (frame_->maxCallargs > 6) ? frame_->maxCallargs - 6 : 0;
  int framesize = -frame_->offset + spaceForArg * reg_manager->WordSize();
  prelog += function_name + ":\n";
  prelog += ".set " + function_name + "_framesize, " +
            std::to_string(framesize) + "\n";
  prelog += "subq $" + std::to_string(framesize) + ", %rsp\n";

  epilog += "addq $" + std::to_string(framesize) + ", %rsp\n";
  epilog += "retq\n";
  return new assem::Proc(prelog, body, epilog);
}

}  // namespace frame