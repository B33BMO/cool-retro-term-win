/*
    Minimal mmap/munmap compatibility for Windows.
    Only implements the subset used by History.h/cpp and BlockArray.cpp.
*/
#ifndef COMPAT_MMAN_H
#define COMPAT_MMAN_H

#if defined(Q_OS_WIN) || defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>

// mmap prot flags
#define PROT_READ     0x1
#define PROT_WRITE    0x2

// mmap flags
#define MAP_PRIVATE   0x02
#define MAP_ANON      0x20
#define MAP_ANONYMOUS MAP_ANON
#define MAP_FAILED    ((void*)-1)

// Use 64-bit offset type to avoid UB with 32-bit off_t on Windows
typedef long long mmap_off_t;

static inline void* mmap(void* addr, size_t length, int prot, int flags, int fd, mmap_off_t offset)
{
    Q_UNUSED(addr);

    if (flags & MAP_ANON) {
        // Anonymous mapping: just use VirtualAlloc
        DWORD flProtect = PAGE_READWRITE;
        void* ptr = VirtualAlloc(nullptr, length, MEM_COMMIT | MEM_RESERVE, flProtect);
        return ptr ? ptr : MAP_FAILED;
    }

    // File-backed mapping
    DWORD flProtect;
    DWORD dwDesiredAccess;
    if (prot & PROT_WRITE) {
        flProtect = PAGE_READWRITE;
        dwDesiredAccess = FILE_MAP_WRITE;
    } else {
        flProtect = PAGE_READONLY;
        dwDesiredAccess = FILE_MAP_READ;
    }

    HANDLE hFile = (HANDLE)_get_osfhandle(fd);
    if (hFile == INVALID_HANDLE_VALUE)
        return MAP_FAILED;

    HANDLE hMapping = CreateFileMappingW(hFile, nullptr, flProtect, 0, 0, nullptr);
    if (!hMapping)
        return MAP_FAILED;

    ULARGE_INTEGER off;
    off.QuadPart = (ULONGLONG)offset;
    void* ptr = MapViewOfFile(hMapping, dwDesiredAccess,
                               off.HighPart,
                               off.LowPart,
                               length);
    CloseHandle(hMapping); // The mapping object stays alive while views exist

    return ptr ? ptr : MAP_FAILED;
}

static inline int munmap(void* addr, size_t length)
{
    Q_UNUSED(length);
    // Try UnmapViewOfFile first (for file mappings), then VirtualFree (for anonymous)
    if (UnmapViewOfFile(addr))
        return 0;
    if (VirtualFree(addr, 0, MEM_RELEASE))
        return 0;
    return -1;
}

#endif // Q_OS_WIN
#endif // COMPAT_MMAN_H
