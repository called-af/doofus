#include "Log.h"

#include <iostream>

void Log::info(const std::string& message)
{
    std::cout
        << "[INFO] "
        << message
        << "\n";
}

void Log::warn(const std::string& message)
{
    std::cout
        << "[WARN] "
        << message
        << "\n";
}

void Log::error(const std::string& message)
{
    std::cerr
        << "[ERROR] "
        << message
        << "\n";
}