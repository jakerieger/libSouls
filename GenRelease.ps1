$Version = Read-Host "Enter the new version number"

$CMakeFile = "CMakeLists.txt"
$DoxyFile = "Doxyfile"

(Get-Content $CMakeFile -Raw) -replace '(?m)^(project\(libSouls VERSION )[\d.]+', "`${1}$Version" | Set-Content $CMakeFile -NoNewline
(Get-Content $DoxyFile -Raw) -replace '(?m)^(PROJECT_NUMBER\s*=\s*)[\d.]+', "`${1}$Version" | Set-Content $DoxyFile -NoNewline

Write-Host "Generating release for v$Version" -ForegroundColor Green

$VsPath = & "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -property installationPath
Import-Module "$VsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Get-Command Enter-VsDevShell -Syntax
Enter-VsDevShell -VsInstallPath $VsPath -Arch amd64 -HostArch amd64 -SkipAutomaticLocation

Write-Host "Target architecture: $env:VSCMD_ARG_TGT_ARCH"
Write-Host "Host architecture:   $env:VSCMD_ARG_HOST_ARCH"

if ($env:VSCMD_ARG_TGT_ARCH -ne "x64" -or $env:VSCMD_ARG_HOST_ARCH -ne "x64")
{
    Write-Host "32-bit builds not supported. Exiting." -ForegroundColor Red
}

enum Config
{
    Debug
    Release
}

function Create-Release
{
    [CmdletBinding()]
    param (
        [Parameter(Mandatory = $true)]
        [Config]$Config
    )

    process {
        $BuildRoot = "$PSScriptRoot\build"
        $ConfigStr = $Config.ToString()
        $BuildDir = "$BuildRoot\$ConfigStr"

        $NULL = cmake --build $BuildDir --config $ConfigStr

        $DistDir = "$BuildRoot\dist\$Version"
        $DistOutput = "$DistDir\$Version-$ConfigStr"
        if (Test-Path -Path $DistOutput)
        {
            Remove-Item -Recurse $DistOutput -Force
        }
        $NULL = New-Item -Path $DistOutput\bin -ItemType Directory -Force
        $NULL = New-Item -Path $DistOutput\include\libSouls -ItemType Directory -Force

        $DllPath = "$BuildDir\bin\Souls.dll"
        $HeaderPath = "$PSScriptRoot\Source\libSouls\CApi\souls.h"

        $NULL = Copy-Item $DllPath $DistOutput\bin\Souls.dll -Force
        $NULL = Copy-Item $HeaderPath $DistOutput\include\libSouls\souls.h -Force

        $OutputFile = "$DistDir\libSouls-$Version-Windows-MSVC-x64-$ConfigStr.zip"
        $NULL = Compress-Archive -Path "$DistOutput\*" -DestinationPath $OutputFile -Force
        $NULL = Remove-Item -Path $DistOutput -Recurse

        gpg --yes --local-user "0B0E935CED5B405BC6E220F6AB77AA328072BE4A" --detach-sign --armor --output "$OutputFile.sig" $OutputFile
        Copy-Item "$env:HOMEPATH\.gpg\libSouls_public.asc" "$DistDir\signature.asc"

        return $OutputFile
    }
}

$DebugRel = Create-Release([Config]::Debug)
$ReleaseRel = Create-Release([Config]::Release)

Write-Host "Created $DebugRel" -ForegroundColor Cyan
Write-Host "Created $ReleaseRel" -ForegroundColor Cyan