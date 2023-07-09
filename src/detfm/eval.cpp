#include "detfm/eval.hpp"
#include <abc/AbcFile.hpp>
#include <abc/info/ConstantPool.hpp>
#include <abc/parser/Instruction.hpp>
#include <abc/parser/Parser.hpp>
#include <abc/parser/opcodes.hpp>
#include <memory>
#include <stack>
#include <stdexcept>
#include <stdint.h>
#include <vector>

namespace athes::detfm {
namespace abc = swf::abc;
using namespace abc::parser;

template <typename T> T& pop(std::stack<T>& stack) {
    auto& value = stack.top();
    stack.pop();
    return value;
}
template <typename T> T add(std::stack<T>& stack) {
    auto& a = pop(stack);
    auto& b = pop(stack);
    return a + b;
}
template <typename T> T divise(std::stack<T>& stack) {
    auto& a = pop(stack);
    auto& b = pop(stack);
    return a / b;
}
template <typename T> T eval_method(std::shared_ptr<abc::AbcFile>& abc, abc::Method& method) {
    Parser parser(method);
    std::stack<T> stack;

    auto ins = parser.begin;
    while (ins) {
        switch (ins->opcode) {
        case OP::getlocal0:
        case OP::pushscope:
            break;
        case OP::pushbyte:
            stack.push(static_cast<T>(static_cast<uint8_t>(ins->args[0])));
            break;
        case OP::pushshort:
            stack.push(static_cast<T>(static_cast<int32_t>(ins->args[0])));
            break;
        case OP::pushint:
            stack.push(static_cast<T>(abc->cpool.integers[ins->args[0]]));
            break;
        case OP::add:
            stack.push(add(stack));
            break;
        case OP::divide:
            stack.push(divise(stack));
            break;
        case OP::returnvalue:
            return stack.top();
        default:
            throw std::runtime_error("Unsupported operation.");
            break;
        }
        ins = ins->next;
    }

    throw std::runtime_error("Nothing to return.");
}

template int eval_method<int>(std::shared_ptr<abc::AbcFile>& abc, abc::Method& method);
template double eval_method<double>(std::shared_ptr<abc::AbcFile>& abc, abc::Method& method);
}