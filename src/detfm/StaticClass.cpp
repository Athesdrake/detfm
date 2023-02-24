#include "detfm/StaticClass.hpp"
#include "detfm/common.hpp"
#include "detfm/eval.hpp"
#include <fmt/core.h>
#include <stdexcept>

StaticClass::StaticClass() { }
StaticClass::StaticClass(std::shared_ptr<abc::AbcFile>& abc, abc::Class& klass) : klass(&klass) {
    std::unordered_map<uint32_t, abc::Trait*> notdefined;
    for (auto& trait : klass.ctraits) {
        if (trait.kind == abc::TraitKind::Slot) {
            slots[trait.name] = &trait;
            if (trait.slot.kind == 0x00)
                notdefined[trait.name] = &trait;
        } else {
            auto& method     = abc->methods[trait.index];
            auto return_type = abc::qname(abc, method.return_type);

            if (return_type == "int")
                methods[trait.name] = eval_method<int32_t>(abc, method);
            else if (return_type == "Number")
                methods[trait.name] = eval_method<double>(abc, method);
            else
                throw std::runtime_error(fmt::format("Unknown return type: {}.", return_type));
        }
    }

    // Evaluate the trait's value from the class init method
    if (!notdefined.empty()) {
        Parser parser(abc->methods[klass.cinit]);

        auto ins = parser.begin;
        while (ins) {
            if (ins->opcode == OP::findproperty && ins->next) {
                auto trait = notdefined.find(ins->args[0]);
                if (trait != notdefined.end()) {
                    ins = ins->next;
                    switch (ins->opcode) {
                    case OP::pushfalse:
                        trait->second->slot.kind = 0x0A;
                        break;
                    case OP::pushtrue:
                        trait->second->slot.kind = 0x0B;
                        break;
                    default:
                        slots.erase(trait->first);
                        break;
                    }
                    notdefined.erase(trait);
                }
            }
            ins = ins->next;
        }
    }
}
bool StaticClass::is_slot(uint32_t index) { return slots.find(index) != slots.end(); }
bool StaticClass::is_slot(std::shared_ptr<Instruction>& ins) {
    return ins->opcode == OP::getproperty && is_slot(ins->args[0]);
}
bool StaticClass::is_method(uint32_t index) { return methods.find(index) != methods.end(); }
bool StaticClass::is_method(std::shared_ptr<Instruction>& ins) {
    return ins->opcode == OP::callproperty && is_method(ins->args[0]);
}

StaticClass& StaticClasses::operator[](uint32_t index) { return classes[index]; }
bool StaticClasses::is_static_class(uint32_t klass) { return classes.find(klass) != classes.end(); }