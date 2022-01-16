#include "tiger/codegen/codegen.h"

extern frame::RegManager *reg_manager;
extern std::vector<std::string> functions_ret_ptr;

std::vector<int> pointer_in_frame_offset;
std::vector<int> pointer_in_arg;

namespace {

constexpr int maxlen = 1024;

}  // namespace

namespace cg {

std::map<std::string, temp::Temp *> str_tempReg_map;

/***************** For GC *****************/

/* 指针传递规则：
    (1) movq的dst的指针属性和src相同
    (2) addq/subq若src中的一个为pointer则dst为pointer,
    (3) subq若src中的两个均为pointer则dst不是pointer(tiger中不涉及)
*/
/*
  指针的根(指针最初的位置, 指针传递的起始)
    (1) 帧中存储指针的位置
    (2) call返回指针的函数的%rax
    (3) 函数参数中传入的指针
  除此之外寄存器中的指针均为传递而来
*/

void pointerTransfer(assem::Instr *instr) {
  // movq中指针的传递
  if (typeid(*instr) == typeid(assem::MoveInstr)) {
    assem::MoveInstr *mov_ins = static_cast<assem::MoveInstr *>(instr);
    if (mov_ins->dst_ && mov_ins->src_)
      mov_ins->dst_->GetList().front()->storePointer =
          mov_ins->src_->GetList().front()->storePointer;
  }
  // addq中指针的传递
  if (typeid(*instr) == typeid(assem::OperInstr)) {
    assem::OperInstr *add_ins = static_cast<assem::OperInstr *>(instr);
    if ((add_ins->assem_.find("add") != add_ins->assem_.npos ||
         add_ins->assem_.find("sub") != add_ins->assem_.npos) &&
        add_ins->dst_ && add_ins->src_ &&
        add_ins->src_->GetList().front()->storePointer)
      add_ins->dst_->GetList().front()->storePointer = true;
  }
}

void generatePointRoot(frame::Frame *frame_) {
  /* 提取函数中的
    (1) 帧中存储指针的位置
    (2) 参数中存储指针的位置
    */
  std::list<frame::Access *> *InframeFormals = frame_->Formals();
  for (frame::Access *access : *InframeFormals) {
    if (typeid(*access) == typeid(frame::InFrameAccess) &&
        static_cast<frame::InFrameAccess *>(access)->storePointer)
      pointer_in_frame_offset.push_back(
          static_cast<frame::InFrameAccess *>(access)->offset);
  }
  auto iter = (*InframeFormals).begin();
  int pos = 1;
  for (; iter != (*InframeFormals).end(); iter++, pos++) {
    frame::Access *access = *iter;
    if (typeid(*access) == typeid(frame::InRegAccess) &&
        static_cast<frame::InRegAccess *>(access)->reg->storePointer)
      pointer_in_arg.push_back(pos);
  }
}

bool argIsPointer(int pos) {
  return std::find(pointer_in_arg.begin(), pointer_in_arg.end(), pos) !=
         pointer_in_arg.end();
}

bool returnValueIsPointer(std::string func_name) {
  return func_name == "init_array" || func_name == "alloc_record" ||
         std::find(functions_ret_ptr.begin(), functions_ret_ptr.end(),
                   func_name) != functions_ret_ptr.end();
}

bool inFrameValueIsPointer(int offset) {
  return std::find(pointer_in_frame_offset.begin(),
                   pointer_in_frame_offset.end(),
                   offset) != pointer_in_frame_offset.end();
}

/***************** End For GC *****************/

/* frame中记录call中参数的最大数目,
 * 在栈传参时从callee frame的底部向上放置参数
 * 从而保证传参不覆盖其他值, 传参所用空间可以被复用*/
/*
 * --------------------------------
 *| caller's frame                |
 *---------------------------------
 *| ... arg8 arg7                 |
 *|(included in caller's frame)   |
 *--------------------------------  <-- %rsp after callee ret
 *| return address                |
 * -------------------------------  <-- (callee's framePointer) %rsp is restored
 *| callee's frame                |                to here after callee excution
  | (the first word is callee's   |
  | parent's staticLink)          |
 *---------------------------------
 */
/* 由于%rsp无需进行寄存器分配，故%rsp不出现在src和dst中 */

void emit(assem::Instr *instr, assem::InstrList &instr_list_) {
  pointerTransfer(instr);
  instr_list_.Append(instr);
}

void saveCalleeSavedRegs(assem::InstrList &instr_list) {
  std::list<temp::Temp *> callee_save = reg_manager->CalleeSaves()->GetList();
  for (temp::Temp *reg : callee_save) {
    temp::Temp *reg_to_save_in = temp::TempFactory::NewTemp();
    std::string reg_name = *(reg_manager->getTempMap()->Look(reg));
    cg::str_tempReg_map[reg_name] = reg_to_save_in;

    temp::TempList *src = new temp::TempList(reg);
    temp::TempList *dst = new temp::TempList(reg_to_save_in);
    assem::Instr *ins = new assem::MoveInstr("movq `s0, `d0", dst, src);
    emit(ins, instr_list);
  }
}

void restoreCalleeSavedRegs(assem::InstrList &instr_list) {
  std::list<temp::Temp *> callee_save = reg_manager->CalleeSaves()->GetList();
  for (temp::Temp *reg : callee_save) {
    std::string reg_name = *(reg_manager->getTempMap()->Look(reg));
    temp::Temp *reg_to_save_in = cg::str_tempReg_map[reg_name];

    temp::TempList *src = new temp::TempList(reg_to_save_in);
    temp::TempList *dst = new temp::TempList(reg);
    assem::Instr *ins = new assem::MoveInstr("movq `s0, `d0", dst, src);
    emit(ins, instr_list);
  }
}

void CodeGen::Codegen() {
  assem::InstrList instr_list_;
  fs_ = frame_->lable_->Name() + "_framesize";
  std::list<tree::Stm *> function_stms = traces_.get()->GetStmList()->GetList();

  // For GC
  generatePointRoot(frame_);

  saveCalleeSavedRegs(instr_list_);
  for (tree::Stm *stm_ : function_stms) stm_->Munch(instr_list_, fs_);
  restoreCalleeSavedRegs(instr_list_);
  frame::procEntryExit2(instr_list_);

  assem::InstrList *new_ins_list = new assem::InstrList();
  for (auto ins : instr_list_.GetList()) new_ins_list->Append(ins);

  assem_instr_ = std::make_unique<AssemInstr>(new_ins_list);
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList()) instr->Print(out, map);
  fprintf(out, "\n");
}

}  // namespace cg

