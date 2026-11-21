#ifndef UTILS_H
#define UTILS_H

#include <cstddef>
#include <string>

struct AllocationRecord {
    void* firstFitPtr;
    void* bestFitPtr;
    void* worstFitPtr;
    void* osPtr;
    double requestedValue;
};

size_t convertToBytes(double value, std::string unit);
double convertFromBytes(size_t bytes, std::string unit);
size_t alignToWordBoundary(size_t bytes, size_t alignment = 8);

#endif