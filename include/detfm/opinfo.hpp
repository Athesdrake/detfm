#pragma once
#include <abc/parser/Parser.hpp>
#include <cstdint>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace swf::abc::parser;
namespace abc = swf::abc;

enum class ErrorField { from, to, target };

class ErrorInfo;
class OpInfo;

using OpRegister = typename std::unordered_map<uint32_t, std::shared_ptr<OpInfo>>;

class OpInfo : std::enable_shared_from_this<OpInfo> {
public:
    uint32_t addr;
    std::shared_ptr<Instruction> ins;
    std::shared_ptr<OpInfo> next;
    std::vector<std::shared_ptr<OpInfo>> jumpsTo;
    std::set<OpInfo*> jumpsHere;
    std::set<std::pair<ErrorField, ErrorInfo*>> errors;

    OpInfo(std::shared_ptr<Instruction> ins, OpRegister& reg);

    void remove(Parser& parser, OpRegister& reg);
    bool removed();
};

class ErrorInfo {
public:
    swf::abc::Exception err;

    std::shared_ptr<OpInfo> from   = nullptr;
    std::shared_ptr<OpInfo> to     = nullptr;
    std::shared_ptr<OpInfo> target = nullptr;
    ErrorInfo(swf::abc::Exception& err, OpRegister& reg);

    void replace(std::shared_ptr<OpInfo>& opinfo, const ErrorField& field);
};
