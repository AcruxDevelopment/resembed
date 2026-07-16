#pragma once

#include <string>
#include <string_view>

// Represents the available name cases based on the provided convention rules.
enum class NameCase
{
    PascalCase,
    CamelCase,
    SnakeCase,
    KebabCase,          // Note: Generates hyphens, which are invalid in C++ identifiers
    FlatCase,
    UpperFlatCase,
    PascalSnakeCase,
    CamelSnakeCase,
    ScreamingSnakeCase,
    TrainCase,          // Note: Generates hyphens, which are invalid in C++ identifiers
    CobolCase           // Note: Generates hyphens, which are invalid in C++ identifiers
};

// Converts a given string view into a formatted identifier string based on the target case.
std::string ConvertToNamingConvention(std::string_view Input, NameCase TargetCase);
