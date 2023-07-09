#pragma once
#include <memory>
namespace swf::abc {
class AbcFile;
class Method;
}

namespace athes::detfm {
template <typename T>
T eval_method(std::shared_ptr<swf::abc::AbcFile>& abc, swf::abc::Method& method);
}