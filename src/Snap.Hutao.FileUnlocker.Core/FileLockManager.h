#pragma once

#include "pch.h"

namespace Snap::Hutao::FileUnlocker::Core
{
    struct LockingProcess
    {
        DWORD ProcessId = 0;
        unsigned long long OpenHandleCount = 0;
        unsigned long long SectionHandleCount = 0;
        unsigned long long MappedViewCount = 0;
        unsigned long long ClosedOpenHandleCount = 0;
        unsigned long long ClosedSectionHandleCount = 0;
        unsigned long long UnmappedViewCount = 0;
        std::wstring ApplicationName;
        std::wstring ImagePath;
    };

    struct DebugPrivilegeState
    {
        bool Attempted = false;
        bool AutoElevationAttempted = false;
        bool AssignedToToken = false;
        bool PreviouslyEnabled = false;
        bool Enabled = false;
        bool RequiresElevation = false;
        DWORD ErrorCode = ERROR_SUCCESS;
        std::wstring Scope;
        std::wstring Mode;
        std::wstring Status;
        std::wstring ErrorMessage;
    };

    struct LockQueryResult
    {
        DWORD ErrorCode = ERROR_SUCCESS;
        bool InUse = false;
        bool RebootRequired = false;
        std::wstring ErrorMessage;
        DebugPrivilegeState DebugPrivilege;
        std::vector<LockingProcess> Processes;
    };

    struct LockUnlockResult
    {
        LockQueryResult Before;
        LockQueryResult After;
        DWORD UnlockErrorCode = ERROR_SUCCESS;
        unsigned long long ClosedOpenHandleCount = 0;
        unsigned long long ClosedSectionHandleCount = 0;
        unsigned long long UnmappedViewCount = 0;
        unsigned long long UnloadedModuleCount = 0;
        unsigned long long TerminatedProcessCount = 0;
        bool ForcedTerminationAttempted = false;
        std::wstring UnlockErrorMessage;
    };

    class FileLockManager final
    {
    public:
        FileLockManager() = delete;

        [[nodiscard]] static LockQueryResult Query(const std::filesystem::path& path);
        [[nodiscard]] static LockUnlockResult Unlock(const std::filesystem::path& path, bool force);

    private:
        [[nodiscard]] static LockQueryResult QueryHandles(const std::filesystem::path& path);
        [[nodiscard]] static LockUnlockResult CloseHandles(const std::filesystem::path& path);
    };
}
