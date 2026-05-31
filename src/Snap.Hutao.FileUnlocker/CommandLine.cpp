#include "pch.h"
#include "CommandLine.h"

namespace Snap::Hutao::FileUnlocker
{
    CommandLine::CommandLine(std::vector<std::wstring> arguments) :
        m_arguments(std::move(arguments))
    {
    }

    ParsedCommand CommandLine::Parse() const
    {
        if (m_arguments.empty())
        {
            throw std::runtime_error("Command is required.");
        }

        if (m_arguments[0] == L"--help" || m_arguments[0] == L"-h" || m_arguments[0] == L"/?")
        {
            PrintUsage();
            std::exit(0);
        }

        ParsedCommand command;
        std::size_t index = 0;

        if (!IsOption(m_arguments[0]))
        {
            const std::wstring& first = m_arguments[0];
            if (first == L"query" || first == L"query-zone" || first == L"remove-zone" || first == L"query-locks" || first == L"unlock-locks")
            {
                command.Operation = ParseOperation(first);
                index = 1;
            }
            else
            {
                command.Operation = Operation::RemoveZone;
            }
        }

        std::optional<std::filesystem::path> targetPath;
        for (; index < m_arguments.size(); ++index)
        {
            const std::wstring& argument = m_arguments[index];

            if (argument == L"--recursive" || argument == L"-r")
            {
                command.Recursive = true;
                continue;
            }

            if (argument == L"--force" || argument == L"-f")
            {
                command.Force = true;
                continue;
            }

            if (argument == L"--help" || argument == L"-h" || argument == L"/?")
            {
                PrintUsage();
                std::exit(0);
            }

            if (IsOption(argument))
            {
                throw std::runtime_error("Unknown option.");
            }

            if (targetPath.has_value())
            {
                throw std::runtime_error("Only one target path is supported.");
            }

            targetPath = argument;
        }

        if (!targetPath.has_value())
        {
            throw std::runtime_error("Target path is required.");
        }

        command.Path = *targetPath;
        return command;
    }

    const std::vector<std::wstring>& CommandLine::RawArguments() const noexcept
    {
        return m_arguments;
    }

    void CommandLine::PrintUsage()
    {
        std::wcout
            << L"Snap.Hutao.FileUnlocker\n"
            << L"Outputs stable JSON metadata for zone markers and file-lock state.\n\n"
            << L"Usage:\n"
            << L"  Snap.Hutao.FileUnlocker.exe query <absolute-file>\n"
            << L"  Snap.Hutao.FileUnlocker.exe query-zone <absolute-file>\n"
            << L"  Snap.Hutao.FileUnlocker.exe remove-zone [--recursive] <absolute-file-or-directory>\n"
            << L"  Snap.Hutao.FileUnlocker.exe query-locks <absolute-file>\n"
            << L"  Snap.Hutao.FileUnlocker.exe unlock-locks [--force] <absolute-file>\n\n"
            << L"Compatibility:\n"
            << L"  Snap.Hutao.FileUnlocker.exe <absolute-file-or-directory>  equals remove-zone.\n";
    }

    Operation CommandLine::ParseOperation(std::wstring_view value)
    {
        if (value == L"query")
        {
            return Operation::Query;
        }

        if (value == L"query-zone")
        {
            return Operation::QueryZone;
        }

        if (value == L"remove-zone")
        {
            return Operation::RemoveZone;
        }

        if (value == L"query-locks")
        {
            return Operation::QueryLocks;
        }

        if (value == L"unlock-locks")
        {
            return Operation::UnlockLocks;
        }

        throw std::runtime_error("Unknown command.");
    }

    bool CommandLine::IsOption(std::wstring_view value)
    {
        return value.starts_with(L"-") || value.starts_with(L"/");
    }
}
