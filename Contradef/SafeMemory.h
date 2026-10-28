#pragma once

#include "pin.H"
#include <cstddef>

template <typename T>
bool ReadTargetValue(ADDRINT address, std::size_t available, T& value)
{
    return address != 0 && available >= sizeof(T) &&
        PIN_SafeCopy(&value, reinterpret_cast<const void*>(address), sizeof(T)) == sizeof(T);
}

template <typename T>
bool WriteTargetValue(ADDRINT address, std::size_t available, const T& value)
{
    return address != 0 && available >= sizeof(T) &&
        PIN_SafeCopy(reinterpret_cast<void*>(address), &value, sizeof(T)) == sizeof(T);
}
