#pragma once
#include <abc/AbcFile.hpp>
#include <abc/parser/Parser.hpp>
#include <cstdint>
#include <memory>
#include <set>

using swf::abc::parser::Instruction;
namespace abc = swf::abc;

class WrapClass {
public:
    abc::Class* klass;
    std::set<uint32_t> methods;

    WrapClass(abc::Class& klass);

    bool operator==(uint32_t& name);
    uint32_t name();

    bool is_wrap(uint32_t method);
    bool is_wrap(std::shared_ptr<Instruction>& ins);
};
bool operator==(uint32_t& name, std::unique_ptr<WrapClass>& klass);