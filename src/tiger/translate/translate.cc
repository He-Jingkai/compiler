#include "tiger/translate/translate.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;
extern std::vector<std::string> functions_ret_ptr;

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  return new Access(level, level->frame_->AllocLocal(escape));
}

class Cx {
 public:
  temp::Label **trues_;
  temp::Label **falses_;
  tree::Stm *stm_;

  Cx(temp::Label **trues, temp::Label **falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
 public:
  [[nodiscard]] virtual tree::Exp *UnEx() const = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() const = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) const = 0;
};

class ExpAndTy {
 public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
 public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() const override { return exp_; }
  [[nodiscard]] tree::Stm *UnNx() const override {
    return new tree::ExpStm(exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    tree::CjumpStm *stm_ = new tree::CjumpStm(
        tree::NE_OP, exp_, new tree::ConstExp(0), nullptr, nullptr);
    temp::Label **true_lable = &stm_->true_label_;
    temp::Label **false_lable = &stm_->false_label_;
    return Cx(true_lable, false_lable, stm_);
  }
};

class NxExp : public Exp {
 public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() const override { return stm_; }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    errormsg->Error(0, "Translate error");
  }
};

class CxExp : public Exp {
 public:
  Cx cx_;

  CxExp(temp::Label **trues, temp::Label **falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    temp::Temp *r = temp::TempFactory::NewTemp();
    temp::Label *true_lable = temp::LabelFactory::NewLabel();
    temp::Label *false_lable = temp::LabelFactory::NewLabel();
    *(cx_.falses_) = false_lable;
    *(cx_.trues_) = true_lable;
    return new tree::EseqExp(
        new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),
        new tree::EseqExp(
            cx_.stm_, new tree::EseqExp(
                          new tree::LabelStm(false_lable),
                          new tree::EseqExp(
                              new tree::MoveStm(new tree::TempExp(r),
                                                new tree::ConstExp(0)),
                              new tree::EseqExp(new tree::LabelStm(true_lable),
                                                new tree::TempExp(r))))));
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    return new tree::ExpStm(UnEx());
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override { return cx_; }
};

void ProgTr::Translate() {
  temp::Label *main_lable = temp::LabelFactory::NamedLabel("tigermain");
  main_level_ = std::make_unique<tr::Level>(
      frame::NewFrame(main_lable, std::list<bool>()), nullptr);
  FillBaseTEnv();
  FillBaseVEnv();
  absyn_tree_->Translate(venv_.get(), tenv_.get(), main_level_.get(),
                         main_lable, errormsg_.get());
}

}  // namespace tr

namespace absyn {
/********** GC Protocol **********/

/*Put record descriptor into stringFrag
 * Structure: "$010101$"
 * LableName: "$typeName$_DESCRIPTOR"
 */
void emitRecordRecordTypeDescriptor(type::RecordTy *recordTy,
                                    sym::Symbol *name) {
  std::string pointMAP, recordNAME;
  recordNAME = name->Name() + "_DESCRIPTOR";
  std::list<type::Field *> field_list = recordTy->fields_->GetList();
  for (const type::Field *field : field_list) {
    if (typeid(*field->ty_->ActualTy()) == typeid(type::RecordTy) ||
        typeid(*field->ty_->ActualTy()) == typeid(type::ArrayTy))
      pointMAP += "1";
    else
      pointMAP += "0";
  }

  temp::Label *str_lable = temp::LabelFactory::NamedLabel(recordNAME);
  frame::StringFrag *str_frag = new frame::StringFrag(str_lable, pointMAP);
  frags->PushBack(str_frag);
}

bool IsPointer(type::Ty *ty_) {
  return typeid(*(ty_->ActualTy())) == typeid(type::RecordTy) ||
         typeid(*(ty_->ActualTy())) == typeid(type::ArrayTy);
}

/********** END GC Protocol **********/

tree::Exp *staticLink(tr::Level *target_, tr::Level *current_);

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  env::EnvEntry *entry = venv->Look(sym_);
  if (entry && typeid(*entry) == typeid(env::VarEntry)) {
    tr::Access *var_access_ = static_cast<env::VarEntry *>(entry)->access_;
    tr::Exp *trans_res = new tr::ExExp(
        var_access_->access_->ToExp(staticLink(var_access_->level_, level)));

    return new tr::ExpAndTy(
        trans_res, static_cast<env::VarEntry *>(entry)->ty_->ActualTy());
  }

