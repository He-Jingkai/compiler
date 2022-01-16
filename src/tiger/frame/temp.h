#ifndef TIGER_FRAME_TEMP_H_
#define TIGER_FRAME_TEMP_H_

#include <list>
#include <string_view>

#include "tiger/symbol/symbol.h"

/* lab7 GC改动说明:
 * 增加 Temp::storePointer, 用于保存storePointer是否为指针
*/

namespace temp {

using Label = sym::Symbol;

class LabelFactory {
 public:
  static Label *NewLabel();
  static Label *NamedLabel(std::string_view name);
  static std::string LabelString(Label *s);

 private:
  int label_id_ = 0;
  static LabelFactory label_factory;
};

class Temp {
  friend class TempFactory;

 public:
  [[nodiscard]] int Int() const;
// #ifdef GC
  bool storePointer;
// #endif

 private:
  int num_;
// #ifndef GC
//   explicit Temp(int num) : num_(num) {}
// #endif
// #ifdef GC
  explicit Temp(int num) : num_(num), storePointer(false) {}
// #endif
};

class TempFactory {
 public:
  static Temp *NewTemp();

 private:
  int temp_id_ = 100;
  static TempFactory temp_factory;
};

class Map {
 public:
  void Enter(Temp *t, std::string *s);
  std::string *Look(Temp *t);
  void DumpMap(FILE *out);

  static Map *Empty();
  static Map *Name();
  static Map *LayerMap(Map *over, Map *under);
  Map() : tab_(new tab::Table<Temp, std::string>()), under_(nullptr) {}

 private:
  tab::Table<Temp, std::string> *tab_;
  Map *under_;

  Map(tab::Table<Temp, std::string> *tab, Map *under)
      : tab_(tab), under_(under) {}
};

class TempList {
 public:
  explicit TempList(Temp *t) : temp_list_({t}) {}
  TempList(std::initializer_list<Temp *> list) : temp_list_(list) {}
  TempList() = default;
  void Append(Temp *t) { temp_list_.push_back(t); }
  [[nodiscard]] Temp *NthTemp(int i) const;
  [[nodiscard]] const std::list<Temp *> &GetList() const { return temp_list_; }
  bool replaceTempFromTempList(Temp *from, Temp *to) {
    bool in = std::find(temp_list_.begin(), temp_list_.end(), from) !=
              temp_list_.end();
    if (!in) return false;
    auto first_pos = std::find(temp_list_.begin(), temp_list_.end(), from);
    while (first_pos != temp_list_.end()) {
      *first_pos = to;
      first_pos = std::find(temp_list_.begin(), temp_list_.end(), from);
    }
    return true;
  }

 private:
  std::list<Temp *> temp_list_;
};

}  // namespace temp

#endif