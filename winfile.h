/*
  replacement for unix/C file handling functions on win32 to always have
  64 bit off_t regardless of what the libc implementation uses.
  also supplying replacements for mmap() and friends.
  note that mmap() on 32bit windows will never be able to map files > 2-3 GB,
  due to lack of address space or physical memory.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <winbase.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#ifndef WINFILE_EXPORT
#define WINFILE_EXPORT
#endif

#define MAP_FAILED ((void *) -1)
#define MAP_PRIVATE    0x02
#define MAP_FIXED      0x10
#define PROT_NONE      0
#define PROT_READ      1
#define PROT_WRITE     2
#define PROT_EXEC      4

typedef int64_t WIN_OFF64_T;

#define set_errno(e) errno = e
#ifdef WINFILE_DEBUG
#define translate_errno() translate_errno_dbg(__FILE__, __LINE__)
#else
#define translate_errno() translate_errno_dbg(0, 0)
#endif

static void translate_errno_dbg(char *file, int line) {
    DWORD lastError = GetLastError();
#ifdef WINFILE_DEBUG
    fprintf(stderr, "translate_errno called from %s:%d\n", file, line);
#else
    (void) file; (void) line;
#endif

    switch (lastError) {
        // Common errors
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            set_errno(ENOENT); // No such file or directory
            break;
        case ERROR_ACCESS_DENIED:
            set_errno(EACCES); // Permission denied
            break;
        case ERROR_ALREADY_EXISTS:
        case ERROR_FILE_EXISTS:
            set_errno(EEXIST); // File exists
            break;
        case ERROR_INVALID_HANDLE:
            set_errno(EBADF); // Bad file descriptor
            break;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            set_errno(ENOMEM); // Out of memory
            break;
        case ERROR_INVALID_PARAMETER:
            set_errno(EINVAL); // Invalid argument
            break;
        case ERROR_DISK_FULL:
            set_errno(ENOSPC); // No space left on device
            break;
        case ERROR_HANDLE_EOF:
            set_errno(0); // EOF reached (not strictly an error in POSIX)
            break;

        // Errors specific to CreateFileMappingA and MapViewOfFile(Ex)
        case ERROR_FILE_INVALID:
            set_errno(EBADF); // Bad file descriptor
            break;
        case ERROR_COMMITMENT_LIMIT:
            set_errno(ENOMEM); // Out of memory
            break;
        case ERROR_INVALID_ADDRESS:
            set_errno(EFAULT); // Bad address
            break;
#ifdef EOPNOTSUPP
        case ERROR_NOT_SUPPORTED:
            set_errno(EOPNOTSUPP); // Operation not supported
            break;
#endif
        default:
            set_errno(EIO); // Input/output error for unhandled cases
            break;
    }
}

static DWORD getAllocationGranularity()
{
	struct _SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);
	return SystemInfo.dwAllocationGranularity;
}

WINFILE_EXPORT
void *win_mmap(void *addr, WIN_OFF64_T len, int prot, int flags, HANDLE hFile, WIN_OFF64_T off)
{
	DWORD flProtect;
	DWORD dwDesiredAccess;
	HANDLE map;
	void *mapview;
	DWORD fsizeL, fsizeH;
	uint64_t fsize, alignment_offset, aligned_offset, mapping_size;

	if (!len) {
	l_einval:
		set_errno(EINVAL);
		return MAP_FAILED;
	}
	// Calculate misalignment of the offset.
	alignment_offset = off & (getAllocationGranularity() - 1);
	/*
	Adjust the offset to be aligned to the granularity boundary.
	The adjusted offset represents the nearest lower multiple of the
	granularity that is less than or equal to the original offset.
	This ensures compatibility with the requirements of MapViewOfFileEx.
	https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffileex
	*/
	aligned_offset = off - alignment_offset;

	/*
	Calculate the size of the mapping region.
	The mapping size must include the misaligned portion of the file
	(alignment_offset) to ensure that the desired range can be accessed
	after the mapping.
	The total mapping size is the sum of the requested length (`len`) and the misaligned portion.
	*/
	mapping_size = alignment_offset + len;

	fsizeL = GetFileSize(hFile, &fsizeH);
	if (fsizeL == -1) {
		translate_errno();
		return MAP_FAILED;
	}
	fsize = fsizeL | ((int64_t)fsizeH << 32);

	/*
	Ensure that the mapping size does not exceed the file size.
	If the requested range (alignment_offset + len) extends beyond the
	end of the file,the mapping size is truncated to the remaining
	file size.
	*/
	if ((aligned_offset + mapping_size) > fsize) {
		set_errno(ENXIO);
		return MAP_FAILED;
	}

	/*
	Ensure the mapping size is lower than 4GB on 32bit windows,
	which is necessary because the dwNumberOfBytesToMap parameter is
	SIZE_T (and more memory can't be addressed anyhow).
	*/
	if(sizeof(SIZE_T) < 8 && mapping_size > 0xFFFFFFFFULL)
		goto l_einval;

	switch (prot) {
	case PROT_WRITE | PROT_EXEC:
	case PROT_READ | PROT_WRITE | PROT_EXEC:
		flProtect = PAGE_EXECUTE_READWRITE;
		dwDesiredAccess = SECTION_ALL_ACCESS;
		break;
	case PROT_READ | PROT_WRITE:
		flProtect = PAGE_READWRITE;
		dwDesiredAccess = SECTION_ALL_ACCESS;
		break;
	case PROT_WRITE:
		flProtect = PAGE_READWRITE;
		dwDesiredAccess = FILE_MAP_WRITE;
		break;
	case PROT_READ:
		flProtect = PAGE_READONLY;
		dwDesiredAccess = FILE_MAP_READ;
		break;
	case PROT_EXEC:
		flProtect = PAGE_EXECUTE;
		dwDesiredAccess = FILE_MAP_READ;
		break;
	case PROT_NONE:
		flProtect = PAGE_NOACCESS;
		dwDesiredAccess = FILE_MAP_READ;
		break;
	default:
		goto l_einval;
	}

	if ( flags & MAP_PRIVATE ) {
		flProtect = PAGE_WRITECOPY;
		dwDesiredAccess = FILE_MAP_COPY;
	}

	if (!(flags & MAP_FIXED))
		addr = 0;

	/*
	Create a file mapping object to enable memory mapping of the file.
	A file mapping object represents a portion of a file in memory.
	The object is created with the desired protection level and the
	maximum mapping size.
	*/
	map = CreateFileMappingA(
		hFile,                     // HANDLE hFile: Handle to the file to be mapped.
		NULL,                      // LPSECURITY_ATTRIBUTES lpAttributes: Default security attributes.
		flProtect,                 // DWORD flProtect: Protection level (e.g., PAGE_READWRITE).
		mapping_size >> 32,        // DWORD dwMaximumSizeHigh: High 32 bits of maximum mapping size.
		mapping_size & 0xFFFFFFFF, // DWORD dwMaximumSizeLow: Low 32 bits of maximum mapping size.
		NULL                       // LPCSTR lpName: No named mapping object.
	);
	if (!map) {
		translate_errno();
		return MAP_FAILED;
	}
	// Map a view of the file into the process's address space.
	mapview = MapViewOfFileEx(
		map,                         // HANDLE hFileMappingObject: Handle to the file mapping object.
		dwDesiredAccess,             // DWORD dwDesiredAccess: Desired access level (e.g., FILE_MAP_READ).
		aligned_offset >> 32,        // DWORD dwFileOffsetHigh: High 32 bits of the file offset.
		aligned_offset & 0xFFFFFFFF, // DWORD dwFileOffsetLow: Low 32 bits of the file offset.
		mapping_size,                // SIZE_T dwNumberOfBytesToMap: Number of bytes to map (max 4 GB).
		addr                         // LPVOID lpBaseAddress: Desired starting address (NULL for automatic).
	);

	if(!mapview) {
		translate_errno();
		mapview = MAP_FAILED;
	}
	CloseHandle(map);
	return mapview;
}

