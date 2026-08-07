#pragma once

#include "pch.h"

namespace Snap::Hutao::FileUnlocker::Core
{
    class PathUtils final
    {
    public:
        PathUtils() = delete;

        static void ValidateAbsolutePath(const std::filesystem::path& path);
        [[nodiscard]] static std::filesystem::path GetZoneIdentifierPath(const std::filesystem::path& path);
        [[nodiscard]] static std::wstring GetFullPath(const std::filesystem::path& path);
        [[nodiscard]] static std::wstring GetLastErrorMessage(DWORD errorCode);
    };
}
