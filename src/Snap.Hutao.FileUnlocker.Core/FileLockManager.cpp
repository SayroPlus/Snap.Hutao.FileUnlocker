#include "pch.h"
#include "FileLockManager.h"
#include "PathUtils.h"

namespace Snap::Hutao::FileUnlocker::Core
{
    using NtStatus = LONG;

    constexpr NtStatus NtStatusSuccess = 0x00000000;
    constexpr NtStatus NtStatusInfoLengthMismatch = static_cast<NtStatus>(0xC0000004);
    constexpr NtStatus NtStatusBufferOverflow = static_cast<NtStatus>(0x80000005);

    enum class SystemInformationClass : ULONG
    {
        SystemHandleInformation = 16,
        SystemHandleInformationEx = 64,
    };

    enum class ObjectInformationClass : ULONG
    {
        ObjectNameInformation = 1,
        ObjectTypeInformation = 2,
    };

    enum class SectionInformationClass : ULONG
    {
        SectionBasicInformation = 0,
    };

    struct UnicodeString
    {
        USHORT Length;
        USHORT MaximumLength;
        PWSTR Buffer;
    };

    struct ObjectNameInformation
    {
        UnicodeString Name;
    };

    struct ObjectTypeInformation
    {
        UnicodeString Name;
    };

    struct SystemHandle
    {
        HANDLE ProcessId;
        BYTE ObjectType;
        BYTE Flags;
        WORD Handle;
        PVOID Address;
        DWORD GrantedAccess;
    };

    struct SystemHandleInformation
    {
        DWORD HandleCount;
        SystemHandle Handles[1];
    };

    struct SystemHandleEx
    {
        PVOID Object;
        HANDLE ProcessId;
        HANDLE Handle;
        ULONG GrantedAccess;
        USHORT CreatorBackTraceIndex;
        USHORT ObjectTypeIndex;
        ULONG HandleAttributes;
        ULONG Reserved;
    };

    struct SystemHandleInformationEx
    {
        ULONG_PTR HandleCount;
        ULONG_PTR Reserved;
        SystemHandleEx Handles[1];
    };

    struct SectionBasicInformation
    {
        PVOID BaseAddress;
        ULONG Attributes;
        LARGE_INTEGER Size;
    };

    using NtQuerySystemInformationFunction = NtStatus(WINAPI*)(
        SystemInformationClass systemInformationClass,
        PVOID systemInformation,
        ULONG systemInformationLength,
        PULONG returnLength);

    using NtQueryObjectFunction = NtStatus(WINAPI*)(
        HANDLE handle,
        ObjectInformationClass objectInformationClass,
        PVOID objectInformation,
        ULONG objectInformationLength,
        PULONG returnLength);

    using NtQuerySectionFunction = NtStatus(WINAPI*)(
        HANDLE sectionHandle,
        SectionInformationClass sectionInformationClass,
        PVOID sectionInformation,
        ULONG sectionInformationLength,
        PULONG returnLength);

    class UniqueHandle final
    {
    public:
        UniqueHandle() noexcept = default;

        explicit UniqueHandle(HANDLE handle) noexcept :
            m_handle(handle)
        {
        }

        ~UniqueHandle()
        {
            Reset();
        }

        UniqueHandle(const UniqueHandle&) = delete;
        UniqueHandle& operator=(const UniqueHandle&) = delete;

        [[nodiscard]] HANDLE Get() const noexcept
        {
            return m_handle;
        }

        [[nodiscard]] HANDLE* Put() noexcept
        {
            Reset();
            return &m_handle;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE;
        }

        void Reset(HANDLE handle = nullptr) noexcept
        {
            if (m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE)
            {
                CloseHandle(m_handle);
            }

            m_handle = handle;
        }

    private:
        HANDLE m_handle = nullptr;
    };

    class ScopedDebugPrivilege final
    {
    public:
        ScopedDebugPrivilege()
        {
            m_state.Attempted = true;
            m_state.Scope = L"current-process-token";
            m_state.Mode = L"try-enable-existing-token-privilege";
            m_state.Enabled = EnablePrivilege(SE_DEBUG_NAME);
            if (!m_state.Enabled)
            {
                m_state.Status = GetPrivilegeStatus(m_state.ErrorCode);
                m_state.ErrorMessage = GetPrivilegeErrorMessage(m_state.ErrorCode);
                m_state.RequiresElevation = m_state.ErrorCode == ERROR_NOT_ALL_ASSIGNED
                    || m_state.ErrorCode == ERROR_PRIVILEGE_NOT_HELD
                    || m_state.ErrorCode == ERROR_ACCESS_DENIED;
            }
            else if (m_state.PreviouslyEnabled)
            {
                m_state.Status = L"already-enabled";
            }
            else
            {
                m_state.Status = L"enabled";
            }
        }

