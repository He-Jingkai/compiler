#include "tiger/escape/escape.h"

#include "tiger/absyn/absyn.h"

namespace esc {
void EscFinder::FindEscape() { absyn_tree_->Traverse(env_.get()); }
}  // namespace esc

namespace absyn {

void AbsynTree::Traverse(esc::EscEnvPtr env) { root_->Traverse(env, 0); }

void SimpleVar::Traverse(esc::EscEnvPtr env, int depth) {
  esc::EscapeEntry *entry = env->Look(sym_);
  if (entry->depth_ < depth) *(entry->escape_) = true;
}

void FieldVar::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
}

void SubscriptVar::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
  subscript_->Traverse(env, depth);
}

void VarExp::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
}

void NilExp::Traverse(esc::EscEnvPtr env, int depth) {}

void IntExp::Traverse(esc::EscEnvPtr env, int depth) {}

void StringExp::Traverse(esc::EscEnvPtr env, int depth) {}

void CallExp::Traverse(esc::EscEnvPtr env, int depth) {
  std::list<absyn::Exp *> args_list = args_->GetList();
  for (absyn::Exp *&exp : args_list) exp->Traverse(env, depth);
}

void OpExp::Traverse(esc::EscEnvPtr env, int depth) {
  left_->Traverse(env, depth);
  right_->Traverse(env, depth);
}

void RecordExp::Traverse(esc::EscEnvPtr env, int depth) {
  std::list<EField *> field_list = fields_->GetList();
  for (EField *&field : field_list) field->exp_->Traverse(env, depth);
}

void SeqExp::Traverse(esc::EscEnvPtr env, int depth) {
  std::list<Exp *> exp_list = seq_->GetList();
  for (absyn::Exp *&exp : exp_list) exp->Traverse(env, depth);
}

void AssignExp::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
  exp_->Traverse(env, depth);
}

void IfExp::Traverse(esc::EscEnvPtr env, int depth) {
  test_->Traverse(env, depth);
  then_->Traverse(env, depth);
  if (elsee_) elsee_->Traverse(env, depth);
}

void WhileExp::Traverse(esc::EscEnvPtr env, int depth) {
  test_->Traverse(env, depth);
  body_->Traverse(env, depth);
}

void ForExp::Traverse(esc::EscEnvPtr env, int depth) {
  escape_ = false;
  env->Enter(var_, new esc::EscapeEntry(depth, &escape_));
  lo_->Traverse(env, depth);
  hi_->Traverse(env, depth);
  body_->Traverse(env, depth);
}

void BreakExp::Traverse(esc::EscEnvPtr env, int depth) {}

void LetExp::Traverse(esc::EscEnvPtr env, int depth) {
  std::list<Dec *> dec_list = decs_->GetList();
  for (Dec *&dec : dec_list) dec->Traverse(env, depth);
  body_->Traverse(env, depth);
}

void ArrayExp::Traverse(esc::EscEnvPtr env, int depth) {
  size_->Traverse(env, depth);
  init_->Traverse(env, depth);
}

void VoidExp::Traverse(esc::EscEnvPtr env, int depth) {}

void FunctionDec::Traverse(esc::EscEnvPtr env, int depth) {
  std::list<FunDec *> fundec_list = functions_->GetList();
  for (FunDec *&fundec : fundec_list) {
    env->BeginScope();
    std::list<Field *> field_list = fundec->params_->GetList();
    for (Field *&field : field_list) {
      field->escape_ = false;
      env->Enter(field->name_,
                 new esc::EscapeEntry(depth + 1, &field->escape_));
    }
    fundec->body_->Traverse(env, depth + 1);
    env->EndScope();
  }
}

void VarDec::Traverse(esc::EscEnvPtr env, int depth) {
  escape_ = false;
  env->Enter(var_, new esc::EscapeEntry(depth, &escape_));
  init_->Traverse(env, depth);
}

void TypeDec::Traverse(esc::EscEnvPtr env, int depth) {}

}  // namespace absyn
