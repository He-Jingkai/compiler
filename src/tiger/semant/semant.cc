#include "tiger/semant/semant.h"

#include "tiger/absyn/absyn.h"

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  root_->SemAnalyze(venv, tenv, 0, errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  env::EnvEntry *entry = venv->Look(sym_);
  if (entry && typeid(*entry) == typeid(env::VarEntry))
    return (static_cast<env::VarEntry *>(entry))->ty_->ActualTy();

  errormsg->Error(pos_, "undefined variable %s", sym_->Name().c_str());
  return type::IntTy::Instance();
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *result;
  result = var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*result) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return type::IntTy::Instance();
  }
  std::list<type::Field *> fieldsList =
      (static_cast<type::RecordTy *>(result))->fields_->GetList();

  for (const type::Field *field : fieldsList)
    if (field->name_->Name() == sym_->Name()) return field->ty_;

  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().c_str());

  return type::VoidTy::Instance();
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  type::Ty *result =
      var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*result) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return type::IntTy::Instance();
  }

  return (static_cast<type::ArrayTy *>(result))->ty_->ActualTy();
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  if (typeid(*var_) == typeid(absyn::SimpleVar))
    return (static_cast<absyn::SimpleVar *>(var_))
        ->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (typeid(*var_) == typeid(absyn::FieldVar))
    return (static_cast<absyn::FieldVar *>(var_))
        ->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (typeid(*var_) == typeid(absyn::SubscriptVar))
    return (static_cast<absyn::SubscriptVar *>(var_))
        ->SemAnalyze(venv, tenv, labelcount, errormsg);

  errormsg->Error(pos_, "var type error");
  return type::IntTy::Instance();
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  env::EnvEntry *entry = venv->Look(func_);

  if (!entry || typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().c_str());
    return type::IntTy::Instance();
  }

  std::list<type::Ty *> ty_list =
      (static_cast<env::FunEntry *>(entry))->formals_->GetList();

  std::list<absyn::Exp *> exp_list = args_->GetList();

  if (ty_list.size() > exp_list.size()) {
    while (exp_list.size()) {
      type::Ty *ty = ty_list.front();
      absyn::Exp *exp = exp_list.front();
      if (!ty->IsSameType(
              exp->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy())) {
        errormsg->Error(pos_, "para type mismatch");
        return type::IntTy::Instance();
      }
      ty_list.pop_front();
      exp_list.pop_front();
    }
    errormsg->Error(pos_, "too little params in function %s",
                    func_->Name().c_str());
    return type::IntTy::Instance();
  } else if (ty_list.size() < exp_list.size()) {
    errormsg->Error(pos_, "too many params in function %s",
                    func_->Name().c_str());
    return type::IntTy::Instance();
  }

  while (ty_list.size()) {
    type::Ty *ty = ty_list.front();
    absyn::Exp *exp = exp_list.front();
    if (!ty->IsSameType(
            exp->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy())) {
      errormsg->Error(pos_, "para type mismatch");
      return type::IntTy::Instance();
    }
    ty_list.pop_front();
    exp_list.pop_front();
  }

  return (static_cast<env::FunEntry *>(entry))->result_->ActualTy();
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *left_exp_type_pointer =
      left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *right_exp_type_pointer =
      right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (oper_ == absyn::PLUS_OP || oper_ == absyn::MINUS_OP ||
      oper_ == absyn::TIMES_OP || oper_ == absyn::DIVIDE_OP) {
    if (typeid(*left_exp_type_pointer) != typeid(type::IntTy) &&
        typeid(*right_exp_type_pointer) != typeid(type::NilTy)) {
      errormsg->Error(pos_, "integer required");
      return type::IntTy::Instance();
    }
    if (typeid(*left_exp_type_pointer) != typeid(type::NilTy) &&
        typeid(*right_exp_type_pointer) != typeid(type::IntTy)) {
      errormsg->Error(pos_, "integer required");
      return type::IntTy::Instance();
    }

    return type::IntTy::Instance();
  }

  if (oper_ == absyn::LT_OP || oper_ == absyn::LE_OP || oper_ == absyn::GE_OP ||
      oper_ == absyn::GT_OP) {
    if (typeid(*left_exp_type_pointer) != typeid(type::IntTy) &&
        typeid(*left_exp_type_pointer) != typeid(type::StringTy))
      errormsg->Error(pos_, "int or string required");
    if (typeid(*right_exp_type_pointer) != typeid(type::IntTy) &&
        typeid(*right_exp_type_pointer) != typeid(type::StringTy))
      errormsg->Error(pos_, "int or string required");
    if (!right_exp_type_pointer->IsSameType(left_exp_type_pointer))
      errormsg->Error(pos_, "same type required");
    return type::IntTy::Instance();
  }

  if (oper_ == absyn::EQ_OP || oper_ == absyn::NEQ_OP) {
    if (typeid(*right_exp_type_pointer) != typeid(*left_exp_type_pointer) &&
        typeid(*right_exp_type_pointer) != typeid(type::NilTy))
      errormsg->Error(pos_, "same type required");
    return type::IntTy::Instance();
  }
  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *type_pointer = tenv->Look(typ_);

  if (!type_pointer) {
    errormsg->Error(pos_, "undefined type %s", this->typ_->Name().c_str());
    return type::VoidTy::Instance();
  }
  return type_pointer;
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  std::list<Exp *> exp_list = seq_->GetList();
  if (exp_list.size() == 0) return type::VoidTy::Instance();

  type::Ty *result;
  for (Exp *exp : exp_list)
    result = exp->SemAnalyze(venv, tenv, labelcount, errormsg);

  return result;
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *var_type_pointer =
      var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *right_exp_type_pointer =
      exp_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (!var_type_pointer->IsSameType(right_exp_type_pointer)) {
    errormsg->Error(pos_, "unmatched assign exp");
    return type::VoidTy::Instance();
  }

  if (typeid(*var_) == typeid(absyn::SimpleVar)) {
    if (venv->Look((static_cast<absyn::SimpleVar *>(var_))->sym_)->readonly_) {
      errormsg->Error(pos_, "loop variable can't be assigned");
      return type::VoidTy::Instance();
    }
  }

  return type::VoidTy::Instance();
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *then_type_pointer =
      then_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (!elsee_ && typeid(*then_type_pointer) != typeid(type::VoidTy))
    errormsg->Error(pos_, "if-then exp's body must produce no value");

  if (then_ && elsee_) {
    type::Ty *else_exp_type_pointer =
        elsee_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
    if (!then_type_pointer->IsSameType(else_exp_type_pointer))
      errormsg->Error(pos_, "then exp and else exp type mismatch");
  }

  return then_type_pointer;
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *test_type_pointer =
      test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *body_exp_type_pointer =
      body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*body_exp_type_pointer) != typeid(type::VoidTy))
    errormsg->Error(pos_, "while body must produce no value");
  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *lo_type_pointer = lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *hi_type_pointer = hi_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if ((typeid(*lo_type_pointer) != typeid(type::IntTy)) ||
      (typeid(*hi_type_pointer) != typeid(type::IntTy)))
    errormsg->Error(pos_, "for exp's range type is not integer");

  venv->Enter(var_, new env::VarEntry(lo_type_pointer, true));

  type::Ty *body_exp_type_pointer =
      body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*body_exp_type_pointer) != typeid(type::VoidTy))
    errormsg->Error(pos_, "for body must produce no value");

  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  tenv->BeginScope();
  for (const Dec *dec : decs_->GetList())
    dec->SemAnalyze(venv, tenv, labelcount, errormsg);

  type::Ty *result;
  if (!body_)
    result = type::VoidTy::Instance();
  else
    result = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*body_) == typeid(absyn::SeqExp)) {
    std::list<Exp *> exp_list_0 =
        (static_cast<absyn::SeqExp *>(body_))->seq_->GetList();
    for (absyn::Exp *exp_0 : exp_list_0) {
      if (typeid(*exp_0) == typeid(absyn::SeqExp)) {
        std::list<Exp *> exp_list_1 =
            (static_cast<absyn::SeqExp *>(exp_0))->seq_->GetList();
        for (const absyn::Exp *exp_1 : exp_list_1)
          if (typeid(*exp_1) == typeid(absyn::BreakExp))
            errormsg->Error(pos_, "break is not inside any loop");
      }
    }
  }

  tenv->EndScope();
  venv->EndScope();
  return result;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *typ_type_pointer = tenv->Look(typ_)->ActualTy();
  type::Ty *size_type_pointer =
      size_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *init_type_pointer =
      init_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (!typ_type_pointer) {
    errormsg->Error(pos_, "undifined type %s", typ_->Name().c_str());
    return type::VoidTy::Instance();
  }
  if (typeid(*typ_type_pointer) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "not array type %s", typ_->Name().c_str());
    return type::VoidTy::Instance();
  }
  if (typeid(*size_type_pointer) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "not integer");
    return type::VoidTy::Instance();
  }
  if (typeid(*init_type_pointer) !=
      typeid(*(
          (static_cast<type::ArrayTy *>(typ_type_pointer)->ty_->ActualTy())))) {
    errormsg->Error(pos_, "type mismatch");
    return type::VoidTy::Instance();
  }
  return typ_type_pointer;
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  std::list<FunDec *> funcdec_list = functions_->GetList();
  for (const FunDec *funDec : funcdec_list) {
    env::EnvEntry *fundec_entry = venv->Look(funDec->name_);
    if (fundec_entry) {
      errormsg->Error(pos_, "two functions have the same name");
      continue;
    }

    std::list<Field *> field_list = funDec->params_->GetList();
    type::TyList *param_type = new type::TyList();
    for (const Field *field : field_list) {
      type::Ty *type_temp = tenv->Look(field->typ_);
      if (!type_temp) {
        errormsg->Error(pos_, "undefined type %s", field->typ_->Name().c_str());
        continue;
      } else
        param_type->Append(type_temp);
    }

    if (!funDec->result_)
      venv->Enter(funDec->name_,
                  new env::FunEntry(param_type, type::VoidTy::Instance()));
    else
      venv->Enter(funDec->name_,
                  new env::FunEntry(param_type, tenv->Look(funDec->result_)));
  }

  for (const FunDec *funDec : funcdec_list) {
    venv->BeginScope();
    std::list<Field *> field_list = funDec->params_->GetList();

    for (const Field *field : field_list)
      venv->Enter(field->name_,
                  new env::VarEntry(tenv->Look(field->typ_), false));

    type::Ty *body_type =
        funDec->body_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

    if (!funDec->result_ && typeid(*body_type) != typeid(type::VoidTy)) {
      errormsg->Error(pos_, "procedure returns value");
      continue;
    }

    venv->EndScope();
  }
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  type::Ty *init_type =
      init_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (!typ_) {
    if (typeid(type::NilTy) == typeid(*init_type))
      errormsg->Error(pos_, "init should not be nil without type specified");
    else
      venv->Enter(var_, new env::VarEntry(init_type->ActualTy()));
  } else {
    type::Ty *typ_type = tenv->Look(typ_)->ActualTy();
    if (!init_type->IsSameType(typ_type))
      errormsg->Error(pos_, "type mismatch");

    venv->Enter(var_, new env::VarEntry(init_type->ActualTy()));
  }
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  std::list<NameAndTy *> nameAndTy_list = types_->GetList();
  for (const NameAndTy *nameAndTy : nameAndTy_list) {
    if (tenv->Look(nameAndTy->name_))
      errormsg->Error(pos_, "two types have the same name");
    tenv->Enter(nameAndTy->name_, new type::NameTy(nameAndTy->name_, nullptr));
  }

  for (const NameAndTy *nameAndTy : nameAndTy_list) {
    type::NameTy *nameAndTy_type =
        (static_cast<type::NameTy *>(tenv->Look(nameAndTy->name_)));
    type::Ty *ty_type = nameAndTy->ty_->SemAnalyze(tenv, errormsg);
    nameAndTy_type->ty_ = ty_type;
  }

  for (const NameAndTy *nameAndTy : nameAndTy_list) {
    type::NameTy *item_type =
        (static_cast<type::NameTy *>(tenv->Look(nameAndTy->name_)));
    type::NameTy *nameAndTy_type = item_type;

    while (typeid(*(nameAndTy_type->ty_)) == typeid(type::NameTy)) {
      nameAndTy_type = (static_cast<type::NameTy *>(nameAndTy_type->ty_));
      if (nameAndTy_type->sym_->Name() == item_type->sym_->Name()) {
        errormsg->Error(pos_, "illegal type cycle");
        return;
      }
    }
  }
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  type::Ty *name_type = tenv->Look(name_);
  return new type::NameTy(name_, name_type);
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  type::FieldList *fields = new type::FieldList();
  std::list<Field *> field_list = record_->GetList();
  for (const Field *field : field_list) {
    type::Ty *type_field = tenv->Look(field->typ_);
    if (!type_field) {
      errormsg->Error(pos_, "undefined type %s", field->typ_->Name().c_str());
      fields->Append(new type::Field(field->name_, type::IntTy::Instance()));
    } else
      fields->Append(new type::Field(field->name_, type_field));
  }
  return new type::RecordTy(fields);
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  type::Ty *array_type = tenv->Look(array_);
  if (!array_type) {
    errormsg->Error(pos_, "undefined type %s", array_->Name().c_str());
    return type::VoidTy::Instance();
  }
  return new type::ArrayTy(array_type);
}

}  // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}

}  // namespace sem
