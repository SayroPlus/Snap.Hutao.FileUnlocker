#include "pch.h"
#include "CommandLine.h"
#include "../Snap.Hutao.FileUnlocker.Core/SnapHutaoFileUnlockerApi.h"

namespace Snap::Hutao::FileUnlocker::Cli
{
    class ExportedString final
    {
    public:
        ExportedString() = default;

        ~ExportedString()
        {
            HutaoFileUnlocker_FreeString(m_value);
        }

        ExportedString(const ExportedString&) = delete;
        ExportedString& operator=(const ExportedString&) = delete;

        [[nodiscard]] wchar_t** Put() noexcept
        {
            return &m_value;
        }

        [[nodiscard]] const wchar_t* Get() const noexcept
        {
            return m_value == nullptr ? L"" : m_value;
        }

    private:
        wchar_t* m_value = nullptr;
    };

    class JsonComposer final
    {
    public:
        JsonComposer() = delete;

        [[nodiscard]] static std::wstring CombineQuery(std::wstring_view path, std::wstring_view zoneJson, std::wstring_view locksJson)
        {
            std::wostringstream stream;
            stream
                << L"{\"schema\":\"snap-hutao-file-unlocker.query.v1\","
                << L"\"operation\":\"query\","
                << L"\"path\":\"" << Escape(path) << L"\","
                << L"\"zone\":" << zoneJson << L","
                << L"\"locks\":" << locksJson << L"}";
            return stream.str();
        }

        [[nodiscard]] static std::wstring Error(std::wstring_view message)
        {
            std::wostringstream stream;
            stream
                << L"{\"schema\":\"snap-hutao-file-unlocker.cli-error.v1\","
                << L"\"operation\":\"cli\","
                << L"\"errorMessage\":\"" << Escape(message) << L"\"}";
            return stream.str();
        }

    private:
        [[nodiscard]] static std::wstring Escape(std::wstring_view value)
        {
            std::wstring result;
            for (wchar_t ch : value)
            {
                switch (ch)
                {
                case L'\\':
                    result.append(L"\\\\");
                    break;
                case L'"':
                    result.append(L"\\\"");
                    break;
                case L'\n':
                    result.append(L"\\n");
                    break;
                case L'\r':
                    result.append(L"\\r");
                    break;
                case L'\t':
                    result.append(L"\\t");
                    break;
                default:
                    result.push_back(ch);
                    break;
                }
            }

            return result;
        }
    };

    class Program final
    {
    public:
        Program() = delete;

        [[nodiscard]] static int Run()
        {
            CommandLine commandLine{ GetArguments() };
            const ParsedCommand command = commandLine.Parse();

            ExportedString output;
            int result = S_OK;

            switch (command.Operation)
            {
            case Operation::Query:
                return RunCombinedQuery(command.Path);
            case Operation::QueryZone:
                result = HutaoFileUnlocker_QueryZoneIdentifier(command.Path.c_str(), output.Put());
                break;
            case Operation::RemoveZone:
                result = HutaoFileUnlocker_RemoveZoneIdentifier(command.Path.c_str(), command.Recursive, output.Put());
                break;
            case Operation::QueryLocks:
                result = HutaoFileUnlocker_QueryFileLocks(command.Path.c_str(), output.Put());
                break;
            case Operation::UnlockLocks:
                result = HutaoFileUnlocker_UnlockFileLocks(command.Path.c_str(), command.Force, output.Put());
                break;
            default:
                throw std::runtime_error("Unsupported operation.");
            }

            std::wcout << output.Get() << L'\n';
            return SUCCEEDED(result) ? 0 : 1;
        }

    private:
        [[nodiscard]] static std::vector<std::wstring> GetArguments()
        {
            int count = 0;
            LPWSTR* rawArguments = CommandLineToArgvW(GetCommandLineW(), &count);
            if (rawArguments == nullptr)
            {
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "CommandLineToArgvW failed");
            }

            std::vector<std::wstring> arguments;
            for (int index = 1; index < count; ++index)
            {
                arguments.emplace_back(rawArguments[index]);
            }

            LocalFree(rawArguments);
            return arguments;
        }

        [[nodiscard]] static int RunCombinedQuery(const std::filesystem::path& path)
        {
            ExportedString zone;
            ExportedString locks;

            const int zoneResult = HutaoFileUnlocker_QueryZoneIdentifier(path.c_str(), zone.Put());
            const int locksResult = HutaoFileUnlocker_QueryFileLocks(path.c_str(), locks.Put());

            std::wcout << JsonComposer::CombineQuery(path.native(), zone.Get(), locks.Get()) << L'\n';
            return SUCCEEDED(zoneResult) && SUCCEEDED(locksResult) ? 0 : 1;
        }
    };
}

int wmain()
{
    try
    {
        return Snap::Hutao::FileUnlocker::Cli::Program::Run();
    }
    catch (const std::exception& ex)
    {
        const std::wstring message = std::filesystem::path{ ex.what() }.native();
        std::wcerr << Snap::Hutao::FileUnlocker::Cli::JsonComposer::Error(message) << L'\n';
        return 1;
    }
}
