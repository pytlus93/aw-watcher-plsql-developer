param([ValidateSet('x64','x86')][string]$Platform='x64',[switch]$Test,[switch]$Package,[switch]$SkipUI)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$installation){throw 'MSVC C++ tools are required. No installation is performed by this script.'}
$developer=Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
$out=Join-Path $root "build\$Platform"
New-Item -ItemType Directory -Force $out | Out-Null
$commands=@(
    "call `"$developer`" -arch=$Platform -host_arch=x64 >nul",
    "rc /nologo /fo plugin.res `"$root\src\plugin.rc`"",
    "cl /nologo /std:c++17 /EHsc /W4 /WX /MT /O2 /DUNICODE /D_UNICODE /LD `"$root\src\core.cpp`" `"$root\src\settings.cpp`" `"$root\src\plugin.cpp`" plugin.res /link /DEF:`"$root\src\plugin.def`" /OUT:ActivityTracker.dll /IMPLIB:PLSQLActivity.lib user32.lib gdi32.lib shell32.lib ole32.lib advapi32.lib winhttp.lib"
)
if($Test) {
    $commands+="cl /nologo /std:c++17 /EHsc /W4 /WX /MT /O2 /DUNICODE /D_UNICODE /DACTIVITY_TEST /LD `"$root\src\core.cpp`" `"$root\src\settings.cpp`" `"$root\src\plugin.cpp`" plugin.res /link /DEF:`"$root\src\plugin.def`" /OUT:PLSQLActivity-test.dll /IMPLIB:PLSQLActivity-test.lib user32.lib gdi32.lib shell32.lib ole32.lib advapi32.lib winhttp.lib"
    $commands+="cl /nologo /std:c++17 /EHsc /W4 /WX /MT /O2 `"$root\src\core.cpp`" `"$root\tests\core_tests.cpp`" /Fe:core_tests.exe"
    $commands+='core_tests.exe'
    $commands+="cl /nologo /std:c++17 /EHsc /W4 /WX /MT /O2 /DUNICODE /D_UNICODE /DACTIVITY_TEST `"$root\src\core.cpp`" `"$root\src\settings.cpp`" `"$root\tests\settings_tests.cpp`" /Fe:settings_tests.exe user32.lib shell32.lib ole32.lib advapi32.lib"
    $commands+='settings_tests.exe'
    $commands+="cl /nologo /std:c++17 /EHsc /W4 /WX /MT /O2 /DUNICODE /D_UNICODE /DACTIVITY_TEST `"$root\src\core.cpp`" `"$root\tests\waka_tests.cpp`" /Fe:waka_tests.exe winhttp.lib"
    $commands+="cl /nologo /std:c++17 /EHsc /W4 /WX /MT /O2 /DUNICODE /D_UNICODE /DACTIVITY_TEST `"$root\src\core.cpp`" `"$root\src\settings.cpp`" `"$root\tests\aw_client_tests.cpp`" /Fe:aw_client_tests.exe user32.lib shell32.lib ole32.lib advapi32.lib winhttp.lib"
    $commands+="cl /nologo /std:c++17 /EHsc /W4 /WX /MT /O2 /DUNICODE /D_UNICODE `"$root\tests\sdk_harness.cpp`" /Fe:sdk_harness.exe user32.lib gdi32.lib"
    if(!$SkipUI){$commands+='sdk_harness.exe'}
}
$batch=Join-Path $out 'compile.cmd'
($commands | ForEach-Object {$_; 'if errorlevel 1 exit /b 1'}) | Set-Content -LiteralPath $batch -Encoding ascii
Push-Location $out
try { & cmd.exe /d /c $batch; if($LASTEXITCODE){throw "Build/test failed: $LASTEXITCODE"} } finally {Pop-Location}
if($Package) {
    $dist=Join-Path $root 'dist'
    New-Item -ItemType Directory -Force $dist | Out-Null
    $stage=Join-Path $out 'package-1.3.1'
    $stageDocs=Join-Path $stage 'docs'
    New-Item -ItemType Directory -Force $stageDocs | Out-Null
    Copy-Item -LiteralPath (Join-Path $out 'ActivityTracker.dll'),(Join-Path $root 'README.md'),(Join-Path $root 'LICENSE') -Destination $stage -Force
    foreach($name in 'SCHEMA.md','SDK.md','VALIDATION.md','PAYLOAD.md','RELEASES.md') {
        Copy-Item -LiteralPath (Join-Path $root "docs\$name") -Destination $stageDocs -Force
    }
    Compress-Archive -Path "$stage\*" -DestinationPath (Join-Path $dist "ActivityTracker-$Platform.zip") -Force
}
