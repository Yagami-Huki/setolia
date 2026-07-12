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

foreach ($obsDir in $PortableDirs) {
    Write-Host "Deploying to: $obsDir" -ForegroundColor Green

    # 1. DLLファイルのコピー (bin\64bit -> obs-plugins\64bit)
    $dllSource = Join-Path $SourcePluginPath "bin\64bit"
    $dllDest = Join-Path $obsDir "obs-plugins\64bit"

    if (Test-Path $dllSource) {
        if (-not (Test-Path $dllDest)) {
            New-Item -ItemType Directory -Force -Path $dllDest | Out-Null
        }
        Copy-Item -Path "$dllSource\*" -Destination $dllDest -Recurse -Force
        Write-Host "  Copied binaries to $dllDest"
    }

    # 2. リソースファイルのコピー (data -> data\obs-plugins\setolia)
    $dataSource = Join-Path $SourcePluginPath "data"
    $dataDest = Join-Path $obsDir "data\obs-plugins\setolia"

    if (Test-Path $dataSource) {
        if (-not (Test-Path $dataDest)) {
            New-Item -ItemType Directory -Force -Path $dataDest | Out-Null
        }
        Copy-Item -Path "$dataSource\*" -Destination $dataDest -Recurse -Force
        Write-Host "  Copied data to $dataDest"
    }
}

Write-Host "Deployment completed!" -ForegroundColor Cyan
