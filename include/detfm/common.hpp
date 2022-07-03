#pragma once
#include <abc/AbcFile.hpp>
#include <cstdint>
#include <memory>
#include <string>

namespace abc = swf::abc;

std::string& resolve_multiname(std::shared_ptr<abc::AbcFile>& abc, uint32_t& index);
std::string& resolve_multiname(std::shared_ptr<abc::AbcFile>& abc, abc::Multiname& mn);