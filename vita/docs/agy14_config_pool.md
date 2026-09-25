# Config String Pool Corruption Root Cause

## The Symptom
The `CrowdMood::ReadConfig` function reads empty strings for every `_STRING` config entry (e.g., `RandomHeckle`). The string pool memory itself was verified as intact, but when the values were read back, the references were corrupted.

## Root Cause
The root cause is an ABI misalignment between the decompiled code's hardcoded offsets and the compiler's structure packing on the Vita (ARM EABI). 

The decompiled source in `src/NL/nlConfig.cpp` iterates through the `Config` hash table using a hardcoded `12`-byte stride:
```cpp
u32 offset = idx * 12;
if (mTvpHash[idx].tag == NULL || nlStrICmp(mTvpHash[idx].tag, tag) == 0)
{
    return *(TagValuePair*)((char*)mTvpHash + offset);
}
```

On the GameCube (PowerPC EABI), `sizeof(TagValuePair)` evaluates exactly to `12` bytes because the nested `enum Type` takes 4 bytes. 
However, on the Vita, the ARM EABI allows enums to be packed into the smallest matching integer type. Consequently, `enum Type` takes `1` byte, and `sizeof(TagValuePair)` evaluates to `9` bytes. 

Because of this size mismatch, `mTvpHash[idx].tag` evaluates to memory at `idx * 9`, while the lookup incorrectly returns a pointer to `(char*)mTvpHash + (idx * 12)`. When `CrowdMood::ReadConfig` calls `GetConfigFloat` (which triggers `Config::Set` internally when a value is not found), the lookup writes new `TagValuePair` data to the misaligned `idx * 12` offset. This blind write completely clobbers the `Value.s` pointer field of neighboring overlapping structs—including the perfectly valid `RandomHeckle` entries populated earlier by `LoadFromFile`. 

## The Fix
To enforce a 12-byte `TagValuePair` across all platforms and align it with the GameCube original behavior, a padding element `_FORCE_32BIT = 0x7FFFFFFF` is added under an `#ifdef TARGET_VITA` block to `enum Type` in `include/NL/nlConfig.h`.

```cpp
enum Type
{
    _BOOL = 0,
    _INT = 1,
    _FLOAT = 2,
    _STRING = 3,
#ifdef TARGET_VITA
    _FORCE_32BIT = 0x7FFFFFFF,
#endif
};
```
This forces the compiler to allocate 4 bytes for the enum, bringing `sizeof(TagValuePair)` back to `12` bytes without touching the GameCube codebase path.

## Evidence

A `sizeof` check injected before the heckle loop confirms the compiler packing anomaly on Vita:
```
SIZEOF sizeof(TVP)=9 sizeof(Type)=1 sizeof(Value)=4
```

After adding the `_FORCE_32BIT` padding, the sizes match the expected offsets:
```
SIZEOF sizeof(TVP)=12 sizeof(Type)=4 sizeof(Value)=4
```
And the strings correctly read back from the hash table without corruption:
```
HECKLE RandomHeckle1 sample=[audio/data/Streams/Heckler DSP Files/HECKLER_Yell_Mono_01.dsp] type=3 count=0 ptr=0x87faa6c3 string_ptr=0x87faa6c3 addr=0x87fb4e44
```
