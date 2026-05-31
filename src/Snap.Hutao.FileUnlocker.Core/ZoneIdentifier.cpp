#include "pch.h"
#include "PathUtils.h"
#include "ZoneIdentifier.h"

namespace Snap::Hutao::FileUnlocker::Core
{
    ZoneQueryResult ZoneIdentifier::Query(const std::filesystem::path& path)
    {
        PathUtils::ValidateAbsolutePath(path);

        ZoneQueryResult result;
        std::error_code statusError;
        result.TargetExists = std::filesystem::exists(path, statusError);
        result.IsDirectory = std::filesystem::is_directory(path, statusError);

        if (!result.TargetExists)
        {
            result.ErrorCode = ERROR_FILE_NOT_FOUND;
            result.ErrorMessage = PathUtils::GetLastErrorMessage(result.ErrorCode);
            return result;
        }

        const std::filesystem::path adsPath = PathUtils::GetZoneIdentifierPath(path);
        HANDLE handle = CreateFileW(
            adsPath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle);
            result.Exists = true;
            return result;
        }

        const DWORD errorCode = GetLastError();
        if (errorCode == ERROR_FILE_NOT_FOUND || errorCode == ERROR_PATH_NOT_FOUND)
        {
            result.Exists = false;
            return result;
        }

        result.ErrorCode = errorCode;
        result.ErrorMessage = PathUtils::GetLastErrorMessage(errorCode);
        return result;
    }

    ZoneRemoveStats ZoneIdentifier::Remove(const std::filesystem::path& path, bool recursive)
    {
        PathUtils::ValidateAbsolutePath(path);

        ZoneRemoveStats stats;
        std::error_code statusError;
        if (std::filesystem::is_regular_file(path, statusError))
        {
            RemoveFileZone(path, stats);
            return stats;
        }

        if (std::filesystem::is_directory(path, statusError))
        {
            if (recursive)
            {
                RemoveDirectoryZones(path, stats);
            }
            else
            {
                RemoveFileZone(path, stats);
            }

            return stats;
        }

        ++stats.FilesFailed;
        return stats;
    }

    void ZoneIdentifier::RemoveFileZone(const std::filesystem::path& path, ZoneRemoveStats& stats)
    {
        ++stats.FilesProcessed;
        const std::filesystem::path adsPath = PathUtils::GetZoneIdentifierPath(path);

        if (DeleteFileW(adsPath.c_str()))
        {
            ++stats.FilesUnblocked;
            return;
        }

        const DWORD errorCode = GetLastError();
        if (errorCode == ERROR_FILE_NOT_FOUND || errorCode == ERROR_PATH_NOT_FOUND)
        {
            ++stats.FilesNoAds;
            return;
        }

        ++stats.FilesFailed;
        if (errorCode == ERROR_ACCESS_DENIED)
        {
            ++stats.PermissionErrors;
        }
    }

    void ZoneIdentifier::RemoveDirectoryZones(const std::filesystem::path& path, ZoneRemoveStats& stats)
    {
        std::error_code iteratorError;
        std::filesystem::directory_iterator iterator{ path, iteratorError };
        if (iteratorError)
        {
            ++stats.FilesFailed;
            if (iteratorError == std::errc::permission_denied)
            {
                ++stats.PermissionErrors;
            }

            return;
        }

        const std::filesystem::directory_iterator end;
        for (; iterator != end; iterator.increment(iteratorError))
        {
            if (iteratorError)
            {
                ++stats.FilesFailed;
                if (iteratorError == std::errc::permission_denied)
                {
                    ++stats.PermissionErrors;
                }

                iteratorError.clear();
                continue;
            }

            const std::filesystem::directory_entry& entry = *iterator;
            std::error_code statusError;
            if (entry.is_regular_file(statusError))
            {
                RemoveFileZone(entry.path(), stats);
            }
            else if (!statusError && entry.is_directory(statusError))
            {
                RemoveDirectoryZones(entry.path(), stats);
            }

            if (statusError)
            {
                ++stats.FilesFailed;
                if (statusError == std::errc::permission_denied)
                {
                    ++stats.PermissionErrors;
                }
            }
        }
    }
}
