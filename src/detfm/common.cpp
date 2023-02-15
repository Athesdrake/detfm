#include "detfm/common.hpp"

// TODO: Move this utility to swflib
namespace swf::abc {
std::string& qname(std::shared_ptr<AbcFile> abc, uint32_t& index) {
    return qname(abc, abc->cpool.multinames[index]);
}
std::string& qname(std::shared_ptr<AbcFile> abc, abc::Multiname& mn) {
    return abc->cpool.strings[mn.get_name_index()];
}

std::string str(std::shared_ptr<AbcFile>& abc, std::vector<uint32_t>& ns_set) {
    std::string name = "";
    for (auto& index : ns_set) {
        if (!name.empty())
            name += "::";
        name += str(abc, abc->cpool.namespaces[index]);
    }
    return name;
}

std::string str(std::shared_ptr<AbcFile>& abc, uint32_t& index) {
    return str(abc, abc->cpool.multinames[index]);
}

std::string str(std::shared_ptr<AbcFile>& abc, Multiname& mn) {
    if (mn.kind == MultinameKind::Typename) {
        std::string types = "<";
        for (auto& type : mn.types)
            types += (type == 0 ? "*" : str(abc, type)) + ',';

        if (mn.types.size() > 1)
            types += '>';
        else
            types.back() = '>';

        return str(abc, mn.data.type_name.qname) + types;
    }

    if (mn.kind == MultinameKind::MultinameL || mn.kind == MultinameKind::MultinameLA)
        return str(abc, abc->cpool.ns_sets[mn.data.multiname_l.ns_set]);

    return qname(abc, mn);
}

std::string& str(std::shared_ptr<AbcFile>& abc, Namespace& ns) {
    return abc->cpool.strings[ns.name];
}

std::string ns(std::shared_ptr<AbcFile>& abc, Multiname& mn) {
    switch (mn.kind) {
    case MultinameKind::QName:
    case MultinameKind::QNameA:
        if (mn.data.qname.ns == 0)
            return "";

        return str(abc, abc->cpool.namespaces[mn.data.qname.ns]);
    case MultinameKind::RTQName:
    case MultinameKind::RTQNameA:
    case MultinameKind::RTQNameL:
    case MultinameKind::RTQNameLA:
        // RTQName does not have namespaces
        return "";
    case MultinameKind::Multiname:
    case MultinameKind::MultinameA:
    case MultinameKind::MultinameL:
    case MultinameKind::MultinameLA:
        return str(abc, abc->cpool.ns_sets[mn.data.multiname.ns_set]);
    default:
        return "";
    }
}

std::string fqn(std::shared_ptr<AbcFile> abc, Class& klass) {
    auto& mn    = abc->cpool.multinames[klass.name];
    auto namesp = ns(abc, mn);
    auto name   = mn.data.qname.name == 0 ? "*" : abc->cpool.strings[mn.data.qname.name];
    return namesp.empty() ? name : namesp + "::" + name;
}
}