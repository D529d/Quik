# build.ps1
# ============================================================================
# 一键构建脚本（Windows / MSVC 2022 / VS 生成器）
# 用法:
#   powershell -ExecutionPolicy Bypass -File build.ps1
#
# 依赖（本机路径可自行调整）:
#   - Visual Studio 2022 (含 C++ 工具集)
#   - Qt 6.8.0: tools\qt\6.8.0\msvc2022_64
#   - CMake: 使用 VS 自带的 CMake，或系统 cmake
# ============================================================================

$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
$BuildDir = Join-Path $Root "build"
$QtDir = Join-Path $Root "tools\qt\6.8.0\msvc2022_64"

# 优先使用 VS 自带的 CMake
$VsCmake = "D:\Visual Studio\Visual Studio 2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$Cmake = if (Test-Path $VsCmake) { $VsCmake } else { "cmake" }

if (-not (Test-Path $QtDir)) {
    Write-Host "[错误] 未找到 Qt: $QtDir" -ForegroundColor Red
    Write-Host "请先执行: aqt install-qt windows desktop 6.8.0 win64_msvc2022_64 -O tools\qt"
    exit 1
}

Write-Host "==> 配置 (CMake: $Cmake)" -ForegroundColor Cyan
& $Cmake -S $Root -B $BuildDir `
    -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_PREFIX_PATH="$QtDir" `
    -DCMAKE_BUILD_TYPE=Release

Write-Host "==> 编译" -ForegroundColor Cyan
& $Cmake --build $BuildDir --config Release --parallel

Write-Host ""
Write-Host "构建完成。运行验证:" -ForegroundColor Green
Write-Host "  build\Release\hello_world.exe -m C:\Users\Deniel_\.lmstudio\models\lmstudio-community\gemma-4-E4B-it-GGUF\gemma-4-E4B-it-Q4_K_M.gguf -p \"你好，请用中文介绍你自己\"" -ForegroundColor Yellow
