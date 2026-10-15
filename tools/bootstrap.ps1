$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolsRoot = Join-Path $projectRoot '.tools'
$vendorRoot = Join-Path $projectRoot 'third_party'
New-Item -ItemType Directory -Force -Path $toolsRoot, $vendorRoot | Out-Null
$headers = @{ 'User-Agent' = 'SideMapping-build' }
$release = Invoke-RestMethod -Uri 'https://api.github.com/repos/mstorsjo/llvm-mingw/releases/tags/20261006' -Headers $headers
$asset = $release.assets | Where-Object { $_.name -eq 'llvm-mingw-20261006-ucrt-x86_64.zip' } | Select-Object -First 1
if (-not $asset) { throw 'The official compiler archive was not found.' }
$archive = Join-Path $toolsRoot $asset.name
$compilerDirectory = Join-Path $toolsRoot 'llvm-mingw-20261006-ucrt-x86_64'
if (-not (Test-Path -LiteralPath (Join-Path $compilerDirectory 'bin\clang++.exe'))) {
    Write-Output ('Downloading official compiler: ' + $asset.name)
    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $archive -Headers $headers -UseBasicParsing
    if ($asset.digest -and $asset.digest.StartsWith('sha256:')) {
        $actualHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualHash -ne $asset.digest.Substring(7)) { throw 'Compiler checksum mismatch.' }
        Write-Output ('Compiler SHA256 verified: ' + $actualHash)
    }
    Write-Output 'Extracting compiler into the project .tools directory.'
    Expand-Archive -LiteralPath $archive -DestinationPath $toolsRoot -Force
}
$jsonHeader = Join-Path $vendorRoot 'json.hpp'
if (-not (Test-Path -LiteralPath $jsonHeader)) {
    Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/nlohmann/json/v3.12.0/single_include/nlohmann/json.hpp' -OutFile $jsonHeader -Headers $headers -UseBasicParsing
    Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/nlohmann/json/v3.12.0/LICENSE.MIT' -OutFile (Join-Path $vendorRoot 'json-LICENSE.MIT') -Headers $headers -UseBasicParsing
}
Write-Output ('Compiler ready: ' + (Join-Path $compilerDirectory 'bin\clang++.exe'))
