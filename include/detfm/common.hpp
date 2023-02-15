#pragma once
#include <abc/AbcFile.hpp>
#include <cstdint>
#include <memory>
#include <string>

namespace abc = swf::abc;

namespace swf::abc {
/* Get the name of QName as a ref from the multiname index */
std::string& qname(std::shared_ptr<AbcFile> abc, uint32_t& index);
/* Get the name of QName as a ref */
std::string& qname(std::shared_ptr<AbcFile> abc, Multiname& mn);
/* Get a multiname's name from its index */
std::string str(std::shared_ptr<AbcFile>& abc, uint32_t& index);
/* Get a multiname's name */
std::string str(std::shared_ptr<AbcFile>& abc, Multiname& mn);
/* Get a namespace's name */
std::string& str(std::shared_ptr<AbcFile>& abc, Namespace& ns);
/* Get the fully qualified name of a class: package::ClassName */
std::string fqn(std::shared_ptr<AbcFile> abc, Class& klass);
}