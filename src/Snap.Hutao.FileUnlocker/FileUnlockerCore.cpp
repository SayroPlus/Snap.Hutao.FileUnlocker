#include "pch.h"
#include "FileLockManager.h"
#include "MetadataSerializer.h"
#include "SnapHutaoFileUnlockerApi.h"
#include "ZoneIdentifier.h"

namespace Snap::Hutao::FileUnlocker::Core
{
    class ExportString final
    {
    public:
        ExportString() = delete;

        static int Write(std::wstring_view value, wchar_t** output) noexcept
        {
            if (output == nullptr)
            {
                return E_POINTER;
            }

            *output = nullptr;

            const std::size_t bytes = (value.size() + 1) * sizeof(wchar_t);
            wchar_t* buffer = static_cast<wchar_t*>(CoTaskMemAlloc(bytes));
            if (buffer == nullptr)
            {
                return E_OUTOFMEMORY;
            }

            memcpy(buffer, value.data(), value.size() * sizeof(wchar_t));
            buffer[value.size()] = L'\0';
            *output = buffer;
            return S_OK;
        }

        static int WriteError(const wchar_t* path, std::wstring_view operation, const std::exception& exception, wchar_t** output) noexcept
        {
            const std::filesystem::path safePath = path == nullptr ? std::filesystem::path{} : std::filesystem::path{ path };
            const std::wstring message = std::filesystem::path{ exception.what() }.native();
            return Write(MetadataSerializer::Error(safePath, operation, ERROR_INVALID_FUNCTION, message), output);
        }
    };
}

extern "C" int __stdcall HutaoFileUnlocker_QueryZoneIdentifier(const wchar_t* path, wchar_t** metadataJson)
{
    using namespace Snap::Hutao::FileUnlocker::Core;

    try
    {
        if (path == nullptr)
        {
            return E_POINTER;
        }

        const std::filesystem::path targetPath{ path };
        const ZoneQueryResult result = ZoneIdentifier::Query(targetPath);
        return ExportString::Write(MetadataSerializer::ZoneQuery(targetPath, result), metadataJson);
    }
    catch (const std::exception& exception)
    {
        return ExportString::WriteError(path, L"query-zone", exception, metadataJson);
    }
}

extern "C" int __stdcall HutaoFileUnlocker_RemoveZoneIdentifier(const wchar_t* path, bool recursive, wchar_t** metadataJson)
{
    using namespace Snap::Hutao::FileUnlocker::Core;

    try
    {
        if (path == nullptr)
        {
            return E_POINTER;
        }

        const std::filesystem::path targetPath{ path };
        const ZoneRemoveStats stats = ZoneIdentifier::Remove(targetPath, recursive);
        return ExportString::Write(MetadataSerializer::ZoneRemove(targetPath, stats), metadataJson);
    }
    catch (const std::exception& exception)
    {
        return ExportString::WriteError(path, L"remove-zone", exception, metadataJson);
    }
}

extern "C" int __stdcall HutaoFileUnlocker_QueryFileLocks(const wchar_t* path, wchar_t** metadataJson)
{
    using namespace Snap::Hutao::FileUnlocker::Core;

    try
    {
        if (path == nullptr)
        {
            return E_POINTER;
        }

        const std::filesystem::path targetPath{ path };
        const LockQueryResult result = FileLockManager::Query(targetPath);
        return ExportString::Write(MetadataSerializer::LockQuery(targetPath, result), metadataJson);
    }
    catch (const std::exception& exception)
    {
        return ExportString::WriteError(path, L"query-locks", exception, metadataJson);
    }
}

extern "C" int __stdcall HutaoFileUnlocker_UnlockFileLocks(const wchar_t* path, bool force, wchar_t** metadataJson)
{
    using namespace Snap::Hutao::FileUnlocker::Core;

    try
    {
        if (path == nullptr)
        {
            return E_POINTER;
        }

        const std::filesystem::path targetPath{ path };
        const LockUnlockResult result = FileLockManager::Unlock(targetPath, force);
        return ExportString::Write(MetadataSerializer::LockUnlock(targetPath, force, result), metadataJson);
    }
    catch (const std::exception& exception)
    {
        return ExportString::WriteError(path, L"unlock-locks", exception, metadataJson);
    }
}

extern "C" void __stdcall HutaoFileUnlocker_FreeString(wchar_t* value)
{
    CoTaskMemFree(value);
}
