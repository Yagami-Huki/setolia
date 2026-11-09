param (
    [string]$SourcePluginPath = "C:\ProgramData\obs-studio\plugins\setolia"
)

Write-Host "Starting deployment to portable OBS instances..." -ForegroundColor Cyan

# プロジェクト内の 'portable' フォルダ以下にあるディレクトリを自動検索してデプロイ対象にする
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$PortablesDir = Join-Path $ProjectRoot "portable"

if (-not (Test-Path $PortablesDir)) {
    Write-Warning "The 'portable' directory does not exist at $PortablesDir"
    Write-Warning "Please create it and extract OBS portable versions (e.g. OBS-Studio-32.1.2) into it."
    exit 0
}

$PortableDirs = Get-ChildItem -Path $PortablesDir -Directory | Select-Object -ExpandProperty FullName

if (-not $PortableDirs) {
    Write-Warning "No portable OBS directories found in $PortablesDir"
    Write-Warning "Please download the zip release of OBS and extract it to a folder like 'OBS-Studio-32.1.2' inside the 'portable' directory."
    exit 0
}

if (-not (Test-Path $SourcePluginPath)) {
    Write-Error "Source plugin path not found: $SourcePluginPath. Please build and install the plugin first."
    exit 1
}

# コピー元の dll: 新しい構造 (フォルダ直下) を優先し、なければ bin\64bit から取る
$dllSourceFile = Join-Path $SourcePluginPath "setolia.dll"
if (-not (Test-Path $dllSourceFile)) {
    $dllSourceFile = Join-Path $SourcePluginPath "bin\64bit\setolia.dll"
}
if (-not (Test-Path $dllSourceFile)) {
    Write-Error "setolia.dll not found in $SourcePluginPath (checked root and bin\64bit)."
    exit 1
}
Write-Host "Using DLL: $dllSourceFile"

# 同じ場所に PDB があれば一緒にコピーする
$pdbSourceFile = [System.IO.Path]::ChangeExtension($dllSourceFile, ".pdb")
$dataSource = Join-Path $SourcePluginPath "data"

function Copy-PluginFiles {
    param(
        [string]$DllDest,
        [string]$DataDest
    )

    New-Item -ItemType Directory -Force -Path $DllDest | Out-Null
    Copy-Item -Path $dllSourceFile -Destination $DllDest -Force
    if (Test-Path $pdbSourceFile) {
        Copy-Item -Path $pdbSourceFile -Destination $DllDest -Force
    }
    Write-Host "  Copied binaries to $DllDest"

    if (Test-Path $dataSource) {
        New-Item -ItemType Directory -Force -Path $DataDest | Out-Null
        Copy-Item -Path "$dataSource\*" -Destination $DataDest -Recurse -Force
        Write-Host "  Copied data to $DataDest"
    }
}

function Get-ObsMajorVersion {
    param([string]$ObsDir)

    $obsExe = Join-Path $ObsDir "bin\64bit\obs64.exe"
    if (-not (Test-Path $obsExe)) {
        return $null
    }
    $fileVersion = (Get-Item $obsExe).VersionInfo.FileVersion
    if ($fileVersion -match '^\s*(\d+)') {
        return [int]$Matches[1]
    }
    return $null
}

foreach ($obsDir in $PortableDirs) {
    Write-Host "Deploying to: $obsDir" -ForegroundColor Green

    $major = Get-ObsMajorVersion -ObsDir $obsDir
    $deployNew = $false
    $deployLegacy = $false

    if ($null -eq $major) {
        Write-Warning "  Could not determine OBS version from $obsDir\bin\64bit\obs64.exe. Deploying to both layouts."
        $deployNew = $true
        $deployLegacy = $true
    } elseif ($major -ge 33) {
        Write-Host "  Detected OBS $major (new plugin layout)"
        $deployNew = $true
    } else {
        Write-Host "  Detected OBS $major (legacy plugin layout)"
        $deployLegacy = $true
    }

    # OBS 33 以降: <OBS>\plugins\setolia\setolia.dll と <OBS>\plugins\setolia\data\
    if ($deployNew) {
        $pluginDir = Join-Path $obsDir "plugins\setolia"
        Copy-PluginFiles -DllDest $pluginDir -DataDest (Join-Path $pluginDir "data")
    }

    # OBS 32 以前: <OBS>\obs-plugins\64bit\ と <OBS>\data\obs-plugins\setolia\
    if ($deployLegacy) {
        Copy-PluginFiles -DllDest (Join-Path $obsDir "obs-plugins\64bit") -DataDest (Join-Path $obsDir "data\obs-plugins\setolia")
    }
}

Write-Host "Deployment completed!" -ForegroundColor Cyan
