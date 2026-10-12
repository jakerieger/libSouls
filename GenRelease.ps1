$Version = Read-Host "Enter the new version number"

$CMakeFile = "CMakeLists.txt"
$DoxyFile = "Doxyfile"

(Get-Content $CMakeFile -Raw) -replace '(?m)^(project\(libSouls VERSION )[\d.]+', "`${1}$Version" | Set-Content $CMakeFile -NoNewline
(Get-Content $DoxyFile -Raw) -replace '(?m)^(PROJECT_NUMBER\s*=\s*)[\d.]+', "`${1}$Version" | Set-Content $DoxyFile -NoNewline

Write-Host "Generating release for v$Version" -ForegroundColor Green

$VsPath = & "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -property installationPath
Import-Module "$VsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $VsPath -Arch amd64 -HostArch amd64 -SkipAutomaticLocation > $null

Write-Host "Target architecture: $env:VSCMD_ARG_TGT_ARCH"
Write-Host "Host architecture:   $env:VSCMD_ARG_HOST_ARCH"

if ($env:VSCMD_ARG_TGT_ARCH -ne "x64" -or $env:VSCMD_ARG_HOST_ARCH -ne "x64")
{
    Write-Host "32-bit builds not supported. Exiting." -ForegroundColor Red
}

function Copy-Headers
{
    [CmdletBinding()]
    param (
        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    process {
        $Source = "$PSScriptRoot\Source\libSouls"
        Robocopy $Source $Destination *.hpp *.h /S > $null
    }
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

        cmake --build $BuildDir --config $ConfigStr 2> $null > $null

        $DistDir = "$BuildRoot\dist\$Version"
        $DistOutput = "$DistDir\$Version-$ConfigStr"
        if (Test-Path -Path $DistOutput)
        {
            Remove-Item -Recurse $DistOutput -Force > $null
        }
        New-Item -Path $DistOutput\bin -ItemType Directory -Force > $null
        New-Item -Path $DistOutput\lib -ItemType Directory -Force > $null
        New-Item -Path $DistOutput\include\libSouls -ItemType Directory -Force > $null

        $DllPath = "$BuildDir\bin\Souls.dll"
        $LibPath = "$BuildDir\lib\Souls.lib"
        $HeaderPath = "$PSScriptRoot\Source\libSouls\CApi\souls.h"

        if ($Config -eq [Config]::Debug)
        {
            #Copy debug symbols
            Copy-Item "$BuildDir\bin\Souls.pdb" $DistOutput\bin\Souls.pdb -Force > $null
        }

        Copy-Item $DllPath $DistOutput\bin\Souls.dll -Force > $null
        Copy-Item $LibPath $DistOutput\lib\Souls.lib -Force > $null
        Copy-Headers("$DistOutput\include\libSouls")

        $OutputFile = "$DistDir\libSouls-$Version-Windows-MSVC-x64-$ConfigStr.zip"
        Compress-Archive -Path "$DistOutput\*" -DestinationPath $OutputFile -Force > $null
        Remove-Item -Path $DistOutput -Recurse > $null

        gpg --yes --local-user "0B0E935CED5B405BC6E220F6AB77AA328072BE4A" --detach-sign --armor --output "$OutputFile.sig" $OutputFile > $null
        Copy-Item "$env:HOMEPATH\.gpg\libSouls_public.asc" "$DistDir\signature.asc" > $null

        return $OutputFile
    }
}

foreach ($BuildConfig in [System.Enum]::GetValues([Config]))
{
    $OutFile = Create-Release($BuildConfig)
    Write-Host "Created $OutFile" -ForegroundColor Cyan
}