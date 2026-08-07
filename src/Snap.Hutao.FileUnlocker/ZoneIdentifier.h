#pragma once

#include "pch.h"

namespace Snap::Hutao::FileUnlocker::Core
{
    struct ZoneQueryResult
    {
        bool Exists = false;
        bool TargetExists = false;
        bool IsDirectory = false;
        DWORD ErrorCode = ERROR_SUCCESS;
        std::wstring ErrorMessage;
    };

    struct ZoneRemoveStats
    {
        unsigned long long FilesProcessed = 0;
        unsigned long long FilesUnblocked = 0;
        unsigned long long FilesNoAds = 0;
        unsigned long long FilesFailed = 0;
        unsigned long long PermissionErrors = 0;
    };

    class ZoneIdentifier final
    {
    public:
        ZoneIdentifier() = delete;

        [[nodiscard]] static ZoneQueryResult Query(const std::filesystem::path& path);
        [[nodiscard]] static ZoneRemoveStats Remove(const std::filesystem::path& path, bool recursive);

    private:
        static void RemoveFileZone(const std::filesystem::path& path, ZoneRemoveStats& stats);
        static void RemoveDirectoryZones(const std::filesystem::path& path, ZoneRemoveStats& stats);
    };
}
