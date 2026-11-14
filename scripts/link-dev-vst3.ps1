# 在系统 VST3 目录中创建指向本地编译产物的目录链接（junction），
# 之后每次重新编译，宿主扫描到的都是最新版本，无需手动复制。
#
# 用法（需要管理员权限，只需运行一次）：
#   以管理员身份打开 PowerShell，进入项目根目录，执行：
#   powershell -ExecutionPolicy Bypass -File scripts\link-dev-vst3.ps1
#
# 参数：
#   -Config Release|Debug   链接到哪个配置的产物（默认 Release）
#   -Remove                 删除链接

param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",
    [switch]$Remove
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $projectRoot "build\Curvable_artefacts\$Config\VST3\Curvable.vst3"
$target = Join-Path $env:CommonProgramFiles "VST3\Curvable.vst3"

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Error "需要管理员权限：请以管理员身份运行 PowerShell 后重试。"
}

if (Test-Path -LiteralPath $target) {
    $item = Get-Item -LiteralPath $target -Force
    if ($item.LinkType -eq "Junction") {
        # 只删除链接本身，不影响链接指向的编译产物
        $item.Delete()
        Write-Host "已删除旧链接：$target"
    }
    elseif (-not $Remove) {
        Write-Host "目标位置已存在一个普通文件夹（之前手动复制的插件）：$target"
        $answer = Read-Host "删除它并改为链接？(y/N)"
        if ($answer -ne "y") { Write-Host "已取消。"; exit 1 }
        Remove-Item -LiteralPath $target -Recurse -Force
        Write-Host "已删除：$target"
    }
}

if ($Remove) {
    Write-Host "完成。"
    exit 0
}

if (-not (Test-Path -LiteralPath $source)) {
    Write-Error "找不到编译产物：$source`n请先运行 cmake --build build --config $Config"
}

New-Item -ItemType Junction -Path $target -Target $source | Out-Null
Write-Host "已创建链接："
Write-Host "  $target"
Write-Host "  -> $source"
Write-Host "之后重新编译即可，宿主无需重新复制插件。"