namespace tree {

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  std::string ass = temp::LabelFactory::LabelString(label_);
  assem::Instr *ins = new assem::LabelInstr(ass, label_);
  cg::emit(ins, instr_list);
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  assem::Instr *ins = new assem::OperInstr(
      "jmp `j0", nullptr, nullptr,
      new assem::Targets(new std::vector<temp::Label *>(1, jumps_->front())));
  cg::emit(ins, instr_list);
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *left_temp = left_->Munch(instr_list, fs);
  temp::Temp *right_temp = right_->Munch(instr_list, fs);
  temp::TempList *src = new temp::TempList({left_temp, right_temp});
  assem::Instr *ins_cmp =
      new assem::OperInstr("cmpq `s1, `s0", nullptr, src, nullptr);
  std::string ass_j;
  switch (op_) {
    case tree::RelOp::EQ_OP:
      ass_j = "je";
      break;
    case tree::RelOp::NE_OP:
      ass_j = "jne";
      break;
    case tree::RelOp::LE_OP:
      ass_j = "jle";
      break;
    case tree::RelOp::LT_OP:
      ass_j = "jl";
      break;
    case tree::RelOp::GE_OP:
      ass_j = "jge";
      break;
    case tree::RelOp::GT_OP:
      ass_j = "jg";
      break;
    case tree::RelOp::UGE_OP:
      ass_j = "jae";
      break;
    case tree::RelOp::UGT_OP:
      ass_j = "ja";
      break;
    case tree::RelOp::ULE_OP:
      ass_j = "jbe";
      break;
    case tree::RelOp::ULT_OP:
      ass_j = "jb";
      break;
    default:
      break;
  }
  ass_j += " `j0";
  std::vector<temp::Label *> *lables =
      new std::vector<temp::Label *>({true_label_});
  assem::Instr *ins_cjmp =
      new assem::OperInstr(ass_j, nullptr, nullptr, new assem::Targets(lables));

