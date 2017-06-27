#include "OSPRayPixelOperations.h"

namespace brayns
{
DeflectPixelOp::DeflectPixelOp()
{
}

DeflectPixelOp::~DeflectPixelOp()
{
}
}

namespace ospray
{
OSP_REGISTER_PIXEL_OP(brayns::DeflectPixelOp, DeflectPixelOp);
}
