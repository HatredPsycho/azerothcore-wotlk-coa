/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */
#ifndef ASCENSION_CUSTOM_RACES_H
#define ASCENSION_CUSTOM_RACES_H

namespace AscensionCustomRaces
{
class TestOverride
{
public:
    explicit TestOverride(bool enabled);
    ~TestOverride();
    TestOverride(TestOverride const&) = delete;
    TestOverride& operator=(TestOverride const&) = delete;
};
}

#endif
