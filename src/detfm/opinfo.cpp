#include "detfm/opinfo.hpp"
#include <abc/parser/Instruction.hpp>
#include <abc/parser/Parser.hpp>

OpInfo::OpInfo(std::shared_ptr<Instruction> ins) : ins(ins), addr(ins->addr) { }
bool OpInfo::removed() { return ins->next == nullptr; }
void OpInfo::remove(Parser& parser, OpRegister& reg) {
    if (removed())
        return;

    for (auto& opinfo : jumpsHere) {
        for (auto i = 0; i < opinfo->jumpsTo.size(); ++i) {
            if (opinfo->jumpsTo[i].get() == this) {
                opinfo->jumpsTo[i] = next;
                break;
            }
        }
    }

    for (auto& err : errors)
        err.second->replace(next, err.first);

    next->errors.merge(errors);
    errors.clear();

    next->jumpsHere.merge(jumpsHere);
    jumpsHere.clear();

    ins->next->prev = ins->prev;
    auto prev       = ins->prev.lock();
    if (prev) {
        prev->next            = ins->next;
        reg[prev->addr]->next = next;
    }

    // if the first instruction is removed, then parser.begin is not valid anymore
    if (ins == parser.begin)
        parser.begin = ins->next;

    // If we remove the next instruction, it will silently invalidate the iterator (linked list)
    // ins->next = nullptr;
    ins->prev = std::weak_ptr<Instruction>();
    next      = nullptr;
}

ErrorInfo::ErrorInfo(abc::Exception& err, OpRegister& reg) : err(err) {
    from   = reg[err.from];
    to     = reg[err.to];
    target = reg[err.target];

    from->errors.insert({ ErrorField::from, this });
    to->errors.insert({ ErrorField::to, this });
    target->errors.insert({ ErrorField::target, this });
}
void ErrorInfo::replace(std::shared_ptr<OpInfo>& opinfo, const ErrorField& field) {
    switch (field) {
    case ErrorField::from:
        from = opinfo;
        break;
    case ErrorField::to:
        to = opinfo;
        break;
    case ErrorField::target:
        target = opinfo;
        break;
    }
}