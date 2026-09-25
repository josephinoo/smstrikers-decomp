#ifndef _NLFILEGC_H_
#define _NLFILEGC_H_

#include "NL/nlFile.h"
#include "types.h"

// Vita-facing GameCube file abstraction interface
class GCFile : public nlFile
{
public:
    GCFile()
        : m_Position(0)
    {
        PendingAsync.m_Count = 0;
    }
    virtual ~GCFile() {}

    virtual u32 FileSize(unsigned int* size) = 0;
    virtual void Read(void* buffer, unsigned int size) = 0;
    virtual s32 GetReadStatus() { return 0; }
    virtual void ReadAsync(void* buffer, unsigned long length, unsigned long offset)
    {
        (void)buffer;
        (void)length;
        (void)offset;
    }
    virtual u32 GetDiscPosition() { return 0; }

    Counter PendingAsync;
    unsigned long m_Position;
};

nlFile* nlOpen(const char* fileName);
void nlSeek(nlFile* file, unsigned int offset, unsigned long origin);
u32 nlGetFilePosition(nlFile* file);
void nlFlushFileCash();
void nlInitFileSystem();
void nlServiceFileSystem();

void* nlLoadEntireFileToVirtualMemory(const char* fileName, int* size, unsigned int transferSize, void* target, eAllocType allocType);
void* nlReadToVirtualMemory(nlFile* file, void* buffer, unsigned int size, unsigned int chunkSize);
void nlAsyncLoadFileToVirtualMemory(nlFile* file, int size, void* buffer, ReadAsyncCallback callback, unsigned long alignment);
void nlReadAsync(nlFile* file, void* buffer, unsigned int size, ReadAsyncCallback callback, unsigned long uParam);
bool nlAsyncReadsPending(nlFile* file);
void nlCancelPendingAsyncReads(nlFile* pFile, void (*callback)(nlFile*, void*, unsigned int, unsigned long, void (*)(nlFile*, void*, unsigned int, unsigned long)));

#include "NL/nlFunction.h"
void nlRegHandleDVDMessageCB(const Function<void(int)>& cb);
void nlRegHandleDVDAllClearCB(const Function<void(int)>& cb);
void nlRegCheckForResetFromFSCB(const Function<FnVoidVoid>& cb);

#endif // _NLFILEGC_H_
