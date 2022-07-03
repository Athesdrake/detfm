#pragma once
#include <memory>
namespace swf::abc {
class AbcFile;
class Method;
}

template <typename T>
T eval_method(std::shared_ptr<swf::abc::AbcFile>& abc, swf::abc::Method& method);