        ~ScopedDebugPrivilege()
        {
            if (m_restoreRequired)
            {
                RestorePrivilege();
            }
        }

        ScopedDebugPrivilege(const ScopedDebugPrivilege&) = delete;
        ScopedDebugPrivilege& operator=(const ScopedDebugPrivilege&) = delete;

        [[nodiscard]] const DebugPrivilegeState& State() const noexcept
        {
            return m_state;
        }

    private:
        [[nodiscard]] bool EnablePrivilege(LPCWSTR privilegeName)
        {
            m_state.ErrorCode = ERROR_SUCCESS;
            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, m_token.Put()))
            {
                m_state.ErrorCode = GetLastError();
                return false;
            }

            LUID luid{};
            if (!LookupPrivilegeValueW(nullptr, privilegeName, &luid))
            {
                m_state.ErrorCode = GetLastError();
                return false;
            }

            TOKEN_PRIVILEGES privileges{};
            privileges.PrivilegeCount = 1;
            privileges.Privileges[0].Luid = luid;
            privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

            m_previousPrivilegeSize = sizeof(m_previousPrivileges);
            AdjustTokenPrivileges(m_token.Get(), FALSE, &privileges, sizeof(m_previousPrivileges), &m_previousPrivileges, &m_previousPrivilegeSize);
            m_state.ErrorCode = GetLastError();
            if (m_state.ErrorCode != ERROR_SUCCESS)
            {
                return false;
            }

            m_state.AssignedToToken = true;
            if (m_previousPrivileges.PrivilegeCount > 0)
            {
                m_state.PreviouslyEnabled = (m_previousPrivileges.Privileges[0].Attributes & SE_PRIVILEGE_ENABLED) == SE_PRIVILEGE_ENABLED;
                m_restoreRequired = true;
            }