  errormsg->Error(pos_, "undefined variable %s", sym_->Name().c_str());
  return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *var_translate =
      var_->Translate(venv, tenv, level, label, errormsg);

  std::list<type::Field *> fieldsList =
      (static_cast<type::RecordTy *>(var_translate->ty_->ActualTy()))
          ->fields_->GetList();
  int pos = 0;
  for (const type::Field *field : fieldsList) {
    if (field->name_->Name() == sym_->Name()) {
      tr::Exp *exp_ = new tr::ExExp(new tree::MemExp(new tree::BinopExp(
          tree::BinOp::PLUS_OP, var_translate->exp_->UnEx(),
          new tree::ConstExp(pos * reg_manager->WordSize()))));
      return new tr::ExpAndTy(exp_, field->ty_->ActualTy());
    }
    pos++;
  }

  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().c_str());
  return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *var_trans = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *sub_trans =
      subscript_->Translate(venv, tenv, level, label, errormsg);

  tr::Exp *exp_ = new tr::ExExp(new tree::MemExp(new tree::BinopExp(
      tree::BinOp::PLUS_OP, var_trans->exp_->UnEx(),
      new tree::BinopExp(tree::BinOp::MUL_OP, sub_trans->exp_->UnEx(),
                         new tree::ConstExp(reg_manager->WordSize())))));
  return new tr::ExpAndTy(
      exp_, static_cast<type::ArrayTy *>(var_trans->ty_)->ty_->ActualTy());
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  if (typeid(*var_) == typeid(absyn::SimpleVar))
    return (static_cast<absyn::SimpleVar *>(var_))
        ->Translate(venv, tenv, level, label, errormsg);

  if (typeid(*var_) == typeid(absyn::FieldVar))
    return (static_cast<absyn::FieldVar *>(var_))
        ->Translate(venv, tenv, level, label, errormsg);

  if (typeid(*var_) == typeid(absyn::SubscriptVar))
    return (static_cast<absyn::SubscriptVar *>(var_))
        ->Translate(venv, tenv, level, label, errormsg);

  errormsg->Error(pos_, "var type error");
  return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(val_)),
                          type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  temp::Label *str_lable = temp::LabelFactory::NewLabel();
  frame::StringFrag *str_frag = new frame::StringFrag(str_lable, str_);
  frags->PushBack(str_frag);
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(str_lable)),
                          type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  env::EnvEntry *entry = venv->Look(func_);
  type::Ty *ty_ = type::VoidTy::Instance();
  if ((static_cast<env::FunEntry *>(entry))->result_)
    ty_ = (static_cast<env::FunEntry *>(entry))->result_->ActualTy();
  std::list<type::Ty *> ty_list =
      (static_cast<env::FunEntry *>(entry))->formals_->GetList();
  std::list<absyn::Exp *> exp_list = args_->GetList();
  tree::ExpList *mov_arg_exp = new tree::ExpList();

  while (exp_list.size()) {
    type::Ty *ty = ty_list.front();
    absyn::Exp *exp = exp_list.front();
    tr::ExpAndTy *arg_trans =
        exp->Translate(venv, tenv, level, label, errormsg);
    mov_arg_exp->Append(arg_trans->exp_->UnEx());
    ty_list.pop_front();
    exp_list.pop_front();
  }

  /* 更新最大栈传参数, 用于确定帧大小 */
  level->frame_->maxCallargs =
      (level->frame_->maxCallargs > mov_arg_exp->GetList().size())
          ? level->frame_->maxCallargs
          : mov_arg_exp->GetList().size();

  /* 如果不是level-0函数, 需要传递static link作为第一个参数 */
  tr::Exp *exp_;
  if (!(static_cast<env::FunEntry *>(entry))->level_->parent_) {
    exp_ = new tr::ExExp(new tree::CallExp(
        new tree::NameExp(temp::LabelFactory::NamedLabel(func_->Name())),
        mov_arg_exp));
  } else {
    tree::Exp *static_link_ = staticLink(
        (static_cast<env::FunEntry *>(entry))->level_->parent_, level);
    mov_arg_exp->Insert(static_link_);

    exp_ = new tr::ExExp(new tree::CallExp(
        new tree::NameExp(temp::LabelFactory::NamedLabel(func_->Name())),
        mov_arg_exp));
  }
  return new tr::ExpAndTy(exp_, ty_);
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *left_exp_trans =
      left_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *right_exp_trans =
      right_->Translate(venv, tenv, level, label, errormsg);

  tr::Exp *exp_ = nullptr;
  switch (oper_) {
    case absyn::Oper::PLUS_OP:
      exp_ = new tr::ExExp(new tree::BinopExp(tree::BinOp::PLUS_OP,
                                              left_exp_trans->exp_->UnEx(),
                                              right_exp_trans->exp_->UnEx()));
      break;
    case absyn::Oper::MINUS_OP:
      exp_ = new tr::ExExp(new tree::BinopExp(tree::BinOp::MINUS_OP,
                                              left_exp_trans->exp_->UnEx(),
                                              right_exp_trans->exp_->UnEx()));
      break;
    case absyn::Oper::TIMES_OP:
      exp_ = new tr::ExExp(new tree::BinopExp(tree::BinOp::MUL_OP,
                                              left_exp_trans->exp_->UnEx(),
                                              right_exp_trans->exp_->UnEx()));
      break;
    case absyn::Oper::DIVIDE_OP:
      exp_ = new tr::ExExp(new tree::BinopExp(tree::BinOp::DIV_OP,
                                              left_exp_trans->exp_->UnEx(),
                                              right_exp_trans->exp_->UnEx()));
      break;

    case absyn::Oper::GE_OP: {
      tree::CjumpStm *cjump_ =
          new tree::CjumpStm(tree::RelOp::GE_OP, left_exp_trans->exp_->UnEx(),
                             right_exp_trans->exp_->UnEx(), nullptr, nullptr);
      exp_ = new tr::CxExp(&cjump_->true_label_, &cjump_->false_label_, cjump_);
      break;
    }
    case absyn::Oper::GT_OP: {
      tree::CjumpStm *cjump_ =
          new tree::CjumpStm(tree::RelOp::GT_OP, left_exp_trans->exp_->UnEx(),
                             right_exp_trans->exp_->UnEx(), nullptr, nullptr);
      exp_ = new tr::CxExp(&cjump_->true_label_, &cjump_->false_label_, cjump_);
      break;
    }
    case absyn::Oper::LE_OP: {
      tree::CjumpStm *cjump_ =
          new tree::CjumpStm(tree::RelOp::LE_OP, left_exp_trans->exp_->UnEx(),
                             right_exp_trans->exp_->UnEx(), nullptr, nullptr);
      exp_ = new tr::CxExp(&cjump_->true_label_, &cjump_->false_label_, cjump_);
      break;
    }
    case absyn::Oper::LT_OP: {
      tree::CjumpStm *cjump_ =
          new tree::CjumpStm(tree::RelOp::LT_OP, left_exp_trans->exp_->UnEx(),
                             right_exp_trans->exp_->UnEx(), nullptr, nullptr);
      exp_ = new tr::CxExp(&cjump_->true_label_, &cjump_->false_label_, cjump_);
      break;
    }
    case absyn::Oper::EQ_OP: {
      if (typeid(*(left_exp_trans->ty_->ActualTy())) ==
          typeid(type::StringTy)) {
        tree::ExpList *str_eq_args = new tree::ExpList();
        str_eq_args->Append(left_exp_trans->exp_->UnEx());
        str_eq_args->Append(right_exp_trans->exp_->UnEx());
        tree::CjumpStm *cjump_ = new tree::CjumpStm(
            tree::RelOp::EQ_OP,
            new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel(
                                  "string_equal")),
                              str_eq_args),
            new tree::ConstExp(1), nullptr, nullptr);
        exp_ =
            new tr::CxExp(&cjump_->true_label_, &cjump_->false_label_, cjump_);
      } else {
        tree::CjumpStm *cjump_ =
            new tree::CjumpStm(tree::RelOp::EQ_OP, left_exp_trans->exp_->UnEx(),
                               right_exp_trans->exp_->UnEx(), nullptr, nullptr);
        exp_ =
            new tr::CxExp(&cjump_->true_label_, &cjump_->false_label_, cjump_);
      }
      break;
    }
    case absyn::Oper::NEQ_OP: {
      if (typeid(*(left_exp_trans->ty_->ActualTy())) ==
          typeid(type::StringTy)) {
        tree::ExpList *str_eq_args = new tree::ExpList();
        str_eq_args->Append(left_exp_trans->exp_->UnEx());
        str_eq_args->Append(right_exp_trans->exp_->UnEx());
        tree::CjumpStm *cjump_ = new tree::CjumpStm(
            tree::RelOp::EQ_OP,
            new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel(
                                  "string_equal")),
                              str_eq_args),
            new tree::ConstExp(0), nullptr, nullptr);
        exp_ =
            new tr::CxExp(&cjump_->true_label_, &cjump_->false_label_, cjump_);
      } else {
        tree::CjumpStm *cjump_ =
            new tree::CjumpStm(tree::RelOp::NE_OP, left_exp_trans->exp_->UnEx(),
                               right_exp_trans->exp_->UnEx(), nullptr, nullptr);
        exp_ =
            new tr::CxExp(&cjump_->true_label_, &cjump_->false_label_, cjump_);
      }
      break;
    }
    default:
      break;
  }
  return new tr::ExpAndTy(exp_, type::IntTy::Instance());
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  type::Ty *type_pointer = tenv->Look(typ_)->ActualTy();
  tree::ExpList *field_exp_ = new tree::ExpList();
  std::list<absyn::EField *> efields = fields_->GetList();

  for (auto efield_iter = efields.begin(); efield_iter != efields.end();
       efield_iter++) {
    tr::ExpAndTy *field_ele_trans =
        (*efield_iter)->exp_->Translate(venv, tenv, level, label, errormsg);
    field_exp_->Append(field_ele_trans->exp_->UnEx());
  }

  /* Alloc record */
  temp::Temp *record_add_reg = temp::TempFactory::NewTemp();
  tree::ExpList *record_size_const = new tree::ExpList();
  record_size_const->Append(
      new tree::ConstExp(efields.size() * reg_manager->WordSize()));
  record_size_const->Append(new tree::NameExp(
      temp::LabelFactory::NamedLabel(typ_->Name() + "_DESCRIPTOR")));
  tree::Stm *alloca_record = new tree::MoveStm(
      new tree::TempExp(record_add_reg),
      new tree::CallExp(
          new tree::NameExp(temp::LabelFactory::NamedLabel("alloc_record")),
          record_size_const));

  /* Initialize Fields */
  std::list<tree::Exp *> exps_ = field_exp_->GetList();
  for (auto exps_iter = exps_.begin(); exps_iter != exps_.end(); exps_iter++)
    alloca_record = new tree::SeqStm(
        alloca_record,
        new tree::MoveStm(
            new tree::MemExp(new tree::BinopExp(
                tree::BinOp::PLUS_OP, new tree::TempExp(record_add_reg),
                new tree::ConstExp(reg_manager->WordSize() *
                                   std::distance(exps_.begin(), exps_iter)))),
            *exps_iter));

  tr::Exp *exp_rt = new tr::ExExp(
      new tree::EseqExp(alloca_record, new tree::TempExp(record_add_reg)));
  return new tr::ExpAndTy(exp_rt, type_pointer->ActualTy());
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  std::list<Exp *> exp_list = seq_->GetList();
  if (exp_list.size() == 0)
    return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());

  tr::ExpAndTy *result;
  tr::Exp *exp_return = new tr::ExExp(new tree::ConstExp(0));
  for (Exp *exp : exp_list) {
    result = exp->Translate(venv, tenv, level, label, errormsg);
    if (result->exp_)
      exp_return = new tr::ExExp(
          new tree::EseqExp(exp_return->UnNx(), result->exp_->UnEx()));
    else
      exp_return = new tr::ExExp(
          new tree::EseqExp(exp_return->UnNx(), new tree::ConstExp(0)));
  }

  return new tr::ExpAndTy(exp_return, result->ty_);
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *var_trans = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *right_exp_trans =
      exp_->Translate(venv, tenv, level, label, errormsg);

  tr::Exp *exp_return = new tr::NxExp(new tree::MoveStm(
      var_trans->exp_->UnEx(), right_exp_trans->exp_->UnEx()));

  return new tr::ExpAndTy(exp_return, type::VoidTy::Instance());
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *test_trans =
      test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *then_trans =
      then_->Translate(venv, tenv, level, label, errormsg);

  tr::Exp *exp_return = nullptr;
  if (!elsee_) {
    tr::Cx test_cx = test_trans->exp_->UnCx(errormsg);
    temp::Label *true_label = temp::LabelFactory::NewLabel();
    temp::Label *false_label = temp::LabelFactory::NewLabel();
    *(test_cx.falses_) = false_label;
    *(test_cx.trues_) = true_label;

    exp_return = new tr::NxExp(new tree::SeqStm(
        test_cx.stm_,
        new tree::SeqStm(new tree::LabelStm(true_label),
                         new tree::SeqStm(then_trans->exp_->UnNx(),
                                          new tree::LabelStm(false_label)))));
  } else {
    tr::ExpAndTy *else_trans =
        elsee_->Translate(venv, tenv, level, label, errormsg);
    tr::Cx test_cx = test_trans->exp_->UnCx(errormsg);
    temp::Label *true_label = temp::LabelFactory::NewLabel();
    temp::Label *false_label = temp::LabelFactory::NewLabel();
    *(test_cx.falses_) = false_label;
    *(test_cx.trues_) = true_label;
    temp::Label *meet_point = temp::LabelFactory::NewLabel();
    temp::Temp *ret_reg = temp::TempFactory::NewTemp();

    std::vector<temp::Label *> *jumps =
        new std::vector<temp::Label *>({meet_point});
    tree::JumpStm *jump_to_meet_point =
        new tree::JumpStm(new tree::NameExp(meet_point), jumps);
    tree::MoveStm *move_then_value_to_reg =
        new tree::MoveStm(new tree::TempExp(ret_reg), then_trans->exp_->UnEx());
    tree::MoveStm *move_else_value_to_reg =
        new tree::MoveStm(new tree::TempExp(ret_reg), else_trans->exp_->UnEx());
    tree::LabelStm *trueLable_stm = new tree::LabelStm(true_label);
    tree::LabelStm *falseLable_stm = new tree::LabelStm(false_label);
    tree::LabelStm *meetLable_stm = new tree::LabelStm(meet_point);
    exp_return = new tr::ExExp(new tree::EseqExp(
        test_cx.stm_,
        new tree::EseqExp(
            trueLable_stm,
            new tree::EseqExp(
                move_then_value_to_reg,
                new tree::EseqExp(
                    jump_to_meet_point,
                    new tree::EseqExp(
                        falseLable_stm,
                        new tree::EseqExp(
                            move_else_value_to_reg,
                            new tree::EseqExp(
                                jump_to_meet_point,
                                new tree::EseqExp(
                                    meetLable_stm,
                                    new tree::TempExp(ret_reg))))))))));
  }
  return new tr::ExpAndTy(exp_return, then_trans->ty_->ActualTy());
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  temp::Label *done_label = temp::LabelFactory::NewLabel();
  tr::ExpAndTy *test_trans =
      test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *body_exp_trans =
      body_->Translate(venv, tenv, level, done_label, errormsg);

  temp::Label *test_label = temp::LabelFactory::NewLabel();
  temp::Label *loop_label = temp::LabelFactory::NewLabel();
  tree::LabelStm *test_label_stm = new tree::LabelStm(test_label);
  tree::LabelStm *loop_label_stm = new tree::LabelStm(loop_label);
  tree::LabelStm *done_label_stm = new tree::LabelStm(done_label);
  std::vector<temp::Label *> *jumps =
      new std::vector<temp::Label *>({test_label});
  tree::JumpStm *jump_to_test_stm =
      new tree::JumpStm(new tree::NameExp(test_label), jumps);
  tr::Cx test_cx = test_trans->exp_->UnCx(errormsg);
  *test_cx.falses_ = done_label;
  *test_cx.trues_ = loop_label;
  tr::Exp *exp_return = new tr::NxExp(new tree::SeqStm(
      test_label_stm,
      new tree::SeqStm(
          test_cx.stm_,
          new tree::SeqStm(
              loop_label_stm,
              new tree::SeqStm(
                  body_exp_trans->exp_->UnNx(),
                  new tree::SeqStm(jump_to_test_stm, done_label_stm))))));
  return new tr::ExpAndTy(exp_return, type::VoidTy::Instance());
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* Convert to While */
  /* let list*/
  absyn::DecList *dec_list = new absyn::DecList();
  dec_list->Prepend(
      new absyn::VarDec(0, var_, sym::Symbol::UniqueSymbol("int"), lo_));
  dec_list->Prepend(
      new absyn::VarDec(0, sym::Symbol::UniqueSymbol("_loop_high_limit_"),
                        sym::Symbol::UniqueSymbol("int"), hi_));

  /* do */
  absyn::ExpList *loop_exp_list = new absyn::ExpList();
  absyn::Exp *increase_var_ = new absyn::AssignExp(
      0, new absyn::SimpleVar(0, var_),
      new absyn::OpExp(0, absyn::Oper::PLUS_OP,
                       new absyn::VarExp(0, new absyn::SimpleVar(0, var_)),
                       new absyn::IntExp(0, 1)));
  loop_exp_list->Prepend(new absyn::IfExp(
      0,
      new absyn::OpExp(0, absyn::Oper::EQ_OP,
                       new absyn::VarExp(0, new absyn::SimpleVar(0, var_)),
                       new absyn::VarExp(0, new absyn::SimpleVar(
                                                0, sym::Symbol::UniqueSymbol(
                                                       "_loop_high_limit_")))),
      new absyn::BreakExp(0), increase_var_));
  loop_exp_list->Prepend(body_);

  absyn::Exp *test_exp = new absyn::OpExp(
      0, absyn::Oper::LE_OP,
      new absyn::VarExp(0, new absyn::SimpleVar(0, var_)),
      new absyn::VarExp(0, new absyn::SimpleVar(0, sym::Symbol::UniqueSymbol(
                                                       "_loop_high_limit_"))));
  absyn::Exp *loop_body = new absyn::SeqExp(0, loop_exp_list);
  absyn::WhileExp *while_ = new absyn::WhileExp(0, test_exp, loop_body);

  absyn::Exp *let_and_while = new absyn::LetExp(0, dec_list, while_);
  return let_and_while->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* Jump to done Lable */
  std::vector<temp::Label *> *jumps = new std::vector<temp::Label *>({label});
  tree::Stm *jump_stm = new tree::JumpStm(new tree::NameExp(label), jumps);
  return new tr::ExpAndTy(new tr::NxExp(jump_stm), type::VoidTy::Instance());
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* 判断是否是tiger_main */
  static bool first = true;  // static只初始化一次
  bool is_main = false;
  if (first) {
    first = false;
    is_main = true;
  }

  venv->BeginScope();
  tenv->BeginScope();

  tree::Stm *stm_ = nullptr;
  std::list<absyn::Dec *> decs_list = decs_->GetList();
  if (decs_list.size()) {
    auto iter_ = decs_list.begin();
    stm_ = (*iter_)->Translate(venv, tenv, level, label, errormsg)->UnNx();
    iter_++;
    while (iter_ != decs_list.end()) {
      stm_ = new tree::SeqStm(
          stm_,
          (*iter_)->Translate(venv, tenv, level, label, errormsg)->UnNx());
      iter_++;
    }
  }

  tr::ExpAndTy *result;
  if (!body_)
    result = new tr::ExpAndTy(nullptr, type::IntTy::Instance());
  else
    result = body_->Translate(venv, tenv, level, label, errormsg);

  tenv->EndScope();
  venv->EndScope();

  tree::Exp *ret_ex = nullptr;

  if (stm_)
    ret_ex = new tree::EseqExp(stm_, result->exp_->UnEx());
  else
    ret_ex = result->exp_->UnEx();
  tree::Stm *ret_stm = new tree::ExpStm(ret_ex);

  if (is_main) frags->PushBack(new frame::ProcFrag(ret_stm, level->frame_));

  return new tr::ExpAndTy(new tr::ExExp(ret_ex), result->ty_->ActualTy());
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  type::Ty *typ_ptr = tenv->Look(typ_)->ActualTy();
  tr::ExpAndTy *size_tran =
      size_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *init_tran =
      init_->Translate(venv, tenv, level, label, errormsg);

  tree::ExpList *exp_list = new tree::ExpList();
  exp_list->Append(size_tran->exp_->UnEx());
  exp_list->Append(init_tran->exp_->UnEx());
  tr::Exp *exp_ = new tr::ExExp(new tree::CallExp(
      new tree::NameExp(temp::LabelFactory::NamedLabel("init_array")),
      exp_list));
  return new tr::ExpAndTy(exp_, typ_ptr);
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  std::list<FunDec *> funcdec_list = functions_->GetList();
  for (const FunDec *funDec : funcdec_list) {
    std::list<Field *> field_list = funDec->params_->GetList();
    type::TyList *param_types = new type::TyList();
    for (const Field *field : field_list) {
      type::Ty *type_temp = tenv->Look(field->typ_);
      param_types->Append(type_temp);
    }

    std::list<bool> formals;
    for (const Field *field : field_list) formals.push_back(field->escape_);

    temp::Label *lable_ = temp::LabelFactory::NamedLabel(funDec->name_->Name());
    tr::Level *level_ = tr::Level::NewLevel(level, lable_, formals);

    if (!funDec->result_)
      venv->Enter(funDec->name_, new env::FunEntry(level_, lable_, param_types,
                                                   type::VoidTy::Instance()));
    else
      venv->Enter(funDec->name_,
                  new env::FunEntry(level_, lable_, param_types,
                                    tenv->Look(funDec->result_)));
  }

  for (const FunDec *funDec : funcdec_list) {
    venv->BeginScope();
    tenv->BeginScope();
    std::list<Field *> field_list = funDec->params_->GetList();
    env::FunEntry *func_entry =
        static_cast<env::FunEntry *>(venv->Look(funDec->name_));
    std::list<frame::Access *> *args_access =
        func_entry->level_->frame_->Formals();

    auto arg_access_iter = args_access->begin();
    arg_access_iter++; /*skip static link*/

    for (auto arg_name_iter = field_list.begin();
         arg_access_iter != args_access->end() &&
         arg_name_iter != field_list.end();
         arg_access_iter++, arg_name_iter++) {
      /*For GC*/
      if (IsPointer(tenv->Look((*arg_name_iter)->typ_)))
        (*arg_access_iter)->setStorePointer();

      venv->Enter((*arg_name_iter)->name_,
                  new env::VarEntry(
                      new tr::Access(func_entry->level_, (*arg_access_iter)),
                      tenv->Look((*arg_name_iter)->typ_)->ActualTy()));
    }

    tr::ExpAndTy *body_tran = funDec->body_->Translate(
        venv, tenv, func_entry->level_, func_entry->label_, errormsg);

    venv->EndScope();
    tenv->EndScope();
    // For GC
    if (IsPointer(func_entry->result_))
      functions_ret_ptr.push_back(func_entry->label_->Name());

    tree::Stm *movToRetReg = new tree::MoveStm(
        new tree::TempExp(reg_manager->ReturnValue()), body_tran->exp_->UnEx());

    /* Add view Shift */
    tree::Stm *func_tree =
        frame::procEntryExit1(movToRetReg, func_entry->level_->frame_);

    frags->PushBack(new frame::ProcFrag(func_tree, func_entry->level_->frame_));
  }

  return new tr::ExExp(new tree::ConstExp(0));
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *init_type =
      init_->Translate(venv, tenv, level, label, errormsg);

  tr::Access *access_ = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(access_, init_type->ty_));

  // For GC
  if (IsPointer(init_type->ty_)) access_->access_->setStorePointer();

  return new tr::NxExp(new tree::MoveStm(
      access_->access_->ToExp(new tree::TempExp(reg_manager->FramePointer())),
      init_type->exp_->UnEx()));
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  std::list<NameAndTy *> nameAndTy_list = types_->GetList();
  for (const NameAndTy *nameAndTy : nameAndTy_list)
    tenv->Enter(nameAndTy->name_, new type::NameTy(nameAndTy->name_, nullptr));

  /* 满足递归定义 */
  for (const NameAndTy *nameAndTy : nameAndTy_list) {
    type::NameTy *nameAndTy_type =
        (static_cast<type::NameTy *>(tenv->Look(nameAndTy->name_)));
    type::Ty *ty_type = nameAndTy->ty_->Translate(tenv, errormsg);
    nameAndTy_type->ty_ = ty_type;
  }

  /* Modified in lab7 for GC*/
  for (const NameAndTy *nameAndTy : nameAndTy_list) {
    type::NameTy *nameAndTy_type =
        (static_cast<type::NameTy *>(tenv->Look(nameAndTy->name_)));
    if (typeid(*nameAndTy_type->ty_->ActualTy()) == typeid(type::RecordTy))
      emitRecordRecordTypeDescriptor(
          static_cast<type::RecordTy *>(nameAndTy_type->ty_),
          nameAndTy_type->sym_);
  }

  return new tr::ExExp(new tree::ConstExp(0));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  return SemAnalyze(tenv, errormsg);
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  return SemAnalyze(tenv, errormsg);
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  return SemAnalyze(tenv, errormsg);
}

/* Keep getting parent until it reached target */
tree::Exp *staticLink(tr::Level *target_, tr::Level *current_) {
  tree::Exp *static_link = new tree::TempExp(reg_manager->FramePointer());
  while (current_ != target_) {
    static_link = new tree::MemExp(
        new tree::BinopExp(tree::BinOp::PLUS_OP, static_link,
                           new tree::ConstExp(-reg_manager->WordSize())));
    current_ = current_->parent_;
  }
  return static_link;
}
}  // namespace absyn
