#include "pch.h"
#include "JsonWriter.h"
#include "MetadataSerializer.h"
#include "PathUtils.h"

namespace Snap::Hutao::FileUnlocker::Core
{
    std::wstring MetadataSerializer::ZoneQuery(const std::filesystem::path& path, const ZoneQueryResult& result)
    {
        JsonWriter writer;
        writer.BeginObject();
        writer.StringProperty(L"schema", L"snap-hutao-file-unlocker.zone-query.v1");
        writer.StringProperty(L"operation", L"query-zone");
        writer.StringProperty(L"path", PathUtils::GetFullPath(path));
        writer.BoolProperty(L"targetExists", result.TargetExists);
        writer.BoolProperty(L"isDirectory", result.IsDirectory);
        writer.BoolProperty(L"hasZoneIdentifier", result.Exists);
        writer.UIntProperty(L"errorCode", result.ErrorCode);
        writer.StringProperty(L"errorMessage", result.ErrorMessage);
        writer.EndObject();
        return writer.ToString();
    }

    std::wstring MetadataSerializer::ZoneRemove(const std::filesystem::path& path, const ZoneRemoveStats& stats)
    {
        JsonWriter writer;
        writer.BeginObject();
        writer.StringProperty(L"schema", L"snap-hutao-file-unlocker.zone-remove.v1");
        writer.StringProperty(L"operation", L"remove-zone");
        writer.StringProperty(L"path", PathUtils::GetFullPath(path));
        writer.BeginObjectProperty(L"stats");
        writer.UIntProperty(L"filesProcessed", stats.FilesProcessed);
        writer.UIntProperty(L"filesUnblocked", stats.FilesUnblocked);
        writer.UIntProperty(L"filesNoAds", stats.FilesNoAds);
        writer.UIntProperty(L"filesFailed", stats.FilesFailed);
        writer.UIntProperty(L"permissionErrors", stats.PermissionErrors);
        writer.EndObject();
        writer.BoolProperty(L"success", stats.FilesFailed == 0);
        writer.EndObject();
        return writer.ToString();
    }

    std::wstring MetadataSerializer::LockQuery(const std::filesystem::path& path, const LockQueryResult& result)
    {
        JsonWriter writer;
        writer.BeginObject();
        writer.StringProperty(L"schema", L"snap-hutao-file-unlocker.lock-query.v1");
        writer.StringProperty(L"operation", L"query-locks");
        writer.StringProperty(L"path", PathUtils::GetFullPath(path));
        WriteLockQuery(writer, L"locks", result);
        writer.EndObject();
        return writer.ToString();
    }

    std::wstring MetadataSerializer::LockUnlock(const std::filesystem::path& path, bool force, const LockUnlockResult& result)
    {
        JsonWriter writer;
        writer.BeginObject();
        writer.StringProperty(L"schema", L"snap-hutao-file-unlocker.lock-unlock.v1");
        writer.StringProperty(L"operation", L"unlock-locks");
        writer.StringProperty(L"path", PathUtils::GetFullPath(path));
        writer.BoolProperty(L"force", force);
        writer.UIntProperty(L"unlockErrorCode", result.UnlockErrorCode);
        writer.StringProperty(L"unlockErrorMessage", result.UnlockErrorMessage);
        writer.BoolProperty(L"forcedTerminationAttempted", result.ForcedTerminationAttempted);
        writer.UIntProperty(L"closedOpenHandleCount", result.ClosedOpenHandleCount);
        writer.UIntProperty(L"closedSectionHandleCount", result.ClosedSectionHandleCount);
        writer.UIntProperty(L"unmappedViewCount", result.UnmappedViewCount);
        writer.UIntProperty(L"unloadedModuleCount", result.UnloadedModuleCount);
        writer.UIntProperty(L"terminatedProcessCount", result.TerminatedProcessCount);
        WriteLockQuery(writer, L"before", result.Before);
        WriteLockQuery(writer, L"after", result.After);
        writer.BoolProperty(L"success", result.UnlockErrorCode == ERROR_SUCCESS && !result.After.InUse);
        writer.EndObject();
        return writer.ToString();
    }

    std::wstring MetadataSerializer::Error(const std::filesystem::path& path, std::wstring_view operation, DWORD errorCode, std::wstring_view message)
    {
        JsonWriter writer;
        writer.BeginObject();
        writer.StringProperty(L"schema", L"snap-hutao-file-unlocker.error.v1");
        writer.StringProperty(L"operation", operation);
        writer.StringProperty(L"path", PathUtils::GetFullPath(path));
        writer.UIntProperty(L"errorCode", errorCode);
        writer.StringProperty(L"errorMessage", message);
        writer.EndObject();
        return writer.ToString();
    }

    void MetadataSerializer::WriteLockQuery(JsonWriter& writer, std::wstring_view name, const LockQueryResult& result)
    {
        writer.BeginObjectProperty(name);
        writer.BoolProperty(L"inUse", result.InUse);
        writer.BoolProperty(L"rebootRequired", result.RebootRequired);
        writer.UIntProperty(L"errorCode", result.ErrorCode);
        writer.StringProperty(L"errorMessage", result.ErrorMessage);
        writer.BeginObjectProperty(L"debugPrivilege");
        writer.BoolProperty(L"attempted", result.DebugPrivilege.Attempted);
        writer.StringProperty(L"scope", result.DebugPrivilege.Scope);
        writer.StringProperty(L"mode", result.DebugPrivilege.Mode);
        writer.StringProperty(L"status", result.DebugPrivilege.Status);
        writer.BoolProperty(L"autoElevationAttempted", result.DebugPrivilege.AutoElevationAttempted);
        writer.BoolProperty(L"assignedToToken", result.DebugPrivilege.AssignedToToken);
        writer.BoolProperty(L"previouslyEnabled", result.DebugPrivilege.PreviouslyEnabled);
        writer.BoolProperty(L"enabled", result.DebugPrivilege.Enabled);
        writer.BoolProperty(L"requiresElevation", result.DebugPrivilege.RequiresElevation);
        writer.UIntProperty(L"errorCode", result.DebugPrivilege.ErrorCode);
        writer.StringProperty(L"errorMessage", result.DebugPrivilege.ErrorMessage);
        writer.EndObject();
        writer.BeginArrayProperty(L"processes");
        for (const LockingProcess& process : result.Processes)
        {
            writer.BeginArrayItemObject();
            writer.UIntProperty(L"processId", process.ProcessId);
            writer.StringProperty(L"applicationName", process.ApplicationName);
            writer.StringProperty(L"imagePath", process.ImagePath);
            writer.UIntProperty(L"openHandleCount", process.OpenHandleCount);
            writer.UIntProperty(L"sectionHandleCount", process.SectionHandleCount);
            writer.UIntProperty(L"mappedViewCount", process.MappedViewCount);
            writer.UIntProperty(L"closedOpenHandleCount", process.ClosedOpenHandleCount);
            writer.UIntProperty(L"closedSectionHandleCount", process.ClosedSectionHandleCount);
            writer.UIntProperty(L"unmappedViewCount", process.UnmappedViewCount);
            writer.EndArrayItemObject();
        }
        writer.EndArray();
        writer.EndObject();
    }
}
