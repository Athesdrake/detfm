#include "detfm/common.hpp"

std::string& resolve_multiname(std::shared_ptr<abc::AbcFile>& abc, uint32_t& index) {
    return resolve_multiname(abc, abc->cpool.multinames[index]);
}

std::string& resolve_multiname(std::shared_ptr<abc::AbcFile>& abc, abc::Multiname& mn) {
    return abc->cpool.strings[mn.get_name_index()];
}
