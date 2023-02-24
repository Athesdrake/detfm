#include "detfm/simplify.hpp"
#include "detfm/common.hpp"
#include "detfm/opinfo.hpp"
#include <abc/AbcFile.hpp>
#include <abc/parser/Parser.hpp>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <stack>
#include <stdexcept>
#include <unordered_map>
#include <variant>
#include <vector>

namespace abc = swf::abc;
using namespace abc::parser;

using StackValue = std::variant<std::monostate, bool, double, std::string>;
using Operation  = std::function<StackValue(const StackValue&, const StackValue&)>;

std::vector<std::unordered_map<OP, Operation>> operations
    = { {}, // monostate
        {}, // bool
        {
            // double
            { OP::add, [](auto& a, auto& b) { return std::get<2>(b) + std::get<2>(a); } },
            { OP::subtract, [](auto& a, auto& b) { return std::get<2>(b) - std::get<2>(a); } },
            { OP::multiply, [](auto& a, auto& b) { return std::get<2>(b) * std::get<2>(a); } },
            { OP::divide, [](auto& a, auto& b) { return std::get<2>(b) / std::get<2>(a); } },
        },
        {
            // string
            { OP::add, [](auto& a, auto& b) { return std::get<3>(b) + std::get<3>(a); } },
        } };

std::unordered_map<OP, std::array<uint32_t, 2>> stack_operations = {
    // { opcode, {take from stack, put on stack}}
    // stack indenpendent
    { OP::jump, {} },
    { OP::kill, {} },
    { OP::popscope, {} },
    { OP::returnvoid, {} },
    { OP::returnvoid, {} },

    // add to the stack
    { OP::getlocal0, { 0, 1 } },
    { OP::getlex, { 0, 1 } },
    { OP::pushnull, { 0, 1 } },
    { OP::findproperty, { 0, 1 } },
    { OP::findpropstrict, { 0, 1 } },
    { OP::newarray, { 0, 1 } },
    { OP::newfunction, { 0, 1 } },
    { OP::newcatch, { 0, 2 } }, // should be 1, but also add the error to the stack

    // take from the stack
    { OP::setlocal1, { 1, 0 } },
    { OP::pushscope, { 1, 0 } },
    { OP::pop, { 1, 0 } },
    { OP::callpropvoid, { 1, 0 } },
    { OP::iffalse, { 1, 0 } },
    { OP::iftrue, { 1, 0 } },
    { OP::ifeq, { 2, 0 } },
    { OP::setproperty, { 2, 0 } },
    { OP::initproperty, { 2, 0 } },
    { OP::setslot, { 2, 0 } },

    // hybrid
    { OP::getproperty, { 1, 1 } },
    { OP::constructprop, { 1, 1 } },
    { OP::construct, { 1, 1 } },
    { OP::applytype, { 1, 1 } },
    { OP::callproperty, { 1, 1 } },
    { OP::equals, { 2, 1 } },
};

std::unordered_map<OP, uint32_t> dynamic_stack_op = {
    // ins->args[0]
    { OP::construct, 0 },
    { OP::applytype, 0 },
    { OP::newarray, 0 },
    // ins->args[1]
    { OP::constructprop, 1 },
    { OP::callproperty, 1 },
    { OP::callpropvoid, 1 },
};

template <typename T> T pop_value(std::stack<T>& stack) {
    auto v = std::move(stack.top());
    stack.pop();
    return v;
}

void edit_ins(
    std::shared_ptr<abc::AbcFile>& abc, Parser& parser, OpRegister& insreg,
    std::stack<StackValue> stack, std::shared_ptr<OpInfo>& opinfo, uint32_t ins2remove) {
    for (uint32_t i = 0; i < ins2remove; ++i)
        insreg[opinfo->ins->prev.lock()->addr]->remove(parser, insreg);

    if (std::holds_alternative<double>(stack.top())) {
        const auto& value = std::get<double>(stack.top());
        if (std::fmod(value, 1) != 0 || std::abs(value) > 0x8000) {
            uint32_t index = abc->cpool.doubles.size();
            abc->cpool.doubles.push_back(value);
            opinfo->ins->opcode = OP::pushdouble;
            opinfo->ins->args   = { index };
        } else {
            opinfo->ins->opcode = std::abs(value) > 0x80 ? OP::pushshort : OP::pushbyte;
            opinfo->ins->args   = { static_cast<uint32_t>(value) };
        }
    } else if (std::holds_alternative<std::string>(stack.top())) {
        uint32_t index = abc->cpool.strings.size();
        abc->cpool.strings.push_back(std::get<std::string>(stack.top()));
        opinfo->ins->opcode = OP::pushstring;
        opinfo->ins->args   = { index };
    } else if (std::holds_alternative<bool>(stack.top())) {
        opinfo->ins->opcode = std::get<bool>(stack.top()) ? OP::pushtrue : OP::pushfalse;
        opinfo->ins->args   = {};
    }
}

