param(
    [Parameter(Mandatory)]
    [string]$VcpkgRootDir,

    [Parameter(Mandatory)]
    [string]$BuildDir,

    [Parameter(Mandatory)]
    [string]$InstallDir,

    [Parameter(Mandatory)]
    [ValidatePattern('^\d+\.\d+\.\d+([+-][0-9A-Za-z.-]+)?$')]
    [string]$Semver,

    [switch]$CCache = $false
)

$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$vcpkgRoot = (Resolve-Path -LiteralPath $VcpkgRootDir).Path
$installedTriplets = @(Get-ChildItem -LiteralPath (Join-Path $vcpkgRoot 'installed') -Directory |
    Where-Object { $_.Name -ne 'vcpkg' })
if ($installedTriplets.Count -ne 1) {
    throw 'Expected one dependency triplet from a native vcpkg installation'
}
$triplet = $installedTriplets[0].Name
$dependencies = $installedTriplets[0].FullName
New-Item -ItemType Directory -Path $BuildDir, $InstallDir -Force | Out-Null
$buildPath = (Resolve-Path -LiteralPath $BuildDir).Path
$installPath = (Resolve-Path -LiteralPath $InstallDir).Path
$runtimeDir = Join-Path $dependencies ($IsWindows ? 'bin' : 'lib')

$arguments = @(
    '-S', $sourceRoot, '-B', $buildPath, '-G', 'Ninja'
    '-DCMAKE_BUILD_TYPE=RelWithDebInfo'
    "-DCMAKE_C_COMPILER_LAUNCHER=$($CCache ? 'ccache' : '')"
    "-DCMAKE_CXX_COMPILER_LAUNCHER=$($CCache ? 'ccache' : '')"
    '-DCMAKE_POLICY_DEFAULT_CMP0141=NEW'
    '-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded'
    "-DCMAKE_TOOLCHAIN_FILE=$vcpkgRoot/scripts/buildsystems/vcpkg.cmake"
    '-DVCPKG_MANIFEST_MODE=OFF'
    "-DVCPKG_INSTALLED_DIR=$vcpkgRoot/installed"
    "-DVCPKG_TARGET_TRIPLET=$triplet"
    "-DVCPKG_OVERLAY_TRIPLETS=$sourceRoot/scripts/vcpkg/triplets"
    "-DQMSETUP_APPLOCAL_DEPS_PATHS_RELWITHDEBINFO=$runtimeDir"
    '-DDSSP_INSTALL=ON'
    '-DDSSP_BUILD_TESTS=OFF'
    '-DDSSP_ENABLE_CUDA=OFF'
    "-DDSSP_VERSION=$Semver"
    "-DCMAKE_INSTALL_PREFIX=$installPath"
)
& cmake @arguments | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
if ($CCache) {
    & ccache --zero-stats | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'CCache statistics reset failed' }
}
& cmake --build $buildPath --config RelWithDebInfo --parallel | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'CMake build failed' }
if ($CCache) {
    & ccache --show-stats | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'CCache statistics retrieval failed' }
}
& cmake --install $buildPath --config RelWithDebInfo | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'CMake install failed' }

# The CMake install rules deploy plugin dependencies separately. Resolve the
# executable's dependencies with the same qmsetup deployment tool.
$qmcorecmd = Join-Path $dependencies ($IsWindows ? 'tools/qmsetup/qmcorecmd.exe' : 'tools/qmsetup/qmcorecmd')
$executable = Join-Path $installPath ($IsWindows ? 'bin/dssp.exe' : 'bin/dssp')
$deployDir = Join-Path $installPath ($IsWindows ? 'bin' : 'lib')
& $qmcorecmd deploy --standard --force `
    --linkdir $deployDir --linkdir $runtimeDir --out $deployDir $executable | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'Executable dependency deployment failed' }

Copy-Item -LiteralPath (Join-Path $sourceRoot 'README.md'), (Join-Path $sourceRoot 'COPYING') -Destination $installPath
# Retain the licenses of the dependencies shipped with the portable application.
$licenseDir = Join-Path $installPath 'share/licenses'
New-Item -ItemType Directory -Path $licenseDir -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $dependencies 'share') -Directory | ForEach-Object {
    $copyright = Join-Path $_.FullName 'copyright'
    if (Test-Path -LiteralPath $copyright -PathType Leaf) {
        $destination = Join-Path $licenseDir $_.Name
        New-Item -ItemType Directory -Path $destination -Force | Out-Null
        Copy-Item -LiteralPath $copyright -Destination $destination
    }
}
