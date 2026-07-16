# Resembed

Resembed is a compile-time asset embedding tool for C++17. It converts raw resource files (images, shaders, text, audio, etc.) into compressed C++ headers that are compiled directly into your application binary.

Your folder structure maps 1:1 to nested C++ namespaces, giving you a strongly-typed asset tree with IDE autocomplete, compile-time safety (missing files = build errors, not runtime crashes), and zero runtime string lookups.

---

## How It Works

Resembed has two parts:

1. **The generator** (`tool/`) — a CLI tool you run as a build step
2. **The consumer** (`test/`) — your application, which includes the generated headers

### The Generator

Run the generator against your resource directory. It:

1. Recursively scans all files in the source directory
2. Converts each file path to a PascalCase C++ namespace hierarchy
3. Compresses the file data using `miniz` (`mz_compress2`)
4. Emits a standalone `.h` file per asset into the output directory
5. Copies `miniz.h` into the output directory (required at runtime for decompression)

**Example mapping:**

| Source file | Generated header | C++ namespace |
|---|---|---|
| `sprites/player/down.png` | `resources/embeds/sprites/player/DownPng.h` | `Resources::Embeds::Sprites::Player::DownPng` |
| `text/message.txt` | `resources/embeds/text/Message.h` | `Resources::Embeds::Text::Message` |

### The Consumer

Your application `#include`s the generated headers. No runtime file I/O, no hash maps — the linker resolves everything. If you rename or delete an asset, the project fails to compile.

---

## Building and Running

### Step 1: Build and run the generator

```bash
cd tool/
bash run.sh
```

This compiles `src/main.cpp` and `src/NamingConventionConverter.cpp` into a `resembed` binary, then runs it:

```bash
./resembed <source_dir> <output_dir> <config_json>
# Example from run.sh:
./resembed resources ../test/Resources/Embeds resources/resources.json
```

The generated headers land in `../test/Resources/Embeds/` and are gitignored — treat generation as a standard build step.

### Step 2: Build and run your consumer

```bash
cd test/
bash run.sh
```

This compiles `main.cpp` (which includes the generated headers) and runs the test binary.

---

## Configuration (`resources.json`)

You control compression and other settings via a JSON config file passed as the third CLI argument.

```json
{
    "@base": {
        "compression": "maximum",
        "include_extensions": false,
        "ignore": ["resources.json"]
    },
    "images": {
        "include_extensions": true
    },
    "images/cpp/cpp.svg": {
        "compression": "none"
    },
    "text": {
        "compression": "none"
    }
}
```

**Compression levels:** `none` (0), `fastest` (1), `balanced` (6), `maximum` (9)

**`include_extensions`:** Controls whether the file extension is part of the C++ identifier. `true` → `CppPng`, `false` → `Cpp`.

**Inheritance rules:**
- `@base` is the global default for all files
- Directory keys (e.g., `"images"`) override `@base` for everything inside that directory
- File-specific keys (e.g., `"images/cpp/cpp.svg"`) override their parent directory
- The `ignore` list is **never inherited** — each level must declare its own ignores explicitly

---

## Generated Header API

Each generated header exposes a namespace with functions to access the data. There are two kinds of headers depending on whether compression is enabled.

### Compressed assets

```cpp
namespace Resources::Embeds::Sprites::Player::DownPng {
    // Metadata
    inline constexpr size_t UncompressedSize();
    inline constexpr size_t CompressedSize();
    inline constexpr const uint8_t* CompressedData();

    // Decompression overloads (see usage section below)
    bool Decompress(uint8_t* destBuffer, size_t destBufferSize);
    std::unique_ptr<uint8_t[]> Decompress();
    bool Decompress(std::ofstream& file);
    bool Decompress(const std::filesystem::path& outputPath);
}
```

### Uncompressed assets (`compression: none`)

```cpp
namespace Resources::Embeds::Text::Message {
    inline constexpr size_t Size();
    inline constexpr const uint8_t* Data();
    inline std::string_view StringView();   // zero-copy string access

    bool Extract(std::ofstream& file);
    bool Extract(const std::filesystem::path& outputPath);
}
```

---

## Usage Examples

### Auto-heap (standard loading)

Best for textures, audio, or any asset you need in memory. Returns a `unique_ptr` sized exactly to the uncompressed data.

```cpp
#include "Resources/Embeds/sprites/player/DownPng.h"

std::unique_ptr<uint8_t[]> buffer = Resources::Embeds::Sprites::Player::DownPng::Decompress();
if (buffer) {
    // pass buffer.get() to your renderer, audio engine, etc.
}
```

### Stack allocation (small assets only)

Zero dynamic allocation. The `static_assert` prevents compilation if the asset exceeds 1 MB, protecting against stack overflows.

```cpp
#include "Resources/Embeds/config/SettingsJson.h"

constexpr size_t size = Resources::Embeds::Config::SettingsJson::UncompressedSize();
static_assert(size <= 1024 * 1024, "Asset too large for stack. Use heap.");

uint8_t stackBuffer[size];
Resources::Embeds::Config::SettingsJson::Decompress(stackBuffer, size);
```

### Manual heap allocation

When you need to control the buffer yourself (e.g., reuse an existing allocation).