  cg::emit(ins_cmp, instr_list);
  cg::emit(ins_cjmp, instr_list);
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *src_reg = src_->Munch(instr_list, fs);
  if (typeid(*dst_) == typeid(tree::MemExp)) {
    /* For GC:
      保证访问帧中的变量通过一条指令, 且指令只有一种形式, 便于后面解析 */
    tree::MemExp *memexp = static_cast<tree::MemExp *>(dst_);
    if (typeid(*(memexp->exp_)) == typeid(tree::BinopExp)) {
      tree::BinopExp *binopexp = static_cast<tree::BinopExp *>(memexp->exp_);
      if (typeid(*(binopexp->right_)) == typeid(tree::ConstExp) &&
          typeid(*(binopexp->left_)) == typeid(tree::TempExp) &&
          static_cast<tree::TempExp *>(binopexp->left_)->temp_ ==
              reg_manager->FramePointer()) {  //取帧中的值
        std::string in_frame_dst = "(" + std::string(fs);
        int offset = static_cast<tree::ConstExp *>(binopexp->right_)->consti_;
        if (offset < 0)
          in_frame_dst += std::to_string(offset);
        else
          in_frame_dst += ("+" + std::to_string(offset));
        in_frame_dst += ")(%rsp)";
        std::string ass = "movq `s0, " + in_frame_dst;
        assem::Instr *instr = new assem::OperInstr(
            ass, nullptr, new temp::TempList({src_reg}), nullptr);
        cg::emit(instr, instr_list);
        return;
      }
      if (typeid(*(binopexp->left_)) == typeid(tree::ConstExp) &&
          typeid(*(binopexp->right_)) == typeid(tree::TempExp) &&
          static_cast<tree::TempExp *>(binopexp->right_)->temp_ ==
              reg_manager->FramePointer()) {
        std::string in_frame_dst = "(" + std::string(fs);
        int offset = static_cast<tree::ConstExp *>(binopexp->left_)->consti_;
        if (offset < 0)
          in_frame_dst += std::to_string(offset);
        else
          in_frame_dst += ("+" + std::to_string(offset));
        in_frame_dst += ")(%rsp)";
        std::string ass = "movq `s0, " + in_frame_dst;
        assem::Instr *instr = new assem::OperInstr(
            ass, nullptr, new temp::TempList({src_reg}), nullptr);
        cg::emit(instr, instr_list);
        return;
      }
    }
    /* End For GC*/
    temp::Temp *dst_addr_reg =
        static_cast<tree::MemExp *>(dst_)->exp_->Munch(instr_list, fs);
    std::string ass = "movq `s0, (`s1)";
    assem::Instr *instr = new assem::OperInstr(
        ass, nullptr, new temp::TempList({src_reg, dst_addr_reg}), nullptr);
    cg::emit(instr, instr_list);
  } else {
    temp::Temp *dst_reg = dst_->Munch(instr_list, fs);
    std::string ass = "movq `s0, `d0";
    assem::Instr *instr = new assem::MoveInstr(ass, new temp::TempList(dst_reg),
                                               new temp::TempList(src_reg));
    cg::emit(instr, instr_list);
  }
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *left_munch = left_->Munch(instr_list, fs);
  temp::Temp *right_munch = right_->Munch(instr_list, fs);
  temp::Temp *dst_reg = temp::TempFactory::NewTemp();
  switch (op_) {
    case tree::BinOp::PLUS_OP: {
      std::string ass_mov = "movq `s0, `d0";
      std::string ass_add = "addq `s0, `d0";
      assem::Instr *ins_mov = new assem::MoveInstr(
          ass_mov, new temp::TempList(dst_reg), new temp::TempList(left_munch));
      assem::Instr *ins_add = new assem::OperInstr(
          ass_add, new temp::TempList(dst_reg),
          new temp::TempList({right_munch, dst_reg}), nullptr);
      cg::emit(ins_mov, instr_list);
      cg::emit(ins_add, instr_list);
      break;
    }
    case tree::BinOp::MINUS_OP: {
      std::string ass_mov = "movq `s0, `d0";
      std::string ass_min = "subq `s0, `d0";
      assem::Instr *ins_mov = new assem::MoveInstr(
          ass_mov, new temp::TempList(dst_reg), new temp::TempList(left_munch));
      assem::Instr *ins_min = new assem::OperInstr(
          ass_min, new temp::TempList(dst_reg),
          new temp::TempList({right_munch, dst_reg}), nullptr);
      cg::emit(ins_mov, instr_list);

      cg::emit(ins_min, instr_list);
      break;
    }
    case tree::BinOp::MUL_OP: {
      std::string ass_mov = "movq `s0, `d0";
      std::string ass_mul = "imulq `s0";

      assem::Instr *ins_mov = new assem::MoveInstr(
          ass_mov, new temp::TempList(reg_manager->findByName("%rax")),
          new temp::TempList(left_munch));

      assem::Instr *ins_mul = new assem::OperInstr(
          ass_mul,
          new temp::TempList({reg_manager->findByName("%rax"),
                              reg_manager->findByName("%rdx")}),
          new temp::TempList({right_munch, reg_manager->findByName("%rax")}),
          nullptr);
      assem::Instr *ins_mov_rax_to_dst =
          new assem::MoveInstr(ass_mov, new temp::TempList({dst_reg}),
                               new temp::TempList({
                                   reg_manager->findByName("%rax"),
                               }));

      cg::emit(ins_mov, instr_list);
      cg::emit(ins_mul, instr_list);
      cg::emit(ins_mov_rax_to_dst, instr_list);

      break;
    }
    case tree::BinOp::DIV_OP: {
      std::string ass_mov = "movq `s0, `d0";
      std::string ass_cqto = "cqto";
      std::string ass_div = "idivq `s0";

      assem::Instr *ins_mov = new assem::MoveInstr(
          ass_mov, new temp::TempList(reg_manager->findByName("%rax")),
          new temp::TempList(left_munch));
      assem::Instr *ins_cqto = new assem::OperInstr(
          ass_cqto,
          new temp::TempList({reg_manager->findByName("%rax"),
                              reg_manager->findByName("%rdx")}),
          new temp::TempList(reg_manager->findByName("%rax")), nullptr);
      assem::Instr *ins_div = new assem::OperInstr(
          ass_div,
          new temp::TempList({reg_manager->findByName("%rax"),
                              reg_manager->findByName("%rdx")}),
          new temp::TempList({
              right_munch,
              reg_manager->findByName("%rax"),
              reg_manager->findByName("%rdx"),
          }),
          nullptr);
      assem::Instr *ins_mov_rax_to_dst =
          new assem::MoveInstr(ass_mov, new temp::TempList({dst_reg}),
                               new temp::TempList({
                                   reg_manager->findByName("%rax"),
                               }));

      cg::emit(ins_mov, instr_list);

      cg::emit(ins_cqto, instr_list);
      cg::emit(ins_div, instr_list);
      cg::emit(ins_mov_rax_to_dst, instr_list);

      break;
    }
    default:
      break;
  }
  return dst_reg;
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *dst_reg = temp::TempFactory::NewTemp();
  /* For GC:
      保证访问帧中的变量通过一条指令, 且指令只有一种形式, 便于后面解析 */
  if (typeid(*(exp_)) == typeid(tree::BinopExp)) {
    tree::BinopExp *binopexp = static_cast<tree::BinopExp *>(exp_);
    if (typeid(*(binopexp->right_)) == typeid(tree::ConstExp) &&
        typeid(*(binopexp->left_)) == typeid(tree::TempExp) &&
        static_cast<tree::TempExp *>(binopexp->left_)->temp_ ==
            reg_manager->FramePointer()) {
      std::string in_frame_dst = "(" + std::string(fs);
      int offset = static_cast<tree::ConstExp *>(binopexp->right_)->consti_;

      // For GC, 指针根(1)
      dst_reg->storePointer = cg::inFrameValueIsPointer(offset);

      if (offset < 0)
        in_frame_dst += std::to_string(offset);
      else
        in_frame_dst += ("+" + std::to_string(offset));
      in_frame_dst += ")(%rsp)";
      assem::Instr *mem_ins =
          new assem::OperInstr("movq " + in_frame_dst + ", `d0",
                               new temp::TempList({dst_reg}), nullptr, nullptr);
      cg::emit(mem_ins, instr_list);
      return dst_reg;
    }
    if (typeid(*(binopexp->left_)) == typeid(tree::ConstExp) &&
        typeid(*(binopexp->right_)) == typeid(tree::TempExp) &&
        static_cast<tree::TempExp *>(binopexp->right_)->temp_ ==
            reg_manager->FramePointer()) {
      std::string in_frame_dst = "(" + std::string(fs);
      int offset = static_cast<tree::ConstExp *>(binopexp->left_)->consti_;

      // For GC, 指针根(1)
      dst_reg->storePointer = cg::inFrameValueIsPointer(offset);

      if (offset < 0)
        in_frame_dst += std::to_string(offset);
      else
        in_frame_dst += ("+" + std::to_string(offset));
      in_frame_dst += ")(%rsp)";
      assem::Instr *mem_ins =
          new assem::OperInstr("movq" + in_frame_dst + ", `d0",
                               new temp::TempList({dst_reg}), nullptr, nullptr);
      cg::emit(mem_ins, instr_list);
      return dst_reg;
    }
  }
  temp::Temp *addr_reg = exp_->Munch(instr_list, fs);
  assem::Instr *mem_ins =
      new assem::OperInstr("movq (`s0), `d0", new temp::TempList({dst_reg}),
                           new temp::TempList({addr_reg}), nullptr);
  cg::emit(mem_ins, instr_list);
  return dst_reg;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  if (temp_ == reg_manager->FramePointer()) {
    temp::Temp *temp_store_fp = temp::TempFactory::NewTemp();
    std::string ass = "leaq " + std::string(fs) + "(%rsp), `d0";
    assem::Instr *instr = new assem::OperInstr(
        ass, new temp::TempList(temp_store_fp), nullptr, nullptr);
    cg::emit(instr, instr_list);
    return temp_store_fp;
  }
  return temp_;
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  stm_->Munch(instr_list, fs);
  return exp_->Munch(instr_list, fs);
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *dst_rip_reg = temp::TempFactory::NewTemp();
  std::string ass_lea = "leaq " + name_->Name() + "(%rip), `d0";
  assem::Instr *ins_getRIP = new assem::OperInstr(
      ass_lea, new temp::TempList({dst_rip_reg}), nullptr, nullptr);
  cg::emit(ins_getRIP, instr_list);
  return dst_rip_reg;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *const_reg = temp::TempFactory::NewTemp();
  std::string ass = "movq $" + std::to_string(consti_) + ", `d0";
  assem::Instr *ins = new assem::OperInstr(ass, new temp::TempList({const_reg}),
                                           nullptr, nullptr);
  cg::emit(ins, instr_list);
  return const_reg;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  std::string funcName = static_cast<tree::NameExp *>(fun_)->name_->Name();