void simplify_expressions(std::shared_ptr<abc::AbcFile>& abc, abc::Method& method) {
    Parser parser(method);
    std::stack<StackValue> stack;

    OpRegister insreg;
    // populate the instruction register
    auto ins  = parser.begin;
    auto prev = insreg[ins->addr] = std::make_shared<OpInfo>(ins);
    while (ins = ins->next) {
        insreg[ins->addr] = std::make_shared<OpInfo>(ins);
        prev = prev->next = insreg[ins->addr];
    }
    ins = parser.begin;
    while (ins) {
        auto opinfo = insreg[ins->addr];

        if (ins->isJump()) {
            for (uint32_t offset : ins->args) {
                auto it = insreg.find(offset);

                // Invalid jump; jump to next instruction instead
                if (it == insreg.end())
                    offset = ins->next->addr;

                auto target = insreg[offset];
                opinfo->jumpsTo.push_back(target);
                target->jumpsHere.insert(opinfo.get());
            }
        }
        ins = ins->next;
    }
    ins = parser.begin;

    bool modified = false;
    while (ins) {
        auto opinfo = insreg[ins->addr];
        switch (ins->opcode) {
        case OP::pushbyte:
            stack.push(static_cast<double>(static_cast<int8_t>(ins->args[0])));
            break;
        case OP::pushshort:
            stack.push(static_cast<double>(static_cast<int32_t>(ins->args[0])));
            break;
        case OP::pushint:
            stack.push(static_cast<double>(abc->cpool.integers[ins->args[0]]));
            break;
        case OP::pushuint:
            stack.push(static_cast<double>(abc->cpool.uintegers[ins->args[0]]));
            break;
        case OP::pushdouble:
            stack.push(abc->cpool.doubles[ins->args[0]]);
            break;
        case OP::pushtrue:
        case OP::pushfalse:
            stack.push(ins->opcode == OP::pushtrue);
            break;
        case OP::pushstring:
            stack.push(abc->cpool.strings[ins->args[0]]);
            break;
        case OP::dup:
            stack.push(stack.top());
            break;
        case OP::swap: {
            auto tmp = stack.top();
            stack.pop();
            if (!stack.empty())
                std::swap(stack.top(), tmp);
            stack.push(tmp);
            break;
        }
        case OP::OP_not: {
            if (std::holds_alternative<bool>(stack.top())) {
                stack.push(std::monostate());
                break;
            }
            stack.top() = !std::get<bool>(stack.top());
            break;
        }
        case OP::subtract:
        case OP::multiply:
        case OP::divide:
        case OP::add: {
            auto a = pop_value(stack), b = pop_value(stack);
            auto& ops = operations[a.index()];
            if (a.index() != b.index() || a.index() == 0 || ops.find(ins->opcode) == ops.end()) {
                // Not the same type, one is monostate, or the operation is not registered
                // Don't bother trying
                stack.push(std::monostate());
                break;
            }
            modified = true;
            stack.push(ops.at(ins->opcode)(a, b));
            edit_ins(abc, parser, insreg, stack, opinfo, 2);
            break;
        }
        case OP::negate: {
            if (!std::holds_alternative<double>(stack.top())) {
                modified    = true;
                stack.top() = -std::get<double>(stack.top());
                edit_ins(abc, parser, insreg, stack, opinfo, 1);
            }
            break;
        }
        default: {
            if (stack_operations.find(ins->opcode) == stack_operations.end())
                throw std::runtime_error(
                    std::string("Unsupported operation.") + opnames[int(ins->opcode)]);

            if (dynamic_stack_op.find(ins->opcode) != dynamic_stack_op.end()) {
                for (uint32_t i = 0; i < ins->args[dynamic_stack_op.at(ins->opcode)]; ++i)
                    stack.pop();
            }

            auto& [take, put] = stack_operations.at(ins->opcode);
            for (uint32_t i = 0; i < take; ++i)
                stack.pop();

            for (uint32_t i = 0; i < put; ++i)
                stack.push(std::monostate());

            break;
        }
        }
        ins = ins->next;
    }

    if (modified) {
        uint32_t pos = 0;

        // re-compute the instructions position
        ins = parser.begin;
        while (ins) {
            auto opinfo  = insreg[ins->addr];
            opinfo->addr = pos;
            pos += ins->size();
            ins = ins->next;
        }

        ins = parser.begin;
        while (ins) {
            auto opinfo = insreg[ins->addr];

            if (ins->isJump())
                for (auto i = 0; i < ins->args.size(); ++i)
                    ins->args[i] = opinfo->jumpsTo[i]->addr;

            ins = ins->next;
        }

        swf::StreamWriter stream;
        ins = parser.begin;
        while (ins) {
            ins->write(stream);
            ins = ins->next;
        }
        method.code.clear();
        method.code.insert(
            method.code.end(), stream.get_buffer(), stream.get_buffer() + stream.size());
    }
}