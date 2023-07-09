#pragma once
#include <abc/parser/Parser.hpp>
#include <abc/parser/opcodes.hpp>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <variant>

namespace athes::detfm {
using namespace swf::abc::parser;
namespace abc = swf::abc;

class StaticClass {
public:
    abc::Class* klass;
    std::unordered_map<uint32_t, abc::Trait*> slots;
    std::unordered_map<uint32_t, std::variant<int32_t, double>> methods;

    StaticClass();
    StaticClass(std::shared_ptr<abc::AbcFile>& abc, abc::Class& klass);
    bool is_slot(uint32_t index);
    bool is_slot(std::shared_ptr<Instruction>& ins);
    bool is_method(uint32_t index);
    bool is_method(std::shared_ptr<Instruction>& ins);
};

class StaticClasses {
public:
    std::unordered_map<uint32_t, StaticClass> classes;

    StaticClass& operator[](uint32_t index);
    bool is_static_class(uint32_t klass);
};
}