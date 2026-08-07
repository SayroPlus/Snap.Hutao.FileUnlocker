#pragma once

#include "pch.h"

namespace Snap::Hutao::FileUnlocker
{
    enum class Operation
    {
        Query,
        QueryZone,
        RemoveZone,
        QueryLocks,
        UnlockLocks,
    };

    struct ParsedCommand
    {
        Operation Operation = Operation::Query;
        std::filesystem::path Path;
        bool Recursive = false;
        bool Force = false;
    };

    class CommandLine
    {
    public:
        explicit CommandLine(std::vector<std::wstring> arguments);

        [[nodiscard]] ParsedCommand Parse() const;
        [[nodiscard]] const std::vector<std::wstring>& RawArguments() const noexcept;

        static void PrintUsage();

    private:
        [[nodiscard]] static Operation ParseOperation(std::wstring_view value);
        [[nodiscard]] static bool IsOption(std::wstring_view value);

        std::vector<std::wstring> m_arguments;
    };
}
