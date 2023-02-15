#include "detfm.hpp"
#include "detfm/common.hpp"
#include "detfm/opinfo.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cstdio>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <stdexcept>
#include <utility>

static const std::vector<OP> new_class_seq = {
    OP::findpropstrict,
    OP::getlocal1,
    OP::constructprop,
};
static const std::vector<OP> sub_handler_seq = {
    OP::getlex, OP::getlocal1, OP::getlex, OP::getproperty, OP::callpropvoid, OP::returnvoid,
};
static const std::vector<OP> tribulle_pkt_getter_seq = {
    OP::getlex, OP::getlex, OP::getproperty, OP::callproperty, OP::coerce,
};
static const std::vector<OP> tribulle_pkt_return_id_seq = {
    OP::label,
    OP::pushdouble,
    OP::returnvalue,
};

bool is_qname(abc::Multiname& mn) {
    return mn.kind == abc::MultinameKind::QName || mn.kind == abc::MultinameKind::QNameA;
}

/** @brief Skip instructions until the specified opcode is reached.
 *  @return true upon success, false if it reached end of code.
 */
bool skip_to_opcode(std::shared_ptr<Instruction>& ins, OP opcode) {
    while (ins && ins->opcode != opcode)
        ins = ins->next;

    return ins != nullptr;
}

detfm::detfm(std::shared_ptr<abc::AbcFile>& abc, Fmt fmt, utils::Logger logger)
    : abc(abc), fmt(fmt), logger(logger) { }

void detfm::analyze() {
    for (uint32_t i = 0; i < abc->cpool.multinames.size(); ++i) {
        auto& mn = abc->cpool.multinames[i];
        if (mn.kind == abc::MultinameKind::QName && abc::str(abc, mn) == "ByteArray") {
            ByteArray = i;
            break;
        }
    }

    for (auto& klass : abc->classes) {
        if (match_sent_pkt(klass)) {
            sent_pkt = &klass;
        } else if (match_recv_pkt(klass)) {
            recv_pkt = &klass;
        } else if (match_wrap_class(klass)) {
            wrap_class = std::make_unique<WrapClass>(klass);
        } else if (match_slot_class(klass)) {
            static_classes.classes.try_emplace(klass.name, abc, klass);
        } else if (match_packet_handler(klass)) {
            pkt_hdlr = &klass;
        }
    }

    std::vector<std::string> missings;
    if (ByteArray == 0)
        missings.push_back("ByteArray Multiname");

    if (sent_pkt == nullptr)
        missings.push_back("Send Base Packet");

    if (recv_pkt == nullptr)
        missings.push_back("Receive Base Packet");

    if (wrap_class == nullptr)
        missings.push_back("Wrapper Class");

    if (pkt_hdlr == nullptr)
        missings.push_back("Packet Handler Class");

    if (static_classes.classes.empty())
        missings.push_back("Static Classes");

    if (!missings.empty())
        throw std::runtime_error(fmt::format("Some classes are missing: {}", missings));
}

