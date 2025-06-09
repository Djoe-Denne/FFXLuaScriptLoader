# patch_memory_usage.py - Documentation

## Overview

`patch_memory_usage.py` is a pre-patching instruction script for the **ff8_hook** project. It transforms assembly instructions extracted from IDA Pro into pre-patched versions with instruction bytes and calculated offsets for dynamic memory relocation.

## Objective

The script takes as input:
- A CSV file of memory addresses extracted from IDA Pro
- A binary file (e.g., `FF8_EN.exe`)

And generates as output:
- A TOML file containing instruction bytes with 'X' markers for bytes to patch and their corresponding memory offsets

## Processing Pipeline

```
IDA CSV → Disassembly → Byte Analysis → TOML → ff8_hook
```

1. **CSV Extraction**: IDA Pro exports memory usage addresses
2. **Disassembly**: Capstone disassembles instructions at those addresses
3. **Byte Analysis**: Identifies memory references within instruction bytes
4. **TOML Export**: Structured format with bytes and offsets for ff8_hook
5. **Runtime Patching**: ff8_hook patches the marked bytes with new memory addresses

## Installation

### Prerequisites

```bash
pip install capstone toml
```

### Python Dependencies

- `capstone`: Disassembler engine
- `toml`: Output format library
- `csv`, `argparse`, `logging`, `struct`: Standard modules

## Usage

### Basic Syntax

```bash
python patch_memory_usage.py --csv <csv_file> --binary <binary_file>
```

### Complete Example

```bash
python patch_memory_usage.py \
    --csv ida_memory_usage.csv \
    --binary FF8_EN.exe \
    --memory-base 0x01CF4064 \
    --image-base 0x400000 \
    --verbose
```

### Available Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `--csv` | **Required** | - | CSV file with IDA addresses |
| `--binary` | **Required** | - | Binary file to analyze |
| `--memory-base` | Hex | `0x01CF4064` | Memory base for offset calculations |
| `--image-base` | Hex | `0x400000` | Binary image base address |
| `--bytes-to-read` | Int | `15` | Bytes per instruction to read |
| `--csv-delimiter` | Char | `\t` | CSV delimiter |
| `--max-offset-range` | Hex | `0x10000` | Maximum offset range |
| `--log-file` | String | `find_memory_usage.log` | Log file |
| `--verbose`, `-v` | Flag | `false` | Debug mode |

## File Formats

### Input CSV File

Expected format from IDA Pro:
```csv
0x0048D774
0x0048D8A4
0x00496C11
0x00496AD9
```

Each line contains a hexadecimal address where an instruction accesses the target memory region.

### Output TOML File

```toml
[metadata]
script_version = "1.0.0"
memory_base = "0x01CF4064"
image_base = "0x400000"
total_instructions = 142
description = "Pre-patched instructions for ff8_hook"

[instructions.0x0048D774]
bytes = "8D 86 XX XX XX XX"
offset = "0x2A"

[instructions.0x0048D8A4]
bytes = "8D 90 XX XX XX XX"
offset = "0x2A"

[instructions.0x00496C11]
bytes = "66 8B 98 XX XX XX XX"
offset = "0x28"

[instructions.0x00496AD9]
bytes = "8A 04 85 XX XX XX XX"
offset = "0x26"
```

### Output Format Explanation

- **`bytes`**: Instruction bytes in hexadecimal with 'X' markers indicating bytes to patch
- **`offset`**: Calculated offset from memory_base (e.g., "0x2A", "-0x10")
- **`XX XX XX XX`**: 4-byte placeholder where the new memory address will be patched

## Integration with ff8_hook

### 1. Generate TOML

```bash
# Generate patch file
python patch_memory_usage.py \
    --csv ida_magic_usage.csv \
    --binary FF8_EN.exe \
    > magic_patches.toml
```

### 2. Usage in ff8_hook

The ff8_hook project can then:
1. Load the TOML file
2. Allocate new memory region
3. Calculate new addresses using the offsets
4. Apply binary patches by replacing 'X' markers with actual addresses

### 3. Example ff8_hook Usage

