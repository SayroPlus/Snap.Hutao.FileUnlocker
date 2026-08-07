#include "pch.h"
#include "PathUtils.h"

namespace Snap::Hutao::FileUnlocker::Core
{
    void PathUtils::ValidateAbsolutePath(const std::filesystem::path& path)
    {
        if (path.empty())
        {
            throw std::invalid_argument("Path is required.");
        }

        if (!path.is_absolute())
        {
            throw std::invalid_argument("Path must be absolute on Windows.");
        }

        const std::wstring value = path.native();
        if (value.find_first_of(L"<>|\0", 0) != std::wstring::npos)
        {
            throw std::invalid_argument("Path contains invalid characters.");
        }
    }

    std::filesystem::path PathUtils::GetZoneIdentifierPath(const std::filesystem::path& path)
    {
        ValidateAbsolutePath(path);

        std::wstring adsPath = path.native();
        adsPath.append(L":Zone.Identifier");
        return std::filesystem::path{ adsPath };
    }

    std::wstring PathUtils::GetFullPath(const std::filesystem::path& path)
    {
        std::error_code error;
        const std::filesystem::path absolutePath = std::filesystem::absolute(path, error);
        if (error)
        {
            return path.native();
        }

        return absolutePath.native();
    }

    std::wstring PathUtils::GetLastErrorMessage(DWORD errorCode)
    {
        wchar_t* buffer = nullptr;
        const DWORD length = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            errorCode,
            0,
            reinterpret_cast<LPWSTR>(&buffer),
            0,
            nullptr);

        if (length == 0 || buffer == nullptr)
        {
            std::wostringstream stream;
            stream << L"Win32 error " << errorCode;
            return stream.str();
        }

        std::wstring message{ buffer, length };
        LocalFree(buffer);

        while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L'.'))
        {
            message.pop_back();
        }

        return message;
    }
}
