param([switch]$Test)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$compilerDirectory = Join-Path $projectRoot '.tools\llvm-mingw-20261006-ucrt-x86_64\bin'
$compiler = Join-Path $compilerDirectory 'clang++.exe'
if (-not (Test-Path -LiteralPath $compiler)) { & (Join-Path $projectRoot 'tools\bootstrap.ps1') }
$buildDirectory = Join-Path $projectRoot 'build'
$distribution = Join-Path $projectRoot 'dist'
New-Item -ItemType Directory -Force -Path $buildDirectory,$distribution | Out-Null
Push-Location $projectRoot
try {
    & (Join-Path $compilerDirectory 'llvm-windres.exe') '-i' 'app.rc' '-o' 'build\app-res.o' '-O' 'coff'
    if ($LASTEXITCODE -ne 0) { throw 'Resource build failed.' }
    $common = @('-std=c++20','-O2','-Wall','-Wextra','-Wno-missing-field-initializers','-DUNICODE','-D_UNICODE','-D_WIN32_WINNT=0x0A00','-DNOMINMAX','-Isrc','-Ithird_party','-static','-finput-charset=UTF-8')
    $libraries = @('-ld2d1','-ldwrite','-ldwmapi','-lwindowscodecs','-lole32','-luuid','-lshell32','-luser32','-lgdi32','-ladvapi32','-lhid')
    & $compiler @common '-municode' '-mwindows' 'src\main.cpp' 'src\app.cpp' 'src\paint.cpp' 'src\model.cpp' 'src\input.cpp' 'build\app-res.o' @libraries '-o' 'dist\侧映.exe' '-Wl,--strip-all'
    if ($LASTEXITCODE -ne 0) { throw 'Application build failed.' }
    Write-Output ('Built: ' + (Join-Path $distribution '侧映.exe'))
    Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\使用说明.txt') -Destination (Join-Path $distribution '使用说明.txt') -Force
    Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE') -Destination (Join-Path $distribution 'LICENSE.txt') -Force
    $licenses = Join-Path $distribution 'licenses'
    New-Item -ItemType Directory -Force -Path $licenses | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectRoot 'third_party\json-LICENSE.MIT') -Destination (Join-Path $licenses 'nlohmann-json-MIT.txt') -Force
    Copy-Item -LiteralPath (Join-Path $projectRoot '.tools\llvm-mingw-20261006-ucrt-x86_64\LICENSE.TXT') -Destination (Join-Path $licenses 'llvm-mingw-NOTICES.txt') -Force
    if ($Test) {
        & $compiler @common '-municode' 'tests\core_tests.cpp' 'src\model.cpp' 'src\input.cpp' @libraries '-o' 'build\core-tests.exe'
        if ($LASTEXITCODE -ne 0) { throw 'Test build failed.' }
        & (Join-Path $buildDirectory 'core-tests.exe') (Join-Path $buildDirectory 'test-data')
        if ($LASTEXITCODE -ne 0) { throw 'Core tests failed.' }
    }
} finally { Pop-Location }