WINFILE_EXPORT
int win_msync(void * addr, size_t length, int flags)
{
	(void) flags;
	if (FlushViewOfFile(addr, length))
		return 0;
	translate_errno();
	return -1;
}

WINFILE_EXPORT
int win_munmap(void *addr, size_t len)
{
	(void) len;
	if (UnmapViewOfFile(addr))
		return 0;
	translate_errno();
	return -1;
}

/* return INVALID_HANDLE_VALUE or a valid HANDLE on success. */
WINFILE_EXPORT
HANDLE win_open(const char *pathname, int flags) {
    DWORD access, shareMode;
    DWORD creation = OPEN_EXISTING;
    DWORD attributes = FILE_ATTRIBUTE_NORMAL;

    // ignore O_BINARY as it doesn't affect raw winapi calls
    if (flags & O_WRONLY) {
        access = GENERIC_WRITE;
        shareMode = FILE_SHARE_WRITE; // Only allow shared write access
    } else if (flags & O_RDWR) {
        access = GENERIC_READ | GENERIC_WRITE;
        shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE; // Allow shared read/write access
    } else {
        // Fallback for O_RDONLY (since O_RDONLY is 0)
        access = GENERIC_READ;
        shareMode = FILE_SHARE_READ; // Only allow shared read access
    }

    // Handle O_CREAT, O_TRUNC, and O_EXCL logic
    if (flags & O_CREAT) {
        if (flags & O_EXCL) {
            creation = CREATE_NEW; // Fails if the file exists
        } else if (flags & O_TRUNC) {
            creation = CREATE_ALWAYS; // Create file if it doesn't exist, truncate if it does
        } else {
            creation = OPEN_ALWAYS; // Create file if it doesn't exist
        }
    } else if (flags & O_TRUNC) {
        creation = TRUNCATE_EXISTING; // Truncate file if it exists (fails if it doesn't)
    }

    // Handle O_APPEND
    if (flags & O_APPEND) {
        access |= FILE_APPEND_DATA; // Append data to the end of the file
    }

    HANDLE hFile = CreateFileA(
        pathname,
        access,
        shareMode,
        NULL,                               // Default security attributes
        creation,                      // OPEN_EXISTING by default
        FILE_ATTRIBUTE_NORMAL,              // Normal file
        NULL                                // No template file
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        translate_errno();
    }
    return hFile;
}