```cpp
// Pseudo-code ff8_hook
void* new_memory = allocate_memory(size);
uintptr_t new_base = (uintptr_t)new_memory;

// Apply patches using bytes and offsets
for (auto& instruction : patches.instructions) {
    std::string addr = instruction.first;
    std::string bytes = instruction.second.bytes;
    int32_t offset = parse_offset(instruction.second.offset);
    
    // Calculate new address
    uintptr_t new_addr = new_base + offset;
    
    // Replace 'XX XX XX XX' with new_addr in bytes
    std::string patched_bytes = replace_x_markers(bytes, new_addr);
    
    // Apply binary patch at addr
    apply_binary_patch(parse_address(addr), patched_bytes);
}
```

## Complete Workflow

### 1. Extraction from IDA Pro

1. Open `FF8_EN.exe` in IDA Pro
2. Identify target memory region (e.g., magic data at `0x01CF4064`)
3. Find all references (Ctrl+X on the address)
4. Export addresses to CSV format

### 2. Pre-patching

```bash
python patch_memory_usage.py \
    --csv ida_refs.csv \
    --binary FF8_EN.exe \
    --memory-base 0x01CF4064 \
    --verbose \
    > magic_patches.toml
```

### 3. ff8_hook Integration

1. Load `magic_patches.toml` in ff8_hook
2. Allocate new memory
3. Copy original data
4. Apply patches using bytes and offsets
5. Redirect memory access

## Logging and Debugging

### Log Levels

- **INFO**: General progress, statistics
- **DEBUG**: Details of each instruction (with `--verbose`)
- **WARNING**: Invalid addresses ignored
- **ERROR**: Processing errors

### Log File

The `find_memory_usage.log` file contains:
```
2024-01-15 14:30:15 - INFO - === Starting pre-patching script ===
2024-01-15 14:30:15 - INFO - CSV file: ida_usage.csv
2024-01-15 14:30:15 - INFO - Memory base: 0x01CF4064
2024-01-15 14:30:16 - INFO - Successful pre-patches: 142
2024-01-15 14:30:16 - INFO - Success rate: 98.61%
```

## Troubleshooting

### Common Errors

1. **"Binary file not found"**
   - Verify path to `FF8_EN.exe`
   - Ensure file is accessible

2. **"Invalid addresses"**
   - Check CSV format (addresses starting with `0x`)
   - Verify delimiter (tab by default)

3. **"Disassembly failures"**
   - Adjust `--bytes-to-read` (15 by default)
   - Verify `--image-base` (0x400000 by default)

### Debug Mode

```bash
python patch_memory_usage.py --csv data.csv --binary game.exe --verbose
```

Verbose mode displays each processed instruction and facilitates debugging.

## Advanced Examples

### Processing Multiple Memory Regions

```bash
# Magic region
python patch_memory_usage.py \
    --csv magic_refs.csv \
    --binary FF8_EN.exe \
    --memory-base 0x01CF4064 \
    > magic_patches.toml

# Items region  
python patch_memory_usage.py \
    --csv items_refs.csv \
    --binary FF8_EN.exe \
    --memory-base 0x01CF8000 \
    > items_patches.toml
```

### Custom Configuration

```bash
# Binary with different base
python patch_memory_usage.py \
    --csv refs.csv \
    --binary custom_game.exe \
    --image-base 0x10000000 \
    --memory-base 0x20000000 \
    --max-offset-range 0x20000 \
    > custom_patches.toml
```

## Technical Details

### Byte Analysis Process

1. **Instruction Disassembly**: Capstone disassembles the instruction
2. **Memory Reference Detection**: Scans instruction bytes for 32-bit addresses
3. **Range Validation**: Checks if addresses are within `max_offset_range` of `memory_base`
4. **Offset Calculation**: Computes `address - memory_base`
5. **Byte Marking**: Replaces address bytes with 'XX XX XX XX' markers

### Output Format Benefits

- **Binary Accuracy**: Exact instruction bytes ensure correct patching
- **Flexible Offsets**: Supports positive and negative offsets
- **Clear Marking**: 'X' markers clearly indicate patch locations
- **Runtime Efficiency**: Direct binary patching without instruction parsing

## License and Contribution

This script is part of the ff8_hook project. See the main README for license and contribution information. 