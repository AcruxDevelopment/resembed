#include "../include/NamingConventionConverter.h"
#include <vector>
#include <cctype>

namespace 
{
    // Extracts words by treating ANY non-alphanumeric character as a separator,
    // while also splitting on camelCase transitions.
    std::vector<std::string_view> ExtractWords(std::string_view Input)
    {
        std::vector<std::string_view> Words;
        size_t Start = std::string_view::npos;

        for (size_t i = 0; i < Input.size(); ++i) 
        {
            char Curr = Input[i];
            bool IsAlnum = std::isalnum(static_cast<unsigned char>(Curr));

            if (!IsAlnum) 
            {
                // We hit a delimiter (e.g., '.', '_', '-', ' '). Push the word if we have one.
                if (Start != std::string_view::npos) 
                {
                    Words.push_back(Input.substr(Start, i - Start));
                    Start = std::string_view::npos;
                }
            } 
            else 
            {
                if (Start == std::string_view::npos) 
                {
                    // Start of a new word
                    Start = i;
                } 
                else 
                {
                    char Prev = Input[i - 1];
                    // Detect a transition from lowercase to uppercase (e.g., "camelCase" -> "camel", "Case")
                    if (std::islower(static_cast<unsigned char>(Prev)) && 
                        std::isupper(static_cast<unsigned char>(Curr))) 
                    {
                        Words.push_back(Input.substr(Start, i - Start));
                        Start = i;
                    }
                }
            }
        }

        // Push the final word if the string ends with alphanumeric characters
        if (Start != std::string_view::npos) 
        {
            Words.push_back(Input.substr(Start));
        }

        return Words;
    }

    // Formats and appends a strictly alphanumeric word based on the requested casing.
    void AppendWord(std::string& Result, std::string_view Word, NameCase TargetCase, bool IsFirstWord) 
    {
        for (size_t i = 0; i < Word.size(); ++i) 
        {
            char c = Word[i];
            
            if (TargetCase == NameCase::SnakeCase || 
                TargetCase == NameCase::KebabCase || 
                TargetCase == NameCase::FlatCase) 
            {
                Result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            } 
            else if (TargetCase == NameCase::ScreamingSnakeCase || 
                     TargetCase == NameCase::UpperFlatCase || 
                     TargetCase == NameCase::CobolCase) 
            {
                Result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            } 
            else 
            {
                // Logic for mixed-case types (Pascal, Camel, Train, etc.)
                bool Capitalize = false;
                if (i == 0) 
                {
                    if (TargetCase == NameCase::PascalCase || 
                        TargetCase == NameCase::PascalSnakeCase || 
                        TargetCase == NameCase::TrainCase) 
                    {
                        Capitalize = true;
                    } 
                    else if (TargetCase == NameCase::CamelCase || 
                             TargetCase == NameCase::CamelSnakeCase) 
                    {
                        Capitalize = !IsFirstWord;
                    }
                }

                if (Capitalize) 
                {
                    Result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                } 
                else 
                {
                    Result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
            }
        }
    }
}

std::string ConvertToNamingConvention(std::string_view Input, NameCase TargetCase)
{
    std::vector<std::string_view> Words = ExtractWords(Input);
    std::string Result;
    
    if (Words.empty()) 
    {
        return Result;
    }

    // Pre-allocate memory to avoid multiple reallocations during appending
    Result.reserve(Input.size() + Words.size());

    for (size_t i = 0; i < Words.size(); ++i) 
    {
        if (i > 0) 
        {
            if (TargetCase == NameCase::SnakeCase || 
                TargetCase == NameCase::PascalSnakeCase || 
                TargetCase == NameCase::CamelSnakeCase || 
                TargetCase == NameCase::ScreamingSnakeCase) 
            {
                Result += '_';
            } 
            else if (TargetCase == NameCase::KebabCase || 
                     TargetCase == NameCase::TrainCase || 
                     TargetCase == NameCase::CobolCase) 
            {
                Result += '-';
            }
        }
        
        AppendWord(Result, Words[i], TargetCase, i == 0);
    }

    // Safety check: A valid C++ identifier cannot begin with a number
    if (!Result.empty() && std::isdigit(static_cast<unsigned char>(Result.front()))) 
    {
        Result.insert(Result.begin(), '_');
    }

    return Result;
}