  temp::TempList *arg_Regs = args_->MunchArgs(instr_list, fs);

  /* call Use传参寄存器, def全部caller saves(使之失效)
  因此无需显式save caller saves reg*/
  std::string ass_call = "callq " + funcName;
  assem::Instr *ins_call = new assem::OperInstr(
      ass_call, reg_manager->CallerSaves(), arg_Regs, nullptr);
  cg::emit(ins_call, instr_list);

  /* For GC: 在call语句后面加入label(即return address为label) */
  temp::Label *return_addr_label_ = temp::LabelFactory::NewLabel();
  std::string ass = temp::LabelFactory::LabelString(return_addr_label_);
  assem::Instr *ins_label = new assem::LabelInstr(ass, return_addr_label_);
  cg::emit(ins_label, instr_list);

  /*移动返回值*/
  temp::Temp *ret_reg = temp::TempFactory::NewTemp();

  // For GC, 指针根(2)
  reg_manager->ReturnValue()->storePointer = cg::returnValueIsPointer(funcName);

  std::string ass_mov_ret = "movq `s0, `d0";
  assem::Instr *ins_mov =
      new assem::MoveInstr(ass_mov_ret, new temp::TempList({ret_reg}),
                           new temp::TempList({reg_manager->ReturnValue()}));
  cg::emit(ins_mov, instr_list);

