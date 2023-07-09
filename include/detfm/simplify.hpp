#pragma once
#include <memory>
namespace swf::abc {
class AbcFile;
class Method;
}

namespace athes::detfm {
void simplify_expressions(std::shared_ptr<swf::abc::AbcFile>& abc, swf::abc::Method& method);
}