```cpp
size_t required = Resources::Embeds::Images::Cpp::CppPng::UncompressedSize();
auto buffer = std::make_unique<uint8_t[]>(required);
Resources::Embeds::Images::Cpp::CppPng::Decompress(buffer.get(), required);
```

### Smart path extraction (write to disk)

Decompresses directly to a file path using a fixed 32 KB internal buffer — memory usage stays constant regardless of file size.

```cpp
Resources::Embeds::Audio::BackgroundMusicWav::Decompress("C:/temp/bg_music.wav");
```

### File stream extraction

Useful when you need to append to an existing file or manage the handle yourself.

```cpp
std::ofstream file("game_data.bin", std::ios::binary | std::ios::app);
if (file.is_open()) {
    Resources::Embeds::Data::Level1Bin::Decompress(file);
}
```

### Text / uncompressed assets

```cpp
#include "Resources/Embeds/text/Message.h"

// Zero-copy string view directly into the binary's .rodata segment
std::string_view msg = Resources::Embeds::Text::Message::StringView();
std::cout << msg << "\n";

// Or extract to disk
Resources::Embeds::Text::Message::Extract("output/message.txt");
```

---

## Requirements

- C++17 compiler with `std::filesystem` support
- `g++` (used by the provided `.sh` scripts)
- Bash (to run `tool/run.sh` and `test/run.sh`)

---

## Project Structure

```
.
├── tool/                          # The generator
│   ├── src/
│   │   ├── main.cpp               # CLI entry point + code generation pipeline
│   │   └── NamingConventionConverter.cpp  # Path → PascalCase namespace converter
│   ├── include/
│   │   ├── zip_file.hpp           # miniz wrapper
│   │   ├── Configuration.h        # Runtime config resolver
│   │   ├── ConfigurationParser.h  # JSON → Configuration parser
│   │   └── Compression.h          # Compression level enum
│   ├── resources/
│   │   └── resources.json         # Example config
│   └── run.sh                     # Build + run the generator
│
└── test/                          # Consumer / test environment
    ├── main.cpp                   # Exercises all decompression strategies
    └── run.sh                     # Build + run the tests
``` [1](#0-0) [2](#0-1) [3](#0-2) [4](#0-3) [5](#0-4) [6](#0-5) [7](#0-6)

### Citations

**File:** tool/run.sh (L1-8)
```shellscript
g++ \
	src/main.cpp \
	src/NamingConventionConverter.cpp \
	-o resembed

rm -fr ../test/Resources/Embeds

./resembed resources ../test/Resources/Embeds resources/resources.json
```

**File:** tool/resources/resources.json (L1-22)
```json
{
	"@base":
	{
		"compression": "maximum",
		"include_extensions": false,
		"ignore": ["resources.json"]
	},

	"images":
	{
		"include_extensions": true
	},
	"images/cpp/cpp.svg":
	{
		"compression": "none"
	},

	"text":
	{
		"compression": "none"
	}
}
```

**File:** test/main.cpp (L1-12)
```cpp
#include "Resources/Embeds/images/cpp/CppPng.h"
#include "Resources/Embeds/images/cpp/CppSvg.h"
#include "Resources/Embeds/text/Message.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <filesystem>

void TestMessage()
{
	std::cout << Resources::Embeds::Text::Message::StringView() << "\n";
}
```

**File:** test/main.cpp (L82-92)
```cpp
void TestStackExtraction()
{
    std::cout << "[4] Testing Stack Allocation Decompression...\n";
    
    constexpr size_t stackRequiredSize = Resources::Embeds::Images::Cpp::CppPng::UncompressedSize();
    
    // SAFETY GUARD: Refuse to compile if the file is larger than 1 MB!
    static_assert(stackRequiredSize <= 1024 * 1024, "Asset is too large for the stack! Use the Heap instead.");

    uint8_t stackBuffer[stackRequiredSize];
    bool success = Resources::Embeds::Images::Cpp::CppPng::Decompress(stackBuffer, stackRequiredSize);
```

**File:** test/main.cpp (L113-137)
```cpp
void TestAutoHeapExtraction()
{
    std::cout << "[5] Testing Auto Heap Allocation Decompression...\n";
    
	std::unique_ptr<uint8_t[]> buffer = Resources::Embeds::Images::Cpp::CppPng::Decompress();
    
    if (buffer)
    {
        // Dump the heap allocation out to verify the data integrity
        std::ofstream heapDumpFile("extracted/output_5_autoheap.cpp", std::ios::binary);
        if (heapDumpFile)
        {
            heapDumpFile.write(
				reinterpret_cast<const char*>(buffer.get()),
				Resources::Embeds::Images::Cpp::CppPng::UncompressedSize()
			);

            std::cout << "    -> Result: SUCCESS (Dumped to extracted/output_5_autoheap.cpp)\n\n";
        }
    }
    else
    {
        std::cout << "    -> Result: FAILED DECOMPRESSION\n\n";
    }
}
```

**File:** test/run.sh (L1-4)
```shellscript
rm -fr extracted
g++ main.cpp -o test
./test
ls extracted
```

**File:** README.md (L47-60)
```markdown
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
```
