#include "resources/embeds/MAIN_CPP.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <filesystem>

// =========================================================================
// CASE 1: SMART FILE EXTRACTION
// The easiest method. Pass a path, and it safely extracts in 32KB chunks.
// =========================================================================
void TestSmartExtraction()
{
    std::cout << "[1] Testing Smart Path Decompression...\n";
    bool success = Resources::Embeds::MAIN_CPP::Decompress("extracted/output_1_smart.cpp");
    std::cout << "    -> Result: " << (success ? "SUCCESS" : "FAILED") << "\n\n";
}

// =========================================================================
// CASE 2: FILE STREAM
// Great for appending, prepending headers, or custom stream wrappers.
// =========================================================================
void TestStreamExtraction()
{
    std::cout << "[2] Testing File Stream Decompression...\n";
    std::ofstream streamFile("extracted/output_2_stream.cpp", std::ios::binary);
    bool success = Resources::Embeds::MAIN_CPP::Decompress(streamFile);
    
    // std::ofstream auto-closes when it goes out of scope
    std::cout << "    -> Result: " << (success ? "SUCCESS" : "FAILED") << "\n\n";
}

// =========================================================================
// CASE 3: IN MEMORY (HEAP ALLOCATION)
// Safe for massive files (500MB+). Now properly dumped to disk!
// =========================================================================
void TestHeapExtraction()
{
    std::cout << "[3] Testing Heap Allocation Decompression...\n";
    size_t heapRequiredSize = Resources::Embeds::MAIN_CPP::UncompressedSize();
    auto heapBuffer = std::make_unique<uint8_t[]>(heapRequiredSize);
    
    bool success = Resources::Embeds::MAIN_CPP::Decompress(heapBuffer.get(), heapRequiredSize);
    
    if (success)
    {
        // Dump the heap allocation out to verify the data integrity
        std::ofstream heapDumpFile("extracted/output_3_heap.cpp", std::ios::binary);
        if (heapDumpFile)
        {
            heapDumpFile.write(reinterpret_cast<const char*>(heapBuffer.get()), heapRequiredSize);
            std::cout << "    -> Result: SUCCESS (Dumped to extracted/output_3_heap.cpp)\n\n";
        }
    }
    else
    {
        std::cout << "    -> Result: FAILED DECOMPRESSION\n\n";
    }
}

// =========================================================================
// CASE 4: IN MEMORY (STACK ALLOCATION)
// Blazing fast, zero dynamic memory. Protected by a compile-time size check.
// =========================================================================
void TestStackExtraction()
{
    std::cout << "[4] Testing Stack Allocation Decompression...\n";
    
    constexpr size_t stackRequiredSize = Resources::Embeds::MAIN_CPP::UncompressedSize();
    
    // SAFETY GUARD: Refuse to compile if the file is larger than 1 MB!
    static_assert(stackRequiredSize <= 1024 * 1024, "Asset is too large for the stack! Use the Heap instead.");

    uint8_t stackBuffer[stackRequiredSize];
    bool success = Resources::Embeds::MAIN_CPP::Decompress(stackBuffer, stackRequiredSize);
    
    if (success)
    {
        std::ofstream stackDumpFile("extracted/output_4_stack.cpp", std::ios::binary);
        if (stackDumpFile) 
        {
            stackDumpFile.write(reinterpret_cast<const char*>(stackBuffer), stackRequiredSize);
            std::cout << "    -> Result: SUCCESS (Dumped to extracted/output_4_stack.cpp)\n\n";
        }
    }
    else
    {
        std::cout << "    -> Result: FAILED DECOMPRESSION\n\n";
    }
}

// =========================================================================
// ENTRY POINT
// =========================================================================
int main()
{
    std::cout << "--- Testing Asset Decompression API ---\n\n";

    // Ensure the output folder exists before running any tests
    std::filesystem::create_directories("extracted");

    TestSmartExtraction();
    TestStreamExtraction();
    TestHeapExtraction();
    TestStackExtraction();

    std::cout << "--- All tests complete! ---\n";
    return 0;
}
