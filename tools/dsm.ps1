[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("check", "build", "run")]
    [string]$Command,

    [Parameter(Mandatory = $true, Position = 1)]
    [string]$ProjectFile,

    [ValidateSet("xmake", "cmake")]
    [string]$BuildSystem = "xmake",

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$engineDescriptorPath = Join-Path $repoRoot "Engine\Engine.dsmengine.json"
if (-not (Test-Path -LiteralPath $engineDescriptorPath -PathType Leaf)) {
    throw "Engine descriptor 不存在: $engineDescriptorPath"
}
$descriptorPath = (Resolve-Path (Join-Path (Get-Location) $ProjectFile)).Path
$descriptor = Get-Content -Raw -LiteralPath $descriptorPath | ConvertFrom-Json
$projectRoot = Split-Path -Parent $descriptorPath
$projectName = [string]$descriptor.name
$targetName = [string]$descriptor.startupTarget

if ([string]::IsNullOrWhiteSpace($projectName) -or [string]::IsNullOrWhiteSpace($targetName)) {
    throw "项目描述必须包含 name 和 startupTarget: $descriptorPath"
}
if ($descriptor.schemaVersion -ne 2) {
    throw "不支持的 .dsmproj schemaVersion=$($descriptor.schemaVersion)，当前只支持 2"
}
if ([string]$descriptor.engine -ne "DSMEngine") {
    throw "项目使用了不支持的 Engine: $($descriptor.engine)"
}

function Resolve-GamePath([string]$value) {
    if ([string]::IsNullOrWhiteSpace($value)) { return $null }
    if (-not $value.StartsWith('/Game/', [StringComparison]::Ordinal)) {
        throw "项目资产路径必须使用 /Game/ 虚拟根: $value"
    }
    $segments = $value.Substring(6).Split('/')
    if ($segments.Count -eq 0 -or ($segments | Where-Object {
            [string]::IsNullOrEmpty($_) -or $_ -eq '.' -or $_ -eq '..'
        }).Count -gt 0) {
        throw "项目虚拟路径包含非法路径段: $value"
    }
    $relative = [string]::Join([System.IO.Path]::DirectorySeparatorChar, $segments)
    $contentRoot = Join-Path $projectRoot 'Content'
    $resolved = [System.IO.Path]::GetFullPath((Join-Path $contentRoot $relative))
    $relativeToContent = [System.IO.Path]::GetRelativePath($contentRoot, $resolved)
    if ($relativeToContent -eq '..' -or $relativeToContent.StartsWith('..' + [System.IO.Path]::DirectorySeparatorChar)) {
        throw "项目虚拟路径越出 Content 根: $value"
    }
    return $resolved
}

$sceneVirtualPath = [string]$descriptor.startupScene
if (-not $sceneVirtualPath.EndsWith('.dsmscene', [StringComparison]::OrdinalIgnoreCase)) {
    throw "startupScene 必须是 .dsmscene: $sceneVirtualPath"
}
$scenePath = Resolve-GamePath $sceneVirtualPath
if (-not (Test-Path -LiteralPath $scenePath -PathType Leaf)) {
    throw "startupScene 不存在: $scenePath"
}

foreach ($root in @('Content', 'Shaders')) {
    $rootPath = Join-Path $projectRoot $root
    if (-not (Test-Path -LiteralPath $rootPath -PathType Container)) {
        throw "项目目录缺少 $root : $rootPath"
    }
}

if ($Command -eq "check") {
    Write-Output ("项目检查通过: {0}`n项目根: {1}`n目标: {2}`n启动场景: {3}" -f
        $descriptorPath, $projectRoot, $targetName, $descriptor.startupScene)
    exit 0
}

Push-Location $repoRoot
try {
    if ($BuildSystem -eq "xmake") {
        & xmake f -m $Configuration.ToLowerInvariant()
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & xmake build $targetName
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    else {
        $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
        if ($null -ne $cmakeCommand) {
            $cmakePath = $cmakeCommand.Source
        }
        else {
            $cmakePath = "C:\Program Files\Microsoft Visual Studio\18\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (-not (Test-Path -LiteralPath $cmakePath)) {
                throw "未找到 CMake。请将 cmake 加入 PATH，或安装 Visual Studio CMake 工具。"
            }
        }
        $preset = switch ($targetName) {
            "PBR" { "pbr" }
            "RayTracing" { "raytracing" }
            default { throw "没有为目标 $targetName 配置 CMake preset" }
        }
        & $cmakePath --preset $preset
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        $buildDir = Join-Path $repoRoot "build\cmake\$preset"
        & $cmakePath --build $buildDir --config $Configuration --target $targetName --parallel
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    if ($Command -eq "build") { exit 0 }

    $binary = Join-Path $projectRoot ("Binaries\{0}\{1}.exe" -f $Configuration, $targetName)
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
        throw "目标构建后仍不存在: $binary"
    }
    Push-Location $projectRoot
    try {
        & $binary --project $descriptorPath
        $result = $LASTEXITCODE
    }
    finally {
        Pop-Location
    }
    exit $result
}
finally {
    Pop-Location
}