            return true;
        }

        void RestorePrivilege() noexcept
        {
            AdjustTokenPrivileges(m_token.Get(), FALSE, &m_previousPrivileges, 0, nullptr, nullptr);
        }

        [[nodiscard]] static std::wstring GetPrivilegeStatus(DWORD errorCode)
        {
            switch (errorCode)
            {
            case ERROR_NOT_ALL_ASSIGNED:
                return L"not-assigned-to-token";
            case ERROR_PRIVILEGE_NOT_HELD:
                return L"not-held-by-process";
            case ERROR_ACCESS_DENIED:
                return L"access-denied";
            default:
                return L"error";
            }
        }

        [[nodiscard]] static std::wstring GetPrivilegeErrorMessage(DWORD errorCode)
        {
            switch (errorCode)
            {
            case ERROR_SUCCESS:
                return {};
            case ERROR_NOT_ALL_ASSIGNED:
                return L"SeDebugPrivilege is not assigned to the current process token.";
            case ERROR_PRIVILEGE_NOT_HELD:
                return L"The current process does not hold SeDebugPrivilege.";
            case ERROR_ACCESS_DENIED:
                return L"Access denied while enabling SeDebugPrivilege.";
            default:
                return PathUtils::GetLastErrorMessage(errorCode);
            }
        }

        UniqueHandle m_token;
        TOKEN_PRIVILEGES m_previousPrivileges{};
        DWORD m_previousPrivilegeSize = 0;
        bool m_restoreRequired = false;
        DebugPrivilegeState m_state;
    };

    struct HandleRecord
    {
        DWORD ProcessId = 0;
        HANDLE HandleValue = nullptr;
        bool IsSection = false;
        std::wstring Path;
    };

    struct MappedViewRecord
    {
        DWORD ProcessId = 0;
        PVOID BaseAddress = nullptr;
        std::wstring Path;
    };

    struct PeUnlockResult
    {
        unsigned long long UnloadedModuleCount = 0;
        unsigned long long TerminatedProcessCount = 0;
    };

    enum class FileKind
    {
        Unknown,
        Normal,
        Exe,
        Dll,
    };

    class NativeFileHandleScanner final
    {
    public:
        explicit NativeFileHandleScanner(std::filesystem::path path) :
            m_targetPath(NormalizePath(PathUtils::GetFullPath(path)))
        {
            HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
            if (ntdll == nullptr)
            {
                ntdll = LoadLibraryW(L"ntdll.dll");
            }

            if (ntdll == nullptr)
            {
                throw std::runtime_error("Unable to load ntdll.dll.");
            }

            m_querySystemInformation = reinterpret_cast<NtQuerySystemInformationFunction>(GetProcAddress(ntdll, "NtQuerySystemInformation"));
            m_queryObject = reinterpret_cast<NtQueryObjectFunction>(GetProcAddress(ntdll, "NtQueryObject"));
            m_querySection = reinterpret_cast<NtQuerySectionFunction>(GetProcAddress(ntdll, "NtQuerySection"));

            if (m_querySystemInformation == nullptr || m_queryObject == nullptr)
            {
                throw std::runtime_error("Required native object query exports are unavailable.");
            }
        }

        [[nodiscard]] std::vector<HandleRecord> Scan()
        {
            std::vector<HandleRecord> records;

            if (!ScanWithClass<SystemHandleInformationEx>(SystemInformationClass::SystemHandleInformationEx, records))
            {
                const bool fallbackSucceeded = ScanWithClass<SystemHandleInformation>(SystemInformationClass::SystemHandleInformation, records);
                (void)fallbackSucceeded;
            }

            return records;
        }

        [[nodiscard]] std::vector<MappedViewRecord> ScanMappedViews()
        {
            std::vector<MappedViewRecord> records;
            UniqueHandle snapshot{ CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) };
            if (!snapshot || snapshot.Get() == INVALID_HANDLE_VALUE)
            {
                return records;
            }

            PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);
            if (!Process32FirstW(snapshot.Get(), &entry))
            {
                return records;
            }

            do
            {
                UniqueHandle process{ OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID) };
                if (!process)
                {
                    continue;
                }

                AppendMappedViews(process.Get(), entry.th32ProcessID, records);
            }
            while (Process32NextW(snapshot.Get(), &entry));

            return records;
        }

        [[nodiscard]] bool CloseRemoteHandle(const HandleRecord& record)
        {
            UniqueHandle process{ OpenProcess(PROCESS_DUP_HANDLE, FALSE, record.ProcessId) };
            if (!process)
            {
                return false;
            }

            UniqueHandle duplicate;
            return DuplicateHandle(
                process.Get(),
                record.HandleValue,
                GetCurrentProcess(),
                duplicate.Put(),
                0,
                FALSE,
                DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE) != FALSE;
        }

        [[nodiscard]] bool UnmapRemoteView(const MappedViewRecord& record)
        {
            UniqueHandle process{ OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION, FALSE, record.ProcessId) };
            if (!process)
            {
                return false;
            }

            HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
            if (kernel32 == nullptr)
            {
                return false;
            }

            FARPROC unmapViewOfFile = GetProcAddress(kernel32, "UnmapViewOfFile");
            if (unmapViewOfFile == nullptr)
            {
                return false;
            }

            UniqueHandle thread{ CreateRemoteThread(
                process.Get(),
                nullptr,
                0,
                reinterpret_cast<LPTHREAD_START_ROUTINE>(unmapViewOfFile),
                record.BaseAddress,
                0,
                nullptr) };

            if (!thread)
            {
                return false;
            }

            WaitForSingleObject(thread.Get(), 1000);
            DWORD exitCode = 0;
            return GetExitCodeThread(thread.Get(), &exitCode) && exitCode != 0;
        }

        [[nodiscard]] PeUnlockResult UnholdPeImage()
        {
            PeUnlockResult result;
            const FileKind fileKind = CheckFileKind();
            if (fileKind != FileKind::Exe && fileKind != FileKind::Dll)
            {
                return result;
            }

            UniqueHandle processSnapshot{ CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) };
            if (!processSnapshot || processSnapshot.Get() == INVALID_HANDLE_VALUE)
            {
                return result;
            }

            PROCESSENTRY32W processEntry{};
            processEntry.dwSize = sizeof(processEntry);
            if (!Process32FirstW(processSnapshot.Get(), &processEntry))
            {
                return result;
            }

            do
            {
                UniqueHandle process{ OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE | PROCESS_CREATE_THREAD, FALSE, processEntry.th32ProcessID) };
                if (!process)
                {
                    continue;
                }

                if (fileKind == FileKind::Exe)
                {
                    const std::wstring imagePath = NormalizePath(GetProcessImagePath(processEntry.th32ProcessID));
                    if (imagePath == m_targetPath && TerminateProcess(process.Get(), 1))
                    {
                        ++result.TerminatedProcessCount;
                    }

                    continue;
                }

                UniqueHandle moduleSnapshot{ CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processEntry.th32ProcessID) };
                if (!moduleSnapshot || moduleSnapshot.Get() == INVALID_HANDLE_VALUE)
                {
                    continue;
                }

                MODULEENTRY32W moduleEntry{};
                moduleEntry.dwSize = sizeof(moduleEntry);
                if (!Module32FirstW(moduleSnapshot.Get(), &moduleEntry))
                {
                    continue;
                }

                do
                {
                    if (NormalizePath(moduleEntry.szExePath) == m_targetPath && RemoteFreeLibrary(process.Get(), moduleEntry.modBaseAddr))
                    {
                        ++result.UnloadedModuleCount;
                    }
                }
                while (Module32NextW(moduleSnapshot.Get(), &moduleEntry));
            }
            while (Process32NextW(processSnapshot.Get(), &processEntry));

            return result;
        }

        [[nodiscard]] static std::wstring GetProcessImagePath(DWORD processId)
        {
            UniqueHandle process{ OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId) };
            if (!process)
            {
                process.Reset(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId));
            }

            if (!process)
            {
                return {};
            }

            std::wstring path(MAX_PATH, L'\0');
            DWORD size = static_cast<DWORD>(path.size());
            if (QueryFullProcessImageNameW(process.Get(), 0, path.data(), &size))
            {
                path.resize(size);
                return path;
            }

            size = static_cast<DWORD>(path.size());
            if (GetProcessImageFileNameW(process.Get(), path.data(), size) > 0)
            {
                path.resize(wcslen(path.c_str()));
                return DevicePathToDrivePath(path);
            }

            return {};
        }

    private:
        void AppendMappedViews(HANDLE process, DWORD processId, std::vector<MappedViewRecord>& records)
        {
            BOOL isWow64 = FALSE;
            IsWow64Process(process, &isWow64);

            SYSTEM_INFO systemInfo{};
            GetNativeSystemInfo(&systemInfo);
            const bool is64BitOs = systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64
                || systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64;
            const unsigned long long maxAddress = (is64BitOs && !isWow64) ? 0x80000000000ULL : 0x100000000ULL;

            MEMORY_BASIC_INFORMATION memory{};
            PVOID lastAllocationBase = nullptr;

            for (unsigned long long address = 0; address < maxAddress;)
            {
                if (!VirtualQueryEx(process, reinterpret_cast<LPCVOID>(address), &memory, sizeof(memory)))
                {
                    break;
                }

                const unsigned long long regionSize = memory.RegionSize == 0 ? 0x1000 : memory.RegionSize;
                if (memory.Type == MEM_MAPPED && lastAllocationBase != memory.AllocationBase)
                {
                    std::wstring mappedPath(MAX_PATH, L'\0');
                    const DWORD length = GetMappedFileNameW(process, memory.BaseAddress, mappedPath.data(), static_cast<DWORD>(mappedPath.size()));
                    if (length > 0)
                    {
                        mappedPath.resize(length);
                        mappedPath = DevicePathToDrivePath(mappedPath);
                        if (PathContainsTarget(mappedPath))
                        {
                            records.push_back({ processId, memory.BaseAddress, std::move(mappedPath) });
                        }
                    }

                    lastAllocationBase = memory.AllocationBase;
                }

                if (address + regionSize <= address)
                {
                    break;
                }

                address += regionSize;
            }
        }

        [[nodiscard]] FileKind CheckFileKind() const
        {
            const std::wstring openPath = ToWin32ExtendedPath(m_targetPath);
            UniqueHandle file{ CreateFileW(openPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) };
            if (!file)
            {
                return FileKind::Unknown;
            }

            UniqueHandle mapping{ CreateFileMappingW(file.Get(), nullptr, PAGE_READONLY, 0, 0, nullptr) };
            if (!mapping)
            {
                return FileKind::Unknown;
            }

            void* view = MapViewOfFile(mapping.Get(), FILE_MAP_READ, 0, 0, 0);
            if (view == nullptr)
            {
                return FileKind::Unknown;
            }

            FileKind kind = FileKind::Normal;
            const auto* dosHeader = static_cast<const IMAGE_DOS_HEADER*>(view);
            if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE)
            {
                const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(static_cast<const std::byte*>(view) + dosHeader->e_lfanew);
                if (ntHeaders->Signature == IMAGE_NT_SIGNATURE)
                {
                    kind = (ntHeaders->FileHeader.Characteristics & IMAGE_FILE_DLL) != 0 ? FileKind::Dll : FileKind::Exe;
                }
            }

            UnmapViewOfFile(view);
            return kind;
        }

        [[nodiscard]] static std::wstring ToWin32ExtendedPath(const std::wstring& path)
        {
            if (path.starts_with(LR"(\\?\)"))
            {
                return path;
            }

            if (path.starts_with(LR"(\\)"))
            {
                std::wstring extendedPath = LR"(\\?\UNC\)";
                extendedPath.append(path.substr(2));
                return extendedPath;
            }

            std::wstring extendedPath = LR"(\\?\)";
            extendedPath.append(path);
            return extendedPath;
        }

        [[nodiscard]] bool RemoteFreeLibrary(HANDLE process, PVOID moduleBaseAddress) const
        {
            HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
            if (ntdll == nullptr)
            {
                return false;
            }

            FARPROC ldrUnloadDll = GetProcAddress(ntdll, "LdrUnloadDll");
            if (ldrUnloadDll == nullptr)
            {
                return false;
            }

            UniqueHandle thread{ CreateRemoteThread(
                process,
                nullptr,
                0,
                reinterpret_cast<LPTHREAD_START_ROUTINE>(ldrUnloadDll),
                moduleBaseAddress,
                0,
                nullptr) };

            if (!thread)
            {
                return false;
            }

            WaitForSingleObject(thread.Get(), 1000);
            DWORD exitCode = 0;
            return GetExitCodeThread(thread.Get(), &exitCode) && exitCode == ERROR_SUCCESS;
        }

        template <typename TSystemHandleInformation>
        [[nodiscard]] bool ScanWithClass(SystemInformationClass informationClass, std::vector<HandleRecord>& records)
        {
            std::unique_ptr<std::byte[]> buffer;
            ULONG length = 0x10000;

            for (;;)
            {
                buffer = std::make_unique<std::byte[]>(length);
                ULONG returnLength = 0;
                const NtStatus status = m_querySystemInformation(informationClass, buffer.get(), length, &returnLength);
                if (status == NtStatusInfoLengthMismatch || status == NtStatusBufferOverflow)
                {
                    length = returnLength > length ? returnLength : length * 2;
                    continue;
                }

                if (status < 0)
                {
                    return false;
                }

                break;
            }

            const TSystemHandleInformation* information = reinterpret_cast<const TSystemHandleInformation*>(buffer.get());
            const ULONG_PTR count = GetHandleCount(information);

            for (ULONG_PTR index = 0; index < count; ++index)
            {
                const DWORD processId = GetProcessIdFromEntry(information, index);
                if (processId == 0)
                {
                    continue;
                }

                const HANDLE handleValue = GetHandleValueFromEntry(information, index);
                UniqueHandle process{ OpenProcess(PROCESS_DUP_HANDLE, FALSE, processId) };
                if (!process)
                {
                    continue;
                }

                UniqueHandle duplicatedHandle;
                if (!DuplicateHandle(process.Get(), handleValue, GetCurrentProcess(), duplicatedHandle.Put(), 0, FALSE, DUPLICATE_SAME_ACCESS))
                {
                    continue;
                }

                if (IsUnsafeDeviceHandle(duplicatedHandle.Get()))
                {
                    continue;
                }

                const std::wstring typeName = QueryObjectTypeName(duplicatedHandle.Get());
                if (_wcsicmp(typeName.c_str(), L"File") == 0)
                {
                    std::wstring handlePath = QueryObjectPath(duplicatedHandle.Get());
                    if (!handlePath.empty() && PathContainsTarget(handlePath))
                    {
                        records.push_back({ processId, handleValue, false, std::move(handlePath) });
                    }
                }
                else if (_wcsicmp(typeName.c_str(), L"Section") == 0 && IsFileSection(duplicatedHandle.Get()))
                {
                    records.push_back({ processId, handleValue, true, {} });
                }
            }

            return true;
        }

        [[nodiscard]] static ULONG_PTR GetHandleCount(const SystemHandleInformationEx* information)
        {
            return information->HandleCount;
        }

        [[nodiscard]] static ULONG_PTR GetHandleCount(const SystemHandleInformation* information)
        {
            return information->HandleCount;
        }

        [[nodiscard]] static DWORD GetProcessIdFromEntry(const SystemHandleInformationEx* information, ULONG_PTR index)
        {
            return static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(information->Handles[index].ProcessId));
        }

        [[nodiscard]] static DWORD GetProcessIdFromEntry(const SystemHandleInformation* information, ULONG_PTR index)
        {
            return static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(information->Handles[index].ProcessId));
        }

        [[nodiscard]] static HANDLE GetHandleValueFromEntry(const SystemHandleInformationEx* information, ULONG_PTR index)
        {
            return information->Handles[index].Handle;
        }

        [[nodiscard]] static HANDLE GetHandleValueFromEntry(const SystemHandleInformation* information, ULONG_PTR index)
        {
            return reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(information->Handles[index].Handle));
        }

        [[nodiscard]] bool IsUnsafeDeviceHandle(HANDLE handle) const
        {
            UniqueHandle mapping{ CreateFileMappingW(handle, nullptr, PAGE_READONLY, 0, 0, nullptr) };
            return !mapping && GetLastError() == ERROR_BAD_EXE_FORMAT;
        }

        [[nodiscard]] std::wstring QueryObjectTypeName(HANDLE handle) const
        {
            std::vector<std::byte> buffer(0x1000);
            ULONG returnLength = 0;
            NtStatus status = m_queryObject(
                handle,
                ObjectInformationClass::ObjectTypeInformation,
                buffer.data(),
                static_cast<ULONG>(buffer.size()),
                &returnLength);

            if ((status == NtStatusInfoLengthMismatch || status == NtStatusBufferOverflow) && returnLength > buffer.size())
            {
                buffer.resize(returnLength);
                status = m_queryObject(
                    handle,
                    ObjectInformationClass::ObjectTypeInformation,
                    buffer.data(),
                    static_cast<ULONG>(buffer.size()),
                    &returnLength);
            }

            if (status < 0)
            {
                return {};
            }

            const ObjectTypeInformation* information = reinterpret_cast<const ObjectTypeInformation*>(buffer.data());
            if (information->Name.Buffer == nullptr || information->Name.Length == 0)
            {
                return {};
            }

            return std::wstring{ information->Name.Buffer, information->Name.Length / sizeof(wchar_t) };
        }

        [[nodiscard]] std::wstring QueryObjectPath(HANDLE handle) const
        {
            ULONG returnLength = 0;
            ObjectNameInformation stackInformation{};
            NtStatus status = m_queryObject(
                handle,
                ObjectInformationClass::ObjectNameInformation,
                &stackInformation,
                sizeof(stackInformation),
                &returnLength);

            if (status != NtStatusSuccess && status != NtStatusBufferOverflow && status != NtStatusInfoLengthMismatch)
            {
                return {};
            }

            std::vector<std::byte> buffer(returnLength + sizeof(wchar_t));
            status = m_queryObject(
                handle,
                ObjectInformationClass::ObjectNameInformation,
                buffer.data(),
                static_cast<ULONG>(buffer.size()),
                &returnLength);

            if (status < 0)
            {
                return {};
            }

            const ObjectNameInformation* information = reinterpret_cast<const ObjectNameInformation*>(buffer.data());
            if (information->Name.Buffer == nullptr || information->Name.Length == 0)
            {
                return {};
            }

            std::wstring path{ information->Name.Buffer, information->Name.Length / sizeof(wchar_t) };
            return DevicePathToDrivePath(path);
        }

        [[nodiscard]] bool IsFileSection(HANDLE handle) const
        {
            if (m_querySection == nullptr)
            {
                return false;
            }

            SectionBasicInformation information{};
            const NtStatus status = m_querySection(
                handle,
                SectionInformationClass::SectionBasicInformation,
                &information,
                sizeof(information),
                nullptr);

            return status >= 0 && information.Attributes == SEC_FILE;
        }

        [[nodiscard]] bool PathContainsTarget(const std::wstring& handlePath) const
        {
            const std::wstring normalizedHandlePath = NormalizePath(handlePath);
            if (normalizedHandlePath.size() < m_targetPath.size())
            {
                return false;
            }

            if (normalizedHandlePath.compare(0, m_targetPath.size(), m_targetPath) != 0)
            {
                return false;
            }

            return normalizedHandlePath.size() == m_targetPath.size()
                || m_targetPath.ends_with(L"\\")
                || normalizedHandlePath[m_targetPath.size()] == L'\\';
        }

        [[nodiscard]] static std::wstring NormalizePath(std::wstring path)
        {
            path = DevicePathToDrivePath(path);

            if (path.starts_with(LR"(\\?\)"))
            {
                path.erase(0, 4);
            }

            std::replace(path.begin(), path.end(), L'/', L'\\');
            std::transform(path.begin(), path.end(), path.begin(), [](wchar_t value)
            {
                return static_cast<wchar_t>(towlower(value));
            });

            return path;
        }

        [[nodiscard]] static std::wstring DevicePathToDrivePath(std::wstring path)
        {
            if (path.empty())
            {
                return path;
            }

            if (_wcsnicmp(path.c_str(), L"\\SystemRoot", 11) == 0)
            {
                std::wstring windowsDirectory(MAX_PATH, L'\0');
                const UINT length = GetWindowsDirectoryW(windowsDirectory.data(), static_cast<UINT>(windowsDirectory.size()));
                windowsDirectory.resize(length);
                path.replace(0, 11, windowsDirectory);
                return path;
            }

            if (_wcsnicmp(path.c_str(), LR"(\??\)", 4) == 0)
            {
                path.erase(0, 4);
                return path;
            }

            std::map<std::wstring, std::wstring> mappings = BuildDeviceDriveMap();
            for (const auto& [devicePath, drivePath] : mappings)
            {
                if (_wcsnicmp(path.c_str(), devicePath.c_str(), devicePath.size()) == 0)
                {
                    path.replace(0, devicePath.size(), drivePath);
                    return path;
                }
            }

            return path;
        }

        [[nodiscard]] static std::map<std::wstring, std::wstring> BuildDeviceDriveMap()
        {
            std::map<std::wstring, std::wstring> mappings;
            DWORD driveMask = GetLogicalDrives();

            for (wchar_t drive = L'A'; drive <= L'Z'; ++drive)
            {
                if ((driveMask & 1) != 0)
                {
                    wchar_t drivePath[] = { drive, L':', L'\0' };
                    std::wstring devicePath(MAX_PATH, L'\0');
                    const DWORD length = QueryDosDeviceW(drivePath, devicePath.data(), static_cast<DWORD>(devicePath.size()));
                    if (length > 0)
                    {
                        devicePath.resize(wcslen(devicePath.c_str()));
                        if (GetDriveTypeW(drivePath) == DRIVE_REMOTE)
                        {
                            NormalizeRemoteDevicePath(devicePath);
                        }

                        mappings[devicePath] = drivePath;
                    }
                }

                driveMask >>= 1;
            }

            return mappings;
        }

        static void NormalizeRemoteDevicePath(std::wstring& devicePath)
        {
            std::size_t slashCount = 0;
            for (std::size_t index = 0; index < devicePath.size(); ++index)
            {
                if (devicePath[index] != L'\\')
                {
                    continue;
                }

                ++slashCount;
                if (slashCount != 3)
                {
                    continue;
                }

                std::size_t nextSlash = index + 1;
                while (nextSlash < devicePath.size() && devicePath[nextSlash] != L'\\')
                {
                    ++nextSlash;
                }

                if (nextSlash < devicePath.size())
                {
                    devicePath.erase(index + 1, nextSlash - index - 1);
                }

                return;
            }
        }

        std::wstring m_targetPath;
        NtQuerySystemInformationFunction m_querySystemInformation = nullptr;
        NtQueryObjectFunction m_queryObject = nullptr;
        NtQuerySectionFunction m_querySection = nullptr;
    };

    LockQueryResult FileLockManager::Query(const std::filesystem::path& path)
    {
        PathUtils::ValidateAbsolutePath(path);
        return QueryHandles(path);
    }

    LockUnlockResult FileLockManager::Unlock(const std::filesystem::path& path, bool)
    {
        PathUtils::ValidateAbsolutePath(path);
        return CloseHandles(path);
    }

    LockQueryResult FileLockManager::QueryHandles(const std::filesystem::path& path)
    {
        LockQueryResult result;

        NativeFileHandleScanner scanner{ path };
        ScopedDebugPrivilege debugPrivilege;
        result.DebugPrivilege = debugPrivilege.State();
        const std::vector<HandleRecord> records = scanner.Scan();
        const std::vector<MappedViewRecord> mappedViews = scanner.ScanMappedViews();

        std::map<DWORD, LockingProcess> processes;
        for (const HandleRecord& record : records)
        {
            if (record.IsSection)
            {
                continue;
            }

            LockingProcess& process = processes[record.ProcessId];
            process.ProcessId = record.ProcessId;
            ++process.OpenHandleCount;
        }

        for (const MappedViewRecord& mappedView : mappedViews)
        {
            LockingProcess& process = processes[mappedView.ProcessId];
            process.ProcessId = mappedView.ProcessId;
            ++process.MappedViewCount;
        }

        for (const HandleRecord& record : records)
        {
            if (!record.IsSection || processes.find(record.ProcessId) == processes.end())
            {
                continue;
            }

            ++processes[record.ProcessId].SectionHandleCount;
        }

        for (auto& [processId, process] : processes)
        {
            process.ImagePath = NativeFileHandleScanner::GetProcessImagePath(processId);
            process.ApplicationName = std::filesystem::path{ process.ImagePath }.filename().native();
            result.Processes.push_back(std::move(process));
        }

        result.InUse = !result.Processes.empty();
        return result;
    }

    LockUnlockResult FileLockManager::CloseHandles(const std::filesystem::path& path)
    {
        LockUnlockResult result;

        NativeFileHandleScanner scanner{ path };
        ScopedDebugPrivilege debugPrivilege;
        const std::vector<HandleRecord> records = scanner.Scan();
        const std::vector<MappedViewRecord> mappedViews = scanner.ScanMappedViews();

        std::map<DWORD, LockingProcess> beforeProcesses;
        for (const MappedViewRecord& mappedView : mappedViews)
        {
            LockingProcess& process = beforeProcesses[mappedView.ProcessId];
            process.ProcessId = mappedView.ProcessId;
            ++process.MappedViewCount;

            if (scanner.UnmapRemoteView(mappedView))
            {
                ++process.UnmappedViewCount;
                ++result.UnmappedViewCount;
            }
        }

        for (const HandleRecord& record : records)
        {
            const bool shouldCloseSection = record.IsSection && beforeProcesses.find(record.ProcessId) != beforeProcesses.end();
            const bool shouldCloseOpenHandle = !record.IsSection;
            if (!shouldCloseSection && !shouldCloseOpenHandle)
            {
                continue;
            }

            LockingProcess& process = beforeProcesses[record.ProcessId];
            process.ProcessId = record.ProcessId;

            if (record.IsSection)
            {
                ++process.SectionHandleCount;
            }
            else
            {
                ++process.OpenHandleCount;
            }

            if (scanner.CloseRemoteHandle(record))
            {
                if (record.IsSection)
                {
                    ++process.ClosedSectionHandleCount;
                    ++result.ClosedSectionHandleCount;
                }
                else
                {
                    ++process.ClosedOpenHandleCount;
                    ++result.ClosedOpenHandleCount;
                }
            }
        }

        const PeUnlockResult peUnlockResult = scanner.UnholdPeImage();
        result.UnloadedModuleCount = peUnlockResult.UnloadedModuleCount;
        result.TerminatedProcessCount = peUnlockResult.TerminatedProcessCount;

        for (auto& [processId, process] : beforeProcesses)
        {
            process.ImagePath = NativeFileHandleScanner::GetProcessImagePath(processId);
            process.ApplicationName = std::filesystem::path{ process.ImagePath }.filename().native();
            result.Before.Processes.push_back(std::move(process));
        }

        result.Before.InUse = !result.Before.Processes.empty();
        result.Before.DebugPrivilege = debugPrivilege.State();
        Sleep(100);
        result.After = QueryHandles(path);
        return result;
    }
}
