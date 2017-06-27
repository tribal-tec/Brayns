//#include "ospray/common/Model.h"

#include <iostream>
namespace ospray
{
extern "C" void ospray_init_module_deflect()
{
    std::cout << "#deflect: initializing ospray deflect plugin" << std::endl;
}
}
