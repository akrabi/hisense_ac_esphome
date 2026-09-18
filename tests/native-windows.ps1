param(
    [string]$Sdk = 'C:\Program Files\Microsoft Visual Studio\18\Enterprise\SDK\ScopeCppSDK\vc15'
)
$ErrorActionPreference = 'Stop'
$env:PATH = "$Sdk\VC\bin;$Sdk\SDK\bin;$PWD\.venv\Scripts;$env:PATH"
$env:INCLUDE = "$Sdk\VC\include;$Sdk\SDK\include\ucrt;$Sdk\SDK\include\shared;$Sdk\SDK\include\um"
$env:LIB = "$Sdk\VC\lib;$Sdk\SDK\lib"
New-Item -ItemType Directory -Force .build\scratch | Out-Null
$env:TMP = "$PWD\.build\scratch"
$env:TEMP = $env:TMP
& .venv\Scripts\cmake.exe -S tests -B .build\native -G 'NMake Makefiles' `
    "-DPython3_EXECUTABLE=$PWD\.venv\Scripts\python.exe" -DCMAKE_BUILD_TYPE=Release `
    '-DCMAKE_EXE_LINKER_FLAGS=/MANIFEST:NO' '-DCMAKE_CXX_FLAGS=/D_CRT_SECURE_NO_WARNINGS'
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& .venv\Scripts\cmake.exe --build .build\native
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& .venv\Scripts\ctest.exe --test-dir .build\native --output-on-failure
exit $LASTEXITCODE
