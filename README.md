# Snap.Hutao.FileUnlocker

C++ Windows utility for file metadata and lock state automation.

The project has two layers:

- `Snap.Hutao.FileUnlocker.Core.dll`: reusable C ABI for other languages.
- `Snap.Hutao.FileUnlocker.exe`: CLI wrapper that writes stable single-line JSON.

Network-origin unblocking removes the NTFS `Zone.Identifier` alternate data stream, matching PowerShell `Unblock-File`.

File occupation detection and release follows the Ring3 approach used by `ez8-co/unlocker`: enumerate system handles through `NtQuerySystemInformation`, duplicate handles into this process, inspect object names through `NtQueryObject`, and close matching remote handles with `DuplicateHandle(..., DUPLICATE_CLOSE_SOURCE)`. It also checks mapped file views with `VirtualQueryEx`/`GetMappedFileName` and uses remote `UnmapViewOfFile` for mapped views. PE handling follows the same model: loaded DLLs are released through remote `LdrUnloadDll`, and running EXE images may be terminated.

The lock APIs try to enable `SeDebugPrivilege` only in the current process token before scanning. This is a scoped `AdjustTokenPrivileges` call: it can enable the privilege only when that privilege is already assigned to the current token, and it restores the previous token state afterward. It does not request elevation, relaunch, or show UAC. Callers should inspect `locks.debugPrivilege.requiresElevation`; if it is `true`, rerun the host process elevated to get fuller process coverage.

## Usage

```cmd
Snap.Hutao.FileUnlocker.exe query "C:\path\to\file.exe"
Snap.Hutao.FileUnlocker.exe query-zone "C:\path\to\file.exe"
Snap.Hutao.FileUnlocker.exe remove-zone "C:\path\to\file.exe"
Snap.Hutao.FileUnlocker.exe remove-zone --recursive "C:\Downloads"
Snap.Hutao.FileUnlocker.exe query-locks "C:\path\to\file.exe"
Snap.Hutao.FileUnlocker.exe unlock-locks "C:\path\to\file.exe"
```

`query` combines zone-marker and lock-state metadata. Other commands return focused schemas:

- `snap-hutao-file-unlocker.zone-query.v1`
- `snap-hutao-file-unlocker.zone-remove.v1`
- `snap-hutao-file-unlocker.lock-query.v1`
- `snap-hutao-file-unlocker.lock-unlock.v1`

Lock query objects include:

```json
"debugPrivilege": {
  "attempted": true,
  "scope": "current-process-token",
  "mode": "try-enable-existing-token-privilege",
  "status": "not-assigned-to-token",
  "autoElevationAttempted": false,
  "assignedToToken": false,
  "previouslyEnabled": false,
  "enabled": false,
  "requiresElevation": true,
  "errorCode": 1300,
  "errorMessage": "SeDebugPrivilege is not assigned to the current process token."
}
```

Calling the executable with only a path remains compatible with the previous behavior and runs `remove-zone`.

## DLL ABI

The public C ABI is declared in `src\Snap.Hutao.FileUnlocker.Core\SnapHutaoFileUnlockerApi.h`.

```cpp
int __stdcall HutaoFileUnlocker_QueryZoneIdentifier(const wchar_t* path, wchar_t** metadataJson);
int __stdcall HutaoFileUnlocker_RemoveZoneIdentifier(const wchar_t* path, bool recursive, wchar_t** metadataJson);
int __stdcall HutaoFileUnlocker_QueryFileLocks(const wchar_t* path, wchar_t** metadataJson);
int __stdcall HutaoFileUnlocker_UnlockFileLocks(const wchar_t* path, bool force, wchar_t** metadataJson);
void __stdcall HutaoFileUnlocker_FreeString(wchar_t* value);
```

Returned strings are allocated with `CoTaskMemAlloc`; callers must free them with `HutaoFileUnlocker_FreeString`.

## Build

Open `src\Snap.Hutao.FileUnlocker.sln` in Visual Studio 2022, or `src\Snap.Hutao.FileUnlocker.slnx` in newer Visual Studio versions.

The project defaults to `v143` for VS2022 compatibility. To switch project files between VS2022 and v2026-style toolsets:

```powershell
.\scripts\set-platform-toolset.ps1 -Toolset v143
.\scripts\set-platform-toolset.ps1 -Toolset v145
```
