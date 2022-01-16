#ifndef TIGER_RUNTIME_GC_ROOTS_H
#define TIGER_RUNTIME_GC_ROOTS_H

#include <iostream>
#include <map>
#include <vector>

#include "tiger/liveness/liveness.h"

namespace gc {

/* 给PointerMap输出提供的结构 */
struct PointerMap {
  std::string label;  //本pointerMap的label, 为"L"+returnAddressLabel
  std::string returnAddressLabel;
  std::string nextPointerMapLable;
  std::string frameSize;
  std::string isMain = "0";
  std::vector<std::string> offsets;
  std::string endLabel = "-1";
};

class Roots {
 public:
  Roots(assem::InstrList *il_, frame::Frame *frame_, fg::FGraphPtr flowgraph,
        std::vector<int> escapes_, temp::Map *color_)
      : escapes(escapes_),
        flowgraph_(flowgraph),
        color(color_),
        il(il_),
        frame(frame_) {}
  ~Roots() = default;

  /* 生成.data段的pointermap */
  std::vector<PointerMap> GetPointerMaps() {
    GenerateAddressLiveMap();
    GenerateTempLiveMap();
    BuildValidPointerMap();
    RewriteProgram();
    std::vector<PointerMap> pointerMaps;
    bool isMain = (frame->lable_->Name() == "tigermain");

    for (auto pair : valid_address_map) {
      PointerMap newMap;
      newMap.frameSize = frame->lable_->Name() + "_framesize";
      newMap.returnAddressLabel =
          static_cast<assem::LabelInstr *>(pair.first)->label_->Name();
      newMap.label = "L" + newMap.returnAddressLabel;
      newMap.nextPointerMapLable = "0";
      if (isMain) newMap.isMain = "1";

      newMap.offsets = std::vector<std::string>();
      for (int offset : pair.second)
        newMap.offsets.push_back(std::to_string(offset));

      pointerMaps.push_back(newMap);
    }

    for (int i = 0; i < (int)pointerMaps.size() - 1; i++)
      pointerMaps[i].nextPointerMapLable = pointerMaps[i + 1].label;

    return pointerMaps;
  }

  assem::InstrList *GetInstrList() { return il; }

 private:
  assem::InstrList *il;
  frame::Frame *frame;
  fg::FGraphPtr flowgraph_;
  std::vector<int> escapes;
  temp::Map *color;
  std::map<fg::FNodePtr, temp::TempList *> temp_in_;
  std::map<fg::FNodePtr, std::vector<int>> address_in_;
  std::map<fg::FNodePtr, std::vector<int>> address_out_;
  std::map<assem::Instr *, std::vector<int>> valid_address_map;
  std::map<assem::Instr *, std::vector<std::string>> valid_temp_map;

  std::vector<int> AddressDef(assem::Instr *ins) {  // dst
    if (typeid(*ins) == typeid(assem::OperInstr)) {
      std::string ass = static_cast<assem::OperInstr *>(ins)->assem_;
      if (ass.find("movq") != ass.npos && ass.find("_framesize") != ass.npos) {
        //确认是对本地栈的访问
        int dstStart = ass.find_first_of(',') + 2;
        //含, 跳过逗号和逗号后的空格
        std::string dst = ass.substr(dstStart);
        if (dst[0] == '(') {
          bool negative = dst.find('-') != dst.npos;
          int OffsetStart;
          if (negative)
            OffsetStart = dst.find_first_of('-') + 1;
          else
            OffsetStart = dst.find_first_of('+') + 1;
          int OffsetEnd = dst.find_first_of(')');
          std::string offset = dst.substr(OffsetStart, OffsetEnd - OffsetStart);
          if (negative)
            return std::vector<int>(1, 0 - std::stoi(offset));
          else
            return std::vector<int>(1, std::stoi(offset));
        }
      }
    }
    return std::vector<int>();
  }

  std::vector<int> AddressUse(assem::Instr *ins) {  // src
    if (typeid(*ins) == typeid(assem::OperInstr)) {
      std::string ass = static_cast<assem::OperInstr *>(ins)->assem_;
      if (ass == "") return escapes;  // escape变量的生命周期到函数末尾
      if (ass.find("movq") != ass.npos && ass.find("_framesize") != ass.npos) {
        //确认是对本地栈的访问
        int srcStart = ass.find_first_of(' ') + 1;  //含, 跳过空格
        int srcEnd = ass.find_first_of(',');        //不含
        std::string src = ass.substr(srcStart, srcEnd - srcStart);
        if (src[0] == '(') {
          bool negative = src.find('-') != src.npos;
          int OffsetStart;
          if (negative)
            OffsetStart = src.find_first_of('-') + 1;
          else
            OffsetStart = src.find_first_of('+') + 1;
          int OffsetEnd = src.find_first_of(')');
          std::string offset = src.substr(OffsetStart, OffsetEnd - OffsetStart);
          if (negative)
            return std::vector<int>(1, 0 - std::stoi(offset));
          else
            return std::vector<int>(1, std::stoi(offset));
        }
      }
    }
    return std::vector<int>();
  }

