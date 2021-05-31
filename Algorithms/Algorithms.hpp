#pragma once

#include <memory>

namespace Windscribe { 

/** @brief  */
class Algorithms
{
public:

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_{ nullptr };
};

}
