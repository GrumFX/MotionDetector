#pragma once
#include "Measurement.h"
#include <vector>
#include <string>

class Scanner
{
public:
    std::vector<Measurement> scan();
};
