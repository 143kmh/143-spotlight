[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( $env:CI -eq $null ) {
    throw "Package-Windows.ps1 requires CI environment"
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Package {
    trap {
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"
    $ReleaseRoot = "${ProjectRoot}/release"
    $PayloadRoot = "${ReleaseRoot}/${Configuration}/${ProductName}"

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ReleaseRoot}/${ProductName}-*-windows-*.zip",
            "${ReleaseRoot}/${ProductName}-*-windows-*-setup.exe"
        )
    }

    Remove-Item @RemoveArgs

    Log-Group "Archiving ${ProductName}..."
    $CompressArgs = @{
        Path = (Get-ChildItem -Path "${ReleaseRoot}/${Configuration}" -Exclude "${OutputName}*.*")
        CompressionLevel = 'Optimal'
        DestinationPath = "${ReleaseRoot}/${OutputName}.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs
    Log-Group

    $IsccCandidates = @(
        "$env:ProgramFiles(x86)\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
    )
    $Iscc = $IsccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

    if ( $null -eq $Iscc ) {
        Write-Warning 'Inno Setup not found; skipping Setup.exe generation.'
        return
    }

    $InstallerScript = "${ProjectRoot}/installer/143-spotlight.iss"
    if ( ! ( Test-Path $InstallerScript ) ) {
        throw "Installer script not found: ${InstallerScript}"
    }

    if ( ! ( Test-Path "${PayloadRoot}/bin/64bit/${ProductName}.dll" ) ) {
        throw "Installer payload is incomplete: ${PayloadRoot}"
    }

    Log-Group "Building ${ProductName} Setup.exe..."
    & $Iscc "/DSourceRoot=${PayloadRoot}" "/DProductName=${ProductName}" "/DProductVersion=${ProductVersion}" "/DOutputDir=${ReleaseRoot}" $InstallerScript
    if ( $LASTEXITCODE -ne 0 ) {
        throw "Inno Setup failed with exit code ${LASTEXITCODE}"
    }
    Log-Group
}

Package
