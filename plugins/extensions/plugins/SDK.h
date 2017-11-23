
#pragma once

#include <brayns/parameters/AnimationParameters.h>
#include <staticjson/staticjson.hpp>

namespace staticjson
{
void init(brayns::AnimationParameters* c, ObjectHandler* h)
{
    h->add_property("start", &c->start);
    h->add_property("end", &c->end);
    h->add_property("current", &c->current);
    h->add_property("delta", &c->delta);
    h->set_flags(Flags::DisallowUnknownKey);
}
}
