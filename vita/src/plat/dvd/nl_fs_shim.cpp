#include "NL/nlFile.h"
#include "NL/nlFileGC.h"
#include "plat_abi.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

namespace {

class VitaFile : public GCFile
{
public:
    explicit VitaFile(void* handle)
        : m_handle(handle)
    {
        m_Position = 0;
        PendingAsync.m_Count = 0;
    }

    virtual ~VitaFile() override
    {
        if (m_handle != nullptr) {
            vita_file_close(m_handle);
            m_handle = nullptr;
        }
    }

    // Return the exact size, not the GameCube's 32-byte-rounded sector count:
    // nlLoadEntireFile allocates *size bytes but reads the returned count, so
    // rounding up here overruns the block and corrupts the allocator's free
    // list. Text loaders do not need the padding — InitFileStringData already
    // allocates one extra byte and zero-fills before copying.
    virtual u32 FileSize(unsigned int* size) override
    {
        long sz = vita_file_size(m_handle);
        if (sz < 0) {
            sz = 0;
        }
        if (size != nullptr) {
            *size = static_cast<unsigned int>(sz);
        }
        return static_cast<u32>(sz);
    }

    virtual void Read(void* buffer, unsigned int size) override
    {
        if (m_handle != nullptr && buffer != nullptr && size > 0) {
            size_t n = vita_file_read(m_handle, buffer, size);
            if (n < size) {
                // Past EOF: the GameCube's sector padding read as zeroes, and
                // text loaders depend on that to terminate the string.
                std::memset(static_cast<char*>(buffer) + n, 0, size - n);
            }
            m_Position += static_cast<unsigned long>(n);
        }
    }

    virtual s32 GetReadStatus() override
    {
        return 0; // Read complete / ready
    }

    virtual void ReadAsync(void* buffer, unsigned long length, unsigned long offset) override
    {
        Seek(static_cast<unsigned int>(offset), 0);
        Read(buffer, static_cast<unsigned int>(length));
    }

    virtual u32 GetDiscPosition() override
    {
        return static_cast<u32>(m_Position);
    }

    bool Seek(unsigned int offset, unsigned long origin)
    {
        if (m_handle == nullptr) {
            return false;
        }
        switch (origin) {
        case 0: // SEEK_SET
            if (vita_file_seek(m_handle, static_cast<long>(offset), SEEK_SET) == 0) {
                long pos = vita_file_tell(m_handle);
                if (pos >= 0) {
                    m_Position = static_cast<unsigned long>(pos);
                }
                return true;
            }
            break;
        case 1: // SEEK_CUR
            if (vita_file_seek(m_handle, static_cast<long>(offset), SEEK_CUR) == 0) {
                long pos = vita_file_tell(m_handle);
                if (pos >= 0) {
                    m_Position = static_cast<unsigned long>(pos);
                }
                return true;
            }
            break;
        case 2: { // SEEK_END (matches GCN convention: FileSize - offset)
            long sz = vita_file_size(m_handle);
            long target = (sz >= static_cast<long>(offset)) ? (sz - static_cast<long>(offset)) : 0;
            if (vita_file_seek(m_handle, target, SEEK_SET) == 0) {
                long pos = vita_file_tell(m_handle);
                if (pos >= 0) {
                    m_Position = static_cast<unsigned long>(pos);
                }
                return true;
            }
            break;
        }
        default:
            break;
        }
        return false;
    }

    void* GetHandle() const { return m_handle; }

private:
    void* m_handle;
};

} // namespace

// nlFile's own constructor/destructor come from src/NL/nlFile.cpp, which the
// Vita build compiles. Defining them here too would duplicate its vtable.

// nlOpen is the backend half of the file API: the portable façade lives in
// nlFile.cpp, but the GameCube supplied this from nlFileGC.cpp, which we
// replace with ux0:data/smstrikers/.
nlFile* nlOpen(const char* fileName)
{
    void* handle = vita_file_open(fileName);
    if (handle == nullptr) {
        return nullptr;
    }
    return new VitaFile(handle);
}

// Minimal weak memory allocation helpers for NL file loads
void nlSeek(nlFile* file, unsigned int offset, unsigned long origin)
{
    if (file != nullptr) {
        static_cast<VitaFile*>(file)->Seek(offset, origin);
    }
}

u32 nlGetFilePosition(nlFile* file)
{
    if (file != nullptr) {
        return static_cast<GCFile*>(file)->m_Position;
    }
    return 0;
}

void nlFlushFileCash()
{
    // No-op for Vita VFS
}

void nlInitFileSystem()
{
    // Mounting handled by p0_mount_data()
}

void nlServiceFileSystem()
{
    // Synchronous reads on Vita, no async event pump required
}

void* nlLoadEntireFileToVirtualMemory(const char* fileName, int* size, unsigned int transferSize, void* target, eAllocType allocType)
{
    (void)transferSize;
    if (target != nullptr) {
        nlFile* f = nlOpen(fileName);
        if (f == nullptr) {
            if (size != nullptr) {
                *size = 0;
            }
            return nullptr;
        }
        unsigned int fs = 0;
        f->FileSize(&fs);
        f->Read(target, fs);
        nlClose(f);
        if (size != nullptr) {
            *size = static_cast<int>(fs);
        }
        return target;
    }

    unsigned long outSize = 0;
    void* buf = nlLoadEntireFile(fileName, &outSize, 32, allocType);
    if (size != nullptr) {
        *size = static_cast<int>(outSize);
    }
    return buf;
}

void nlReadAsync(nlFile* file, void* buffer, unsigned int size, ReadAsyncCallback callback, unsigned long uParam)
{
    if (file != nullptr) {
        file->Read(buffer, size);
        if (callback != nullptr) {
            // GC AsyncManager advances m_pBuffer during the read, so the
            // completion callback receives end = start+size. Audio CBs then do
            // (pData - Length) before nlFree. Passing start here freed heap
            // metadata (Invalid write @0xfffda224 → freelist death → black menu).
            callback(file, (char*)buffer + size, size, uParam);
        }
    }
}

bool nlAsyncReadsPending(nlFile* file)
{
    (void)file;
    return false;
}

void nlCancelPendingAsyncReads(nlFile* pFile, void (*callback)(nlFile*, void*, unsigned int, unsigned long, void (*)(nlFile*, void*, unsigned int, unsigned long)))
{
    (void)pFile;
    (void)callback;
}

void* nlReadToVirtualMemory(nlFile* file, void* buffer, unsigned int size, unsigned int /*chunkSize*/)
{
    /* Vita: read straight into destination (GC path used chunked DMA into VRAME). */
    nlRead(file, buffer, size);
    return buffer;
}

void nlAsyncLoadFileToVirtualMemory(nlFile* file, int size, void* buffer, ReadAsyncCallback callback, unsigned long userData)
{
    /* Vita: sync read then fire callback (GC was async ARAM DMA). */
    if (file != nullptr && buffer != nullptr && size > 0) {
        nlRead(file, buffer, static_cast<unsigned int>(size));
    }
    if (callback != nullptr) {
        callback(file, buffer, static_cast<unsigned int>(size), userData);
    }
}
