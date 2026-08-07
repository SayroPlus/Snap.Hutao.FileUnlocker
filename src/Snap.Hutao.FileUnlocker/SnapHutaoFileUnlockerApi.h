#pragma once

#ifdef SNAP_HUTAO_FILE_UNLOCKER_EXPORTS
#define SNAP_HUTAO_FILE_UNLOCKER_API __declspec(dllexport)
#else
#define SNAP_HUTAO_FILE_UNLOCKER_API __declspec(dllimport)
#endif

extern "C"
{
    SNAP_HUTAO_FILE_UNLOCKER_API int __stdcall HutaoFileUnlocker_QueryZoneIdentifier(const wchar_t* path, wchar_t** metadataJson);
    SNAP_HUTAO_FILE_UNLOCKER_API int __stdcall HutaoFileUnlocker_RemoveZoneIdentifier(const wchar_t* path, bool recursive, wchar_t** metadataJson);
    SNAP_HUTAO_FILE_UNLOCKER_API int __stdcall HutaoFileUnlocker_QueryFileLocks(const wchar_t* path, wchar_t** metadataJson);
    SNAP_HUTAO_FILE_UNLOCKER_API int __stdcall HutaoFileUnlocker_UnlockFileLocks(const wchar_t* path, bool force, wchar_t** metadataJson);
    SNAP_HUTAO_FILE_UNLOCKER_API void __stdcall HutaoFileUnlocker_FreeString(wchar_t* value);
}
