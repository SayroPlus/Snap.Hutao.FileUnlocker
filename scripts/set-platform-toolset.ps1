param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("v143", "v145")]
    [string]$Toolset,

    [string]$Root,

    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Root)) {
    $scriptRoot = if (-not [string]::IsNullOrWhiteSpace($PSScriptRoot)) {
        $PSScriptRoot
    }
    else {
        Split-Path -Parent $PSCommandPath
    }

    $Root = (Resolve-Path (Join-Path $scriptRoot "..")).Path
}
else {
    $Root = (Resolve-Path $Root).Path
}

$projectVersionByToolset = @{
    v143 = "16.0"
    v145 = "18.0"
}

$vcxprojFiles = Get-ChildItem -Path $Root -Recurse -Filter *.vcxproj -File |
    Where-Object { $_.FullName -notmatch "\\.git\\" -and $_.FullName -notmatch "\\build\\" }

foreach ($file in $vcxprojFiles) {
    $content = [System.IO.File]::ReadAllText($file.FullName)
    $newContent = [regex]::Replace($content, '<VCProjectVersion>\s*([^<\s]+)\s*</VCProjectVersion>', "<VCProjectVersion>$($projectVersionByToolset[$Toolset])</VCProjectVersion>")
    $newContent = [regex]::Replace($newContent, '<PlatformToolset>\s*([^<\s]+)\s*</PlatformToolset>', "<PlatformToolset>$Toolset</PlatformToolset>")

    if ($newContent -ne $content) {
        if ($DryRun) {
            Write-Host "[dry-run] would update: $($file.FullName)"
        }
        else {
            [System.IO.File]::WriteAllText($file.FullName, $newContent, [System.Text.UTF8Encoding]::new($false))
            Write-Host "[updated] $($file.FullName)"
        }
    }
}
