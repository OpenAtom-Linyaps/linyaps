#pragma once

#include <string>

namespace ocppi::runtime
{

class Signal : public std::string {
        using std::string::string;
};

}
