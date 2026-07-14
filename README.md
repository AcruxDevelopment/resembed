# Resembed 📦

**Resembed** is a compile-time asset embedding tool for C++. It parses raw resource files (images, shaders, text, audio) and compiles them directly into compressed C++ headers.

By mapping your physical folder structure directly to **C++17 nested namespaces**, Resembed turns your filesystem into a strongly-typed asset tree.

### Key Features

* **IDE Introspection:** Assets are strongly typed. Typing `Resources::Embeds::` triggers your IDE's autocomplete, allowing you to browse your filesystem directly in code.
* **Compile-Time Safety:** If an asset is deleted or renamed on disk, the project will fail to compile. This catches broken references during the build step rather than at runtime.
* **Link-Time Resolution:** Avoids runtime string parsing or hash map lookups (e.g., `std::map<std::string, Data>`). All asset associations are resolved by the linker.
* **Configurable Decompression:** Uses `miniz` to offer multiple memory delivery strategies (Stack, Heap, File Stream) so you can control exactly how data is unpacked.
* **Stack Overflow Protection:** Uses `constexpr` evaluation and `static_assert` to refuse compilation if you attempt to unpack a large file onto the stack.

---

## 📂 Project Structure

The repository is divided into the generator (`tool/`) and the consumer testing environment (`test/`).

```text
.
├── tool/                    # The Asset Generator (Resembed)
│   ├── main.cpp             # CLI entry point
│   ├── NamingConvention...  # Converts paths to PascalCase namespaces
│   ├── zip_file.hpp         # Miniz wrapper for compression
│   └── run.sh               # Builds and executes the generator
│
└── test/                    # The Consumer Environment
    ├── main.cpp             # Validates decompression methods
    ├── run.sh               # Builds and runs the test executable
    ├── resources/embeds/    # [Gitignored] Auto-generated headers & miniz.h
    └── extracted/           # [Gitignored] Decompressed output validation

```

> **Note:** The generated `resources/` headers, the `extracted/` test outputs, and the compiled binaries are intentionally `.gitignore`d. Generating headers should be treated as a standard build step.

---

## 🚀 How It Works

Run the tool against your target directory. Resembed recursively explores the files, compresses them using `mz_compress2`, and generates a standalone `.h` file for each asset in the output directory.

The generator preserves your directory structure and applies `PascalCase` to both the folder names and the filename.

**Example File:** `sprites/player/down.png`
**Generated Header:** `resources/embeds/sprites/player/DownPng.h`
**Generated Namespace:** `Resources::Embeds::Sprites::Player::DownPng`

```cpp
// Inside DownPng.h
namespace Resources::Embeds::Sprites::Player::DownPng {
    inline constexpr size_t ORIGINAL_SIZE = 4096;
    inline constexpr uint8_t DATA[] = { 0x42, 0x4D, ... };
    
    // Decompression API...
}

```

---

## 💻 API Usage & Examples

Every generated header exposes multiple ways to access the data. You should choose your extraction method based on the file size and your application's memory constraints.

### Strategy Comparison

| Method | Best For | Description |
| --- | --- | --- |
| **Auto-Heap** | Standard loading | Returns a `std::unique_ptr<uint8_t[]>` directly for straightforward memory management. |
| **Stack Allocation** | Small assets (< 1MB) | Zero dynamic memory allocation. Protected by compile-time size checks. |
| **Smart Path** | Quick disk writes | Pass a string path; automatically handles file creation, chunking, and cleanup. |
| **File Stream** | Custom IO | Pass a `std::ofstream`. Useful for appending data or writing custom headers first. |

### 1. Auto-Heap (Smart Pointers)

This is the standard approach for loading assets into memory for immediate use (e.g., passing texture data to OpenGL or Vulkan).

```cpp
#include "resources/embeds/sprites/player/DownPng.h"
#include <memory>
#include <iostream>

void LoadTexture()
{
    // Allocates the exact memory needed and unzips the payload
    std::unique_ptr<uint8_t[]> buffer = Resources::Embeds::Sprites::Player::DownPng::Decompress();

    if (buffer) {
        // Data is ready to use
        // renderer.UploadTexture(buffer.get(), ...);
    }
}

```

### 2. Safe Stack Allocation

Useful for small configuration files, JSON strings, or shaders where you want to avoid heap allocation entirely.

```cpp
#include "resources/embeds/config/SettingsJson.h"

void ReadConfig()
{
    // 1. Evaluate required size at compile-time
    constexpr size_t size = Resources::Embeds::Config::SettingsJson::UncompressedSize();

    // 2. Safety guard: Compiler throws an error if the file exceeds 1MB
    static_assert(size <= 1024 * 1024, "Asset is too large for the stack. Use the heap.");

    // 3. Allocate a standard C-array (Not a Variable Length Array)
    uint8_t stackBuffer[size];
    
    if (Resources::Embeds::Config::SettingsJson::Decompress(stackBuffer, size)) {
        // Process configuration data safely
    }
}

```

### 3. Smart Path Extraction

If you need to write the embedded asset back out to the user's local disk, you can pass a path directly. The tool unpacks the file using a fixed 32KB stack buffer to keep memory usage low, regardless of the target file's size.

```cpp
#include "resources/embeds/audio/BackgroundMusicWav.h"

void ExtractAudio()
{
    // Automatically opens the file, streams the decompressed data, and closes it.
    bool success = Resources::Embeds::Audio::BackgroundMusicWav::Decompress("C:/temp/bg_music.wav");
}

```

### 4. Custom File Stream

If you need to manually manage the file handle—for example, if you are concatenating multiple embedded files into one physical file.

```cpp
#include "resources/embeds/data/Level1Bin.h"
#include <fstream>

void AppendLevelData()
{
    // Open in append mode
    std::ofstream file("game_data.bin", std::ios::binary | std::ios::app);
    
    if (file.is_open()) {
        Resources::Embeds::Data::Level1Bin::Decompress(file);
    }
}

```
