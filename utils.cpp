#include "utils.h"

size_t convertToBytes(double value, std::string unit) {
    if (unit == "KB") return static_cast<size_t>(value * 1024);
    if (unit == "MB") return static_cast<size_t>(value * 1024 * 1024);
    if (unit == "GB") return static_cast<size_t>(value * 1024 * 1024 * 1024);
    return static_cast<size_t>(value);
}

double convertFromBytes(size_t bytes, std::string unit) {
    if (unit == "KB") return static_cast<double>(bytes) / 1024.0;
    if (unit == "MB") return static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (unit == "GB") return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    return static_cast<double>(bytes);
}

size_t alignToWordBoundary(size_t bytes, size_t alignment) {
    return (bytes + alignment - 1) & ~(alignment - 1);
}