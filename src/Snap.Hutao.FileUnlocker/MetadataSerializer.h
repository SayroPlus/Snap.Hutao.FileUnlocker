#pragma once

#include "pch.h"
#include "FileLockManager.h"
#include "JsonWriter.h"
#include "ZoneIdentifier.h"

namespace Snap::Hutao::FileUnlocker::Core
{
    class MetadataSerializer final
    {
    public:
        MetadataSerializer() = delete;

        [[nodiscard]] static std::wstring ZoneQuery(const std::filesystem::path& path, const ZoneQueryResult& result);
        [[nodiscard]] static std::wstring ZoneRemove(const std::filesystem::path& path, const ZoneRemoveStats& stats);
        [[nodiscard]] static std::wstring LockQuery(const std::filesystem::path& path, const LockQueryResult& result);
        [[nodiscard]] static std::wstring LockUnlock(const std::filesystem::path& path, bool force, const LockUnlockResult& result);
        [[nodiscard]] static std::wstring Error(const std::filesystem::path& path, std::wstring_view operation, DWORD errorCode, std::wstring_view message);

    private:
        static void WriteLockQuery(JsonWriter& writer, std::wstring_view name, const LockQueryResult& result);
    };
}
