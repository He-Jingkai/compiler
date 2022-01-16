#include "straightline/slp.h"

namespace A {
int A::CompoundStm::MaxArgs() const {
  return (stm1->MaxArgs() > stm2->MaxArgs()) ? stm1->MaxArgs()
                                             : stm2->MaxArgs();
}

Table *A::CompoundStm::Interp(Table *t) const {
  return stm2->Interp(stm1->Interp(t));
}

int A::AssignStm::MaxArgs() const { return exp->MaxArgs(); }

Table *A::AssignStm::Interp(Table *t) const {
  IntAndTable *temp = exp->Interp(t);
  return temp->t->Update(id, temp->i);
}

int A::PrintStm::MaxArgs() const {
  return exps->NumExps() > exps->MaxArgs() ? exps->NumExps() : exps->MaxArgs();
}

Table *A::PrintStm::Interp(Table *t) const {
  ExpList *temp = exps;
  IntAndTable *tempIntAndTable = NULL;
  do {
    tempIntAndTable = temp->Interp(t);
    std::cout << tempIntAndTable->i << " ";
    t = tempIntAndTable->t;
  } while ((temp = temp->RemoveFirstExp()) != NULL);
  std::cout << "\n";
  return tempIntAndTable->t;
}

int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}
Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}

IntAndTable *IdExp::Interp(Table *t) const {
  return new IntAndTable(t->Lookup(id), t);
}
int IdExp::MaxArgs() const { return 0; }

IntAndTable *NumExp::Interp(Table *t) const { return new IntAndTable(num, t); }
int NumExp::MaxArgs() const { return 0; }

IntAndTable *OpExp::Interp(Table *t) const {
  IntAndTable *leftIntAndTable = left->Interp(t);
  IntAndTable *rightIntAndTable = right->Interp(leftIntAndTable->t);
  int temp = leftIntAndTable->i;

  switch (oper) {
    case PLUS: {
      temp += rightIntAndTable->i;
      break;
    }
    case MINUS: {
      temp -= rightIntAndTable->i;
      break;
    }
    case TIMES: {
      temp *= rightIntAndTable->i;
      break;
    }
    case DIV: {
      temp /= rightIntAndTable->i;
      break;
    }
  }
  return new IntAndTable(temp, rightIntAndTable->t);
}
int OpExp::MaxArgs() const {
  return (left->MaxArgs() > right->MaxArgs()) ? left->MaxArgs()
                                              : right->MaxArgs();
}

IntAndTable *EseqExp::Interp(Table *t) const {
  Table *temp = stm->Interp(t);
  return new IntAndTable(exp->Interp(temp)->i, exp->Interp(temp)->t);
}
int EseqExp::MaxArgs() const {
  return (stm->MaxArgs() > exp->MaxArgs()) ? stm->MaxArgs() : exp->MaxArgs();
}

int PairExpList::NumExps() const { return 1 + tail->NumExps(); }
int PairExpList::MaxArgs() const {
  return exp->MaxArgs() > tail->MaxArgs() ? exp->MaxArgs() : tail->MaxArgs();
}
IntAndTable *PairExpList::Interp(Table *t) const {
  IntAndTable *temp = exp->Interp(t);
  return temp;
}
ExpList *PairExpList::RemoveFirstExp() const { return tail; }

int LastExpList::NumExps() const { return 1; }
int LastExpList::MaxArgs() const { return exp->MaxArgs(); }
IntAndTable *LastExpList::Interp(Table *t) const {
  IntAndTable *temp = exp->Interp(t);
  return temp;
}
ExpList *LastExpList::RemoveFirstExp() const { return NULL; }

}  // namespace A
