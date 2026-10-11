if (Test-Path -Path Docs/html)
{
    Remove-Item -Recurse $PSScriptRoot/Docs/html/*
}

doxygen Doxyfile

if ($LASTEXITCODE -eq 0)
{
    Write-Host "Generated Doxygen documentation in 'Docs/html'"
}