WINFILE_EXPORT
ssize_t win_read(HANDLE hFile, void *buffer, size_t count) {
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, buffer, (DWORD)count, &bytesRead, NULL)) {
        translate_errno();
        return -1;
    }
    return (ssize_t)bytesRead;
}

WINFILE_EXPORT
ssize_t win_write(HANDLE hFile, const void *buffer, size_t count) {
    DWORD bytesWritten = 0;
    if (!WriteFile(hFile, buffer, (DWORD)count, &bytesWritten, NULL)) {
        translate_errno();
        return -1;
    }
    return (ssize_t)bytesWritten;
}

WINFILE_EXPORT
WIN_OFF64_T win_lseek(HANDLE hFile, WIN_OFF64_T offset, int whence) {
    LARGE_INTEGER liDistanceToMove;
    LARGE_INTEGER liNewFilePointer;
    DWORD moveMethod;

    liDistanceToMove.QuadPart = offset;

    switch (whence) {
        case SEEK_SET: moveMethod = FILE_BEGIN; break;
        case SEEK_CUR: moveMethod = FILE_CURRENT; break;
        case SEEK_END: moveMethod = FILE_END; break;
        default:
            errno = EINVAL;
            return -1LL;
    }

    if (!SetFilePointerEx(hFile, liDistanceToMove, &liNewFilePointer, moveMethod)) {
        translate_errno();
        return -1LL;
    }
    return liNewFilePointer.QuadPart;
}

WINFILE_EXPORT
void win_close(HANDLE hFile) {
    if (!CloseHandle(hFile)) {
        translate_errno();
    }
}

WINFILE_EXPORT
int win_rename(const char *old, const char *new) {
	BOOL res = MoveFileExA(old, new, MOVEFILE_REPLACE_EXISTING|MOVEFILE_COPY_ALLOWED);
	if(res) return 0;
	translate_errno();
	return -1;
}

WINFILE_EXPORT
WIN_OFF64_T win_filesize(const char *fn) {
	HANDLE fd = win_open(fn, O_RDONLY);
	if(fd == INVALID_HANDLE_VALUE)
		return -1LL;
	WIN_OFF64_T res = win_lseek(fd, 0, SEEK_END);
	win_close(fd);
	return res;
}

#ifdef WINFILE_TEST

int main() {
    const char *filename = "example.txt";

    // Open the file for reading and writing (O_RDWR) in binary mode (O_BINARY)
    HANDLE hFile = win_open(filename, O_RDWR | O_BINARY);
    if (hFile == INVALID_HANDLE_VALUE) {
        perror("Error opening file");
        return 1;
    }

    // Write data to the file
    const char *data = "Hello, Win32!";
    if (win_write(hFile, data, strlen(data)) == -1) {
        perror("Error writing to file");
        win_close(hFile);
        return 1;
    }

    // Seek to the end of the file
    WIN_OFF64_T offset = win_lseek(hFile, 0, SEEK_END);
    if (offset == -1LL) {
        perror("Error seeking in file");
        win_close(hFile);
        return 1;
    }
    printf("File size: %lld bytes\n", offset);

    // Seek to the beginning of the file
    if (win_lseek(hFile, 0, SEEK_SET) == -1LL) {
        perror("Error seeking in file");
        win_close(hFile);
        return 1;
    }

    // Read data from the file
    char buffer[128] = {0};
    if (win_read(hFile, buffer, sizeof(buffer) - 1) == -1) {
        perror("Error reading from file");
        win_close(hFile);
        return 1;
    }

    printf("Read from file: %s\n", buffer);

    // Close the file
    win_close(hFile);

    return 0;
}

#endif