  return ret_reg;
}

/*有side effect!*/
temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list,
                                   std::string_view fs) {
  temp::TempList *args_Regs = new temp::TempList();

  auto exp_list_iter = exp_list_.begin();
  for (int i = 0; i < exp_list_.size(); i++, exp_list_iter++) {
    temp::Temp *arg_reg = (*exp_list_iter)->Munch(instr_list, fs);
    if (i < 6) {
      std::string ass_mov = "movq `s0, `d0";
      args_Regs->Append(reg_manager->ArgRegs()->NthTemp(i));

      // For GC:指针根(3)
      reg_manager->ArgRegs()->NthTemp(i)->storePointer = cg::argIsPointer(i);

      assem::Instr *ins_mov = new assem::MoveInstr(
          ass_mov, new temp::TempList(reg_manager->ArgRegs()->NthTemp(i)),
          new temp::TempList({arg_reg}));
      cg::emit(ins_mov, instr_list);
    } else {
      int distance = (i - 6) * reg_manager->WordSize();
      std::string ass_mov = "movq `s0, " + std::to_string(distance) + "(%rsp)";
      assem::Instr *ins_mov = new assem::OperInstr(
          ass_mov, nullptr, new temp::TempList({arg_reg}), nullptr);
      cg::emit(ins_mov, instr_list);
    }
  }
  return args_Regs;
}

}  // namespace tree
