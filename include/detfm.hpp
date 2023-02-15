#pragma once
#include "detfm/StaticClass.hpp"
#include "detfm/WrapClass.hpp"
#include "renamer.hpp"
#include <abc/parser/Parser.hpp>
#include <abc/parser/opcodes.hpp>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

constexpr const char* version = "0.2.0";

using swf::abc::parser::Instruction;
namespace abc = swf::abc;

class detfm {
    using MethodIterator = std::vector<abc::Method>::iterator;

public:
    uint32_t ByteArray   = 0;
    abc::Class* sent_pkt = nullptr;
    abc::Class* recv_pkt = nullptr;
    abc::Class* pkt_hdlr = nullptr;
    std::unique_ptr<WrapClass> wrap_class;
    StaticClasses static_classes;

    detfm(std::shared_ptr<abc::AbcFile>& abc, Fmt fmt);

    /* Find classes needed to unscrumble the code */
    void analyze();
    /* Unscramble bytecode by removing useless wrapper methods and resolving static slots */
    void unscramble();
    void unscramble(MethodIterator first, MethodIterator last);
    void unscramble(abc::Method& method);
    /* Rename Classes to make it easier to read */
    void rename();

private:
    bool match_sent_pkt(abc::Class& klass);
    bool match_recv_pkt(abc::Class& klass);
    bool match_wrap_class(abc::Class& klass);
    bool match_slot_class(abc::Class& klass);
    bool match_packet_handler(abc::Class& klass);
    bool match_packet_handler(abc::Trait& trait);

    /* Rename any method that is an equivalent to write*() */
    void rename_writeany();

    void find_recv_packets();
    void find_recv_packets(abc::Class& klass, uint32_t& trait_name, uint8_t& category);

    bool find_recv_tribulle(std::shared_ptr<Instruction> ins);
    void find_sent_tribulle(abc::Class& klass);

    std::optional<abc::Class> find_class_by_name(uint32_t& name);
    std::optional<abc::Trait>
    find_ctrait_by_name(abc::Class& klass, uint32_t& name, bool check_super = true);
    std::optional<abc::Trait>
    find_itrait_by_name(abc::Class& klass, uint32_t& name, bool check_super = true);
    std::optional<abc::Trait> find_trait(abc::Class& klass, uint32_t& name);

    bool get_packet_code(std::shared_ptr<Instruction>& ins, uint8_t& code);
    bool is_sequence(std::shared_ptr<Instruction> ins, const std::vector<OP>& ops);

    void set_class_ns(abc::Class& klass, uint32_t& ns);
    uint32_t create_package(std::string name);

    Fmt fmt;
    std::shared_ptr<abc::AbcFile> abc;
    std::mutex add_value_mut;
    struct {
        uint32_t pkt; // packets
        uint32_t spkt; // packets.sent
        uint32_t rpkt; // packets.recv

        uint32_t tpkt; // packets.tribulle
        uint32_t tspkt; // packets.tribulle.sent
        uint32_t trpkt; // packets.tribulle.recv

        uint32_t slot;
    } ns;
};
