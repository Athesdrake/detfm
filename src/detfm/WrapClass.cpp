#include "detfm/WrapClass.hpp"

using abc::parser::OP;

WrapClass::WrapClass(abc::Class& klass) : klass(&klass) {
    for (auto& trait : klass.ctraits)
        methods.insert(trait.name);
}

uint32_t WrapClass::name() { return klass->name; }

bool WrapClass::is_wrap(uint32_t method) { return methods.find(method) != methods.end(); }

bool WrapClass::is_wrap(std::shared_ptr<Instruction>& ins) {
    if (ins->opcode == OP::callproperty || ins->opcode == OP::getproperty)
        return is_wrap(ins->args[0]);
    return false;
}

bool WrapClass::operator==(uint32_t& name) { return name == klass->name; }
bool operator==(uint32_t& name, std::unique_ptr<WrapClass>& klass) {
    return klass->operator==(name);
}