void detfm::unscramble() {
    for (auto& method : abc->methods)
        unscramble(method);
}
void detfm::unscramble(MethodIterator first, MethodIterator last) {
    for (auto it = first; it != last; ++it)
        unscramble(*it);
}
void detfm::unscramble(abc::Method& method) {
    if (method.code.empty())
        return;

    Parser parser(method);
    std::unordered_map<uint32_t, std::shared_ptr<Instruction>> ops;
    std::vector<ErrorInfo> exceptions;
    OpRegister insreg;

    const auto is_call = [](std::shared_ptr<Instruction>& ins) {
        return ins->opcode == OP::call || ins->opcode == OP::getglobalscope;
    };

    auto ins             = parser.begin;
    bool modified        = false;
    int remove_next_call = 0;

    // populate the instruction register
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

    for (auto& err : method.exceptions)
        exceptions.emplace_back(err, insreg);

    ins = parser.begin;
    while (ins) {
        auto opinfo = insreg[ins->addr];

        if (wrap_class->is_wrap(ins)) {
            opinfo->remove(parser, insreg);

            if (ins->opcode == OP::getproperty)
                ++remove_next_call;

            modified = true;
        } else if (remove_next_call > 0 && is_call(ins)) {
            remove_next_call -= ins->opcode == OP::call;
            opinfo->remove(parser, insreg);
        } else if (ins->opcode == OP::getlex) {
            if (static_classes.is_static_class(ins->args[0])) {
                auto klass   = static_classes[ins->args[0]];
                auto lastins = ins;
                auto lastop  = opinfo;

                ins    = ins->next;
                opinfo = insreg[ins->addr];
                if (klass.is_slot(ins)) {
                    auto trait = klass.slots[ins->args[0]];
                    opinfo->ins->opcode
                        = trait->slot.kind == 0x01 ? OP::pushstring : OP::pushdouble;
                    opinfo->ins->args = { trait->index };
                    lastop->remove(parser, insreg);

                    modified = true;
                } else if (klass.is_method(ins)) {
                    // Make it thread-safe 🤓 thanks to this little guy 💂
                    std::lock_guard<std::mutex> guard(add_value_mut);
                    auto value          = klass.methods[ins->args[0]];
                    bool is_double      = std::holds_alternative<double>(value);
                    opinfo->ins->opcode = is_double ? OP::pushdouble : OP::pushint;
                    opinfo->ins->args   = { static_cast<uint32_t>(
                        is_double ? abc->cpool.doubles.size() : abc->cpool.integers.size()) };
                    lastop->remove(parser, insreg);
                    if (is_double)
                        abc->cpool.doubles.push_back(std::get<double>(value));
                    else
                        abc->cpool.integers.push_back(std::get<int32_t>(value));

                    modified = true;
                } else {
                    ins = lastins;
                }
            } else if (ins->args[0] == wrap_class->name()) {
                opinfo->remove(parser, insreg);
                modified = true;
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

        for (auto i = 0; i < method.exceptions.size(); ++i) {
            method.exceptions[i].from   = exceptions[i].from->addr;
            method.exceptions[i].to     = exceptions[i].to->addr;
            method.exceptions[i].target = exceptions[i].target->addr;
        }
    }
}

void detfm::rename() {
    ns.slot  = create_package("com.obfuscate");
    ns.pkt   = create_package("packets");
    ns.spkt  = create_package("packets.sent");
    ns.rpkt  = create_package("packets.recv");
    ns.tpkt  = create_package("packets.tribulle");
    ns.tspkt = create_package("packets.tribulle.sent");
    ns.trpkt = create_package("packets.tribulle.recv");

    set_class_ns(*sent_pkt, ns.pkt);
    set_class_ns(*recv_pkt, ns.pkt);

    sent_pkt->rename("SPacketBase");
    sent_pkt->itraits[2].rename("pcode");
    rename_writeany();

    recv_pkt->rename("RPacketBase");
    recv_pkt->itraits[0].rename("pcode0");
    recv_pkt->itraits[1].rename("pcode1");
    recv_pkt->itraits[2].rename("buffer");

    auto spkt_name    = sent_pkt->get_name();
    auto rpkt_name    = recv_pkt->get_name();
    auto recv_counter = 0;
    for (auto& klass : abc->classes) {
        if (klass.super_name == 0)
            continue;

        auto super_name = klass.get_super_name();
        if (super_name == spkt_name) {
            Parser parser(abc->methods[klass.iinit]);
            uint32_t pcode = 0;

            auto ins = parser.begin;
            // Find the Packet's code
            while (ins && ins->opcode != OP::constructsuper) {
                if (ins->opcode == OP::pushdouble)
                    pcode = pcode << 8 | static_cast<uint32_t>(abc->cpool.doubles[ins->args[0]]);

                ins = ins->next;
            }
            klass.rename(fmt.sent_packet.format(pcode >> 8, pcode & 0xff));

            set_class_ns(klass, ns.spkt);
        } else if (super_name == rpkt_name) {
            klass.rename(fmt.unknown_recv_packet.format(++recv_counter));
            set_class_ns(klass, ns.rpkt);
        }
    }

    auto counter = 0;
    for (auto& it : static_classes.classes) {
        auto& klass = *it.second.klass;
        klass.ctraits.clear();
        klass.itraits.clear();
        klass.rename(fmt::format("$StaticClass_{:02d}", ++counter));
        set_class_ns(klass, ns.slot);
        abc->methods[klass.cinit].code.clear();
    }
    auto& klass = *wrap_class->klass;
    klass.ctraits.clear();
    klass.itraits.clear();
    klass.rename("$WrapperClass");
    set_class_ns(klass, ns.slot);

    find_recv_packets();
}

void detfm::find_recv_packets() {
    const auto predicate = [this](auto& t) { return match_packet_handler(t); };
    const auto trait = std::find_if(pkt_hdlr->ctraits.begin(), pkt_hdlr->ctraits.end(), predicate);
    auto& method     = abc->methods[trait->index];
    pkt_hdlr->rename("PacketHandler");
    trait->rename("handle_packet");
    set_class_ns(*pkt_hdlr, ns.pkt);

    Parser parser(method);
    auto ins = parser.begin;
    uint8_t category, code;
    while (ins) {
        category = code = 0;
        if (ins->opcode == OP::getlex && ins->args[0] == pkt_hdlr->name) {
            if (get_packet_code(ins, category)) {
                auto target = ins->targets[0].lock();
                auto found  = false;

                ins = ins->next;
                if (ins->opcode == OP::pushdouble)
                    ins = ins->next;

                while (get_packet_code(ins, code)) {
                    auto codetarget = ins->targets[0].lock();

                    // Handle the tribulle packets
                    if (category == 0x3c && code == 0x03) {
                        found = find_recv_tribulle(ins);
                        break;
                    }

                    while (ins && ins->opcode != OP::returnvoid) {
                        if (is_sequence(ins, new_class_seq)) {
                            auto klass = find_class_by_name(ins->args[0]);

                            if (klass) {
                                klass->rename(fmt.recv_packet.format(category, code));
                                set_class_ns(*klass, ns.rpkt);
                            }
                            break;
                        }
                        ins = ins->next;
                    }

                    ins   = codetarget;
                    found = true;
                    if (ins->addr == target->addr)
                        break;

                    if (ins->opcode == OP::pushdouble)
                        ins = ins->next;
                }

                if (!found && is_sequence(ins, sub_handler_seq)
                    && ins->next->next->args[0] == pkt_hdlr->name) {
                    auto handler = find_class_by_name(ins->args[0]);

                    if (handler) {
                        logger.info("Found sub handler ({})\n", handler->get_name());
                        find_recv_packets(*handler, trait->name, category);
                    }
                }
                ins = target->prev.lock();
            }
        }
        ins = ins->next;
    }
}
void detfm::find_recv_packets(abc::Class& klass, uint32_t& trait_name, uint8_t& category) {
    auto trait   = find_ctrait_by_name(klass, trait_name);
    auto& method = abc->methods[trait->index];
    uint8_t code = 0;
    klass.rename(fmt.packet_subhandler.format(category));
    set_class_ns(klass, ns.pkt);

    Parser parser(method);
    auto ins = parser.begin;
    while (ins) {
        if (ins->opcode == OP::getlocal2) {
            auto tmp = ins->next->opcode == OP::pushdouble ? ins = ins->next : ins->prev.lock();

            if (tmp->opcode == OP::pushdouble) {
                code = static_cast<uint8_t>(abc->cpool.doubles[tmp->args[0]]);
                if (ins->next->opcode == OP::ifne) {
                    auto target = ins->next->targets[0].lock();

                    ins = ins->next->next;
                    while (ins && ins->opcode != OP::returnvoid) {
                        if (is_sequence(ins, new_class_seq)) {
                            auto klass = find_class_by_name(ins->args[0]);
                            if (klass) {
                                klass->rename(fmt.recv_packet.format(category, code));
                                set_class_ns(*klass, ns.rpkt);
                            }
                            break;
                        }

                        if (ins->addr == target->addr)
                            break;

                        ins = ins->next;
                    }
                    ins = target->prev.lock();
                }
            }
        }
        ins = ins->next;
    }
}

bool detfm::find_recv_tribulle(std::shared_ptr<Instruction> ins) {
    std::optional<abc::Class> klass;

    while (ins && ins->opcode != OP::returnvoid && !is_sequence(ins, tribulle_pkt_getter_seq))
        ins = ins->next;

    // Safety check
    if (ins->opcode == OP::returnvoid || !(klass = find_class_by_name(ins->args[0])))
        return false;

    auto callprop = ins->next->next->next;
    auto trait    = find_ctrait_by_name(*klass, callprop->args[0]);

    // Make sure we have a method!
    if (!trait || trait->kind != abc::TraitKind::Method)
        return false;

    // This method calls another one, which contains the code we want
    Parser parser(abc->methods[trait->index]);
    ins = parser.begin;

    // First we have a getlex
    if (!skip_to_opcode(ins, OP::getlex) || !(klass = find_class_by_name(ins->args[0])))
        return false;

    ins = ins->next;
    // then we should have 2 getproperty
    while (ins && ins->opcode == OP::getproperty) {
        auto name = abc::str(abc, ins->args[0]);
        // Make sure we get a slot trait with a specified type
        if (!(trait = find_trait(*klass, ins->args[0])) || trait->kind != abc::TraitKind::Slot
            || trait->slot.type == 0)
            return false;

        // Get the trait's type, so we can resolve later traits
        if (!(klass = find_class_by_name(trait->slot.type)))
            return false;

        ins = ins->next;
    }

    // followed by some args and a callproperty
    if (!skip_to_opcode(ins, OP::callproperty))
        return false;

    // Get the class & method
    auto name = ins->args[0];
    while (klass && !(trait = find_itrait_by_name(*klass, name, false)) && klass->super_name)
        klass = find_class_by_name(klass->super_name);

    if (!klass || !trait || trait->kind != abc::TraitKind::Method)
        return false;

    // The same class has several interesting stuff
    find_sent_tribulle(*klass);

    // found the magic method, we need to do the get_packet_code thing again!
    // but first let's rename the base packet
    auto& method = abc->methods[trait->index];
    if (klass = find_class_by_name(method.return_type)) {
        set_class_ns(*klass, ns.tpkt);
        klass->rename("TRPacketBase");
    }

    parser = Parser(method);
    ins    = parser.begin;
    if (!ins)
        return false;

    // similar to get_packet_code, but much simpler
    // it's always `local2 == <pushdouble>` (or inversed)
    // so we only need to find the pushdouble
    uint16_t code;
    do {
        if (ins->opcode != OP::pushdouble)
            continue;

        code = abc->cpool.doubles[ins->args[0]];
        // find the next findpropstrict, that's the class we need to rename!
        while (ins && ins->opcode != OP::findpropstrict)
            ins = ins->next;

        if (!ins || !(klass = find_class_by_name(ins->args[0])))
            continue; // should we return false?

        // rename it!
        set_class_ns(*klass, ns.trpkt);
        klass->rename(fmt.tribulle_recv_packet.format(code));
    } while (ins = ins->next);

    return true;
}

void detfm::find_sent_tribulle(abc::Class& klass) {
    // First we can get the Tribulle aka Community Platform version
    Parser parser(abc->methods[klass.iinit]);
    auto ins = parser.begin;

    // Skip it if we don't find it, that's not the end of the world
    if (skip_to_opcode(ins, OP::pushstring)) {
        auto version = 'v' + abc->cpool.strings[ins->args[0]];
        logger.info(
            "Found Tribulle {}\n",
            fmt::styled(version, fmt::emphasis::italic | fmt::fg(fmt::color::orchid)));
    } else {
        logger.warn("Tribulle version not found.\n");
    }

    // this class has a method called "getIdPaquet" which takes one param,
    // and return its corresponding id. The function check the param's type using `istypelate`
    // So we can get the class from its id and rename it.
    std::optional<abc::Method> method;

    // don't search the trait from its name
    for (auto& trait : klass.itraits) {
        if (trait.kind != abc::TraitKind::Method)
            continue;

        auto& meth = abc->methods[trait.index];
        if (meth.params.size() != 1 || abc::qname(abc, meth.return_type) != "int")
            continue;

        method = meth;
        trait.rename("getPacketId");
        break;
    }

    if (!method)
        return;

    parser = Parser(*method);
    ins    = parser.begin;

    std::unordered_map<uint32_t, uint16_t> addr2id;
    std::unordered_map<uint32_t, uint32_t> index2name;
    while (ins && !is_sequence(ins, tribulle_pkt_return_id_seq))
        ins = ins->next;

    while (ins && is_sequence(ins, tribulle_pkt_return_id_seq)) {
        addr2id[ins->addr] = static_cast<uint16_t>(abc->cpool.doubles[ins->next->args[0]]);
        // Skip the sequence 💩
        ins = ins->next->next->next;
    }

    // find the getlex's and the index used in the lookupswitch
    while (ins) {
        uint32_t name;
        if (skip_to_opcode(ins, OP::getlex))
            name = ins->args[0];

        if (skip_to_opcode(ins, OP::pushbyte)) {
            index2name[ins->args[0]] = name;

            if (!is_sequence(ins->next, { OP::jump, OP::getlocal1 }))
                break;
        }
    }

    if (!skip_to_opcode(ins, OP::lookupswitch))
        return;

    const auto targets_count = ins->targets.size();
    std::optional<abc::Class> cls;
    for (auto it : index2name) {
        if (!(cls = find_class_by_name(it.second)) || it.first >= targets_count)
            continue;

        auto addr = ins->targets[it.first + 1].lock()->addr;
        if (addr2id.find(addr) == addr2id.end())
            continue;

        set_class_ns(*cls, ns.tspkt);
        cls->rename(fmt.tribulle_sent_packet.format(addr2id[addr]));
    }
}

std::optional<abc::Class> detfm::find_class_by_name(uint32_t& name) {
    auto klass = std::find_if(abc->classes.begin(), abc->classes.end(), [name](abc::Class& cls) {
        return cls.name == name;
    });

    if (klass != abc->classes.end())
        return *klass;

    return {};
}
std::optional<abc::Trait>
detfm::find_ctrait_by_name(abc::Class& klass, uint32_t& name, bool check_super) {
    auto trait = std::find_if(klass.ctraits.begin(), klass.ctraits.end(), [name](abc::Trait& t) {
        return t.name == name;
    });

    if (trait != klass.ctraits.end())
        return *trait;

    if (!check_super || klass.super_name == 0)
        return {};

    // Check the prototype chain
    // TODO: unroll it to prevent any recursive issue?
    std::optional<abc::Class> super;
    if (super = find_class_by_name(klass.super_name))
        return find_ctrait_by_name(*super, name);

    return {};
}
std::optional<abc::Trait>
detfm::find_itrait_by_name(abc::Class& klass, uint32_t& name, bool check_super) {
    auto& mn = abc->cpool.multinames[name];
    for (auto& trait : klass.itraits)
        if (trait.name == name)
            return trait;

    if (!check_super || klass.super_name == 0)
        return {};

    // Check the prototype chain
    // TODO: unroll it to prevent any recursive issue?
    std::optional<abc::Class> super;
    if (super = find_class_by_name(klass.super_name))
        return find_itrait_by_name(*super, name);

    return {};
}
std::optional<abc::Trait> detfm::find_trait(abc::Class& klass, uint32_t& name) {
    auto trait = find_ctrait_by_name(klass, name, false);
    if (!trait)
        trait = find_itrait_by_name(klass, name, false);

    return trait;
}

bool detfm::get_packet_code(std::shared_ptr<Instruction>& ins, uint8_t& code) {
    auto lastins = ins;
    if (ins->opcode != OP::getlex || ins->args[0] != pkt_hdlr->name)
        return false;

    if (!ins->next || ins->next->opcode != OP::getproperty)
        return false;

    ins = ins->next->next;
    if (ins && ins->opcode == OP::pushdouble) {
        code = static_cast<uint8_t>(abc->cpool.doubles[ins->args[0]]);
        ins  = ins->next;
        return ins && ins->opcode == OP::ifne;
    }

    // the pushdouble could be right before
    auto tmp = lastins->prev.lock();
    if (tmp && tmp->opcode == OP::pushdouble) {
        code = static_cast<uint8_t>(abc->cpool.doubles[tmp->args[0]]);
        return ins && ins->opcode == OP::ifne;
    }

    ins = lastins;
    return false;
}

bool detfm::is_sequence(std::shared_ptr<Instruction> ins, const std::vector<OP>& ops) {
    for (auto& op : ops) {
        if (!ins || ins->opcode != op)
            return false;
        ins = ins->next;
    }
    return true;
}

bool detfm::match_sent_pkt(abc::Class& klass) {
    if (sent_pkt != nullptr)
        return false;

    if (!klass.isSealed() || !klass.isProtected())
        return false;

    if (klass.itraits.empty() || klass.itraits[0].kind != abc::TraitKind::Slot)
        return false;

    return klass.itraits[0].slot.type == ByteArray;
}
bool detfm::match_recv_pkt(abc::Class& klass) {
    if (recv_pkt != nullptr)
        return false;

    auto isize = klass.itraits.size();
    auto csize = klass.ctraits.size();
    if (csize == 0 || csize > 9 || isize < 4 || isize > 9)
        return false;

    if (klass.itraits[2].kind != abc::TraitKind::Slot)
        return false;

    return klass.itraits[2].slot.type == ByteArray;
}
bool detfm::match_wrap_class(abc::Class& klass) {
    if (wrap_class != nullptr)
        return false;

    if (!klass.itraits.empty() || klass.ctraits.empty())
        return false;

    for (auto& trait : klass.ctraits) {
        if (trait.kind != abc::TraitKind::Method)
            return false;

        auto& method = abc->methods[trait.index];
        if (method.params.size() != 1 || method.params[0] != method.return_type)
            return false;
    }
    return true;
}
bool detfm::match_slot_class(abc::Class& klass) {
    if (!klass.itraits.empty() || klass.ctraits.size() < 100)
        return false;

    for (auto& trait : klass.ctraits) {
        bool is_slot   = trait.kind == abc::TraitKind::Slot;
        bool is_method = trait.kind == abc::TraitKind::Method;
        if (!is_slot && !is_method)
            return false;

        if (is_slot && trait.attr != 0)
            return false;

        if (is_method && trait.attr != abc::TraitAttributes::Final)
            return false;
    }
    return true;
}
bool detfm::match_packet_handler(abc::Class& klass) {
    if (!klass.itraits.empty())
        return false;

    return std::any_of(klass.ctraits.begin(), klass.ctraits.end(), [this](abc::Trait& trait) {
        return match_packet_handler(trait);
    });
}
bool detfm::match_packet_handler(abc::Trait& trait) {
    if (trait.kind != abc::TraitKind::Method)
        return false;

    auto& method = abc->methods[trait.index];
    if (method.max_stack < 30 || method.local_count < 200)
        return false;

    return method.params.size() == 1 && method.params[0] == ByteArray;
}

void detfm::rename_writeany() {
    for (auto& trait : sent_pkt->itraits) {
        if (trait.kind == swf::abc::TraitKind::Method) {
            auto& method = abc->methods[trait.index];
            if (method.max_stack == method.local_count && method.max_stack <= 2
                && method.init_scope_depth == method.max_scope_depth - 1
                && method.return_type == sent_pkt->name) {
                Parser parser(method);
                std::set<OP> allowed = {
                    OP::getlocal0,
                    OP::getlocal1,
                    OP::pushscope,
                    OP::returnvalue,
                };
                auto isAllowed = [&allowed](OP op) { return allowed.find(op) != allowed.end(); };
                uint32_t name  = 0;
                auto ins       = parser.begin;
                while (ins) {
                    if (ins->opcode == OP::getproperty) {
                        // Make sure we get the buffer
                        if (ins->args[0] != sent_pkt->itraits[0].name)
                            break;

                    } else if (ins->opcode == OP::callpropvoid) {
                        if (name == 0) {
                            name = abc->cpool.multinames[ins->args[0]].get_name_index();
                        } else {
                            // two names !!
                            name = 0;
                            break;
                        }
                    } else if (!isAllowed(ins->opcode)) {
                        break;
                    }
                    ins = ins->next;
                }

                // If we got a name, rename the trait with it. Do not use trait.name =
                // ... because it will do some 💩 with multinames
                if (name != 0)
                    trait.rename(abc->cpool.strings[name]);
            }
        }
    }
}

void detfm::set_class_ns(abc::Class& klass, uint32_t& ns) {
    if (ns == 0)
        throw std::runtime_error("Cannot rename class' namespace to null.");

    auto& mn = abc->cpool.multinames[klass.name];
    switch (mn.kind) {
    case abc::MultinameKind::QName:
    case abc::MultinameKind::QNameA:
        mn.data.qname.ns = ns;
        break;
    default:
        break;
    }
}
uint32_t detfm::create_package(std::string name) {
    auto index = abc->cpool.namespaces.size();
    auto& ns   = abc->cpool.namespaces.emplace_back();
    ns.kind    = abc::NamespaceKind::Package;
    ns.name    = (uint32_t)abc->cpool.strings.size();
    abc->cpool.strings.push_back(name);
    return static_cast<uint32_t>(index);
}