  /* 活跃的帧地址 */
  void GenerateAddressLiveMap() {
    std::list<fg::FNodePtr> flowgrapg_nodes = flowgraph_->Nodes()->GetList();
    std::map<fg::FNodePtr, std::vector<int>> pre_in, pre_out;

    for (fg::FNodePtr node_ : flowgrapg_nodes) {
      address_in_[node_] = std::vector<int>();
      address_out_[node_] = std::vector<int>();
    }

    bool changed = false;
    do {
      pre_in = address_in_;
      pre_out = address_out_;
      changed = false;
      for (fg::FNodePtr node_ : flowgrapg_nodes) {
        assem::Instr *node_ins = node_->NodeInfo();
        std::vector<int> use_list = AddressUse(node_ins);
        std::vector<int> def_list = AddressDef(node_ins);

        address_out_[node_] = std::vector<int>();
        std::list<fg::FNodePtr> succ = node_->Succ()->GetList();
        for (fg::FNodePtr succ_node : succ)
          address_out_[node_] =
              VectorUnion(address_out_[node_], address_in_[succ_node]);
        address_in_[node_] = VectorUnion(
            VectorDifference(address_out_[node_], def_list), use_list);
        //判断该点的in和out有无变化
        if (address_out_[node_] != pre_out[node_] ||
            address_in_[node_] != pre_in[node_])
          changed = true;
      }
    } while (changed);
  }

  /* 活跃的寄存器 */
  void GenerateTempLiveMap() {
    live::LiveGraphFactory *liveGraphFacPtr =
        new live::LiveGraphFactory(flowgraph_);
    liveGraphFacPtr->LiveMap();
    graph::Table<assem::Instr, temp::TempList> *in_ =
        liveGraphFacPtr->in_.get();
    std::list<fg::FNodePtr> flowgrapg_nodes = flowgraph_->Nodes()->GetList();
    for (fg::FNodePtr node_ : flowgrapg_nodes) {
      temp_in_[node_] = in_->Look(node_);
    }
  }

  /* call之后活跃的callee saved和帧地址 */
  void BuildValidPointerMap() {
    std::vector<std::string> calleeSaved = {"%r13", "%rbp", "%r12",
                                            "%rbx", "%r14", "%r15"};
    std::list<fg::FNodePtr> flowgrapg_nodes = flowgraph_->Nodes()->GetList();
    bool nextReturnLabel = false;
    for (fg::FNodePtr node_ : flowgrapg_nodes) {
      assem::Instr *ins = node_->NodeInfo();
      if (typeid(*ins) == typeid(assem::OperInstr)) {
        std::string ass = static_cast<assem::OperInstr *>(ins)->assem_;
        if (ass.find("call") != ass.npos) {
          nextReturnLabel = true;
          continue;
        }
      }
      if (nextReturnLabel) {
        nextReturnLabel = false;
        //存储指针的帧地址
        valid_address_map[ins] = address_in_[node_];
        //筛选出存储指针的callee saves寄存器
        valid_temp_map[ins] = std::vector<std::string>();
        for (auto temp : temp_in_[node_]->GetList())
          if (temp->storePointer) {
            std::string regName = *color->Look(temp);
            if (std::find(calleeSaved.begin(), calleeSaved.end(), regName) !=
                    calleeSaved.end() &&
                std::find(valid_temp_map[ins].begin(),
                          valid_temp_map[ins].end(),
                          regName) == valid_temp_map[ins].end())
              valid_temp_map[ins].push_back(regName);
            //合并的寄存器不重复放入
          }
      }
    }
  }

  /* 将callee saves寄存器中的pointer放入frame中 */
  void RewriteProgram() {
    std::list<assem::Instr *> ins_list = il->GetList();
    auto iter = ins_list.begin();
    while (iter != ins_list.end()) {
      if (typeid(*(*iter)) == typeid(assem::OperInstr)) {
        std::string ass = static_cast<assem::OperInstr *>(*iter)->assem_;
        if (ass.find("call") == ass.npos) {
          iter++;
          continue;
        }
        //下一个指令
        auto labelnode = iter;
        labelnode++;
        //将有指针的temp放入栈中(无先后顺序)

        for (std::string reg : valid_temp_map[(*labelnode)]) {
          int offset = frame->offset;
          frame::Access *access = frame->AllocLocal(true);
          access->setStorePointer();
          valid_address_map[(*labelnode)].push_back(offset);
          std::string ass = "movq " + reg + ", (" + frame->lable_->Name() +
                            "_framesize" + std::to_string(offset) + ")(%rsp)";
          assem::Instr *save_pointer =
              new assem::OperInstr(ass, nullptr, nullptr, nullptr);
          iter = ins_list.insert(iter, save_pointer);  // iter指向save_pointer
          iter++;                                      // iter指向call
        }
      }
      iter++;
    }
    il = new assem::InstrList();
    for (auto ins : ins_list) il->Append(ins);
  }

  std::vector<int> VectorUnion(std::vector<int> vec1, std::vector<int> vec2) {
    std::vector<int> result;
    std::sort(vec1.begin(), vec1.end());
    std::sort(vec2.begin(), vec2.end());
    std::set_union(vec1.begin(), vec1.end(), vec2.begin(), vec2.end(),
                   std::back_inserter(result));
    return result;
  }

  std::vector<int> VectorDifference(std::vector<int> vec1,
                                    std::vector<int> vec2) {
    std::vector<int> result;
    for (int item : vec1)
      if (std::find(vec2.begin(), vec2.end(), item) == vec2.end())
        result.push_back(item);
    return result;
  }
};

}  // namespace gc

#endif  // TIGER_RUNTIME_GC_ROOTS_H