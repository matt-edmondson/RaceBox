[CmdletBinding(PositionalBinding = $false)]
param(
  [string]$Target = "esp32s3",
  [string]$Port,
  [int]$Baud = 921600,
  [switch]$Clean,
  [switch]$FullClean,
  [switch]$Flash,
  [switch]$Monitor,
  [switch]$Menuconfig,
  [switch]$Erase,
  [string]$IdfRef = "v5.2.2",
  [string]$InstallPath,
  [Parameter(ValueFromRemainingArguments = $true)]
  [string[]]$IdfArgs
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Section([string]$message) {
  Write-Host "`n=== $message ===" -ForegroundColor Cyan
}

function Ensure-InRepoRoot {
  $scriptDirectory = Split-Path -Parent $PSCommandPath
  Set-Location $scriptDirectory
}

function Get-DefaultInstallPath {
  if ([string]::IsNullOrWhiteSpace($InstallPath)) {
    if ($env:IDF_INSTALL_PATH) { return $env:IDF_INSTALL_PATH }
    if ($IsWindows) { return Join-Path $env:USERPROFILE ".espressif/esp-idf-$IdfRef" }
    else { return Join-Path $env:HOME ".espressif/esp-idf-$IdfRef" }
  }
  return $InstallPath
}

function Install-ESP-IDF([string]$Ref, [string]$Dest, [string]$InstallTargets) {
  Write-Section "Installing ESP-IDF $Ref to $Dest"
  if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw 'git is required to install ESP-IDF.' }
  if (-not (Test-Path $Dest)) { New-Item -ItemType Directory -Path $Dest | Out-Null }
  $idfPath = Join-Path $Dest "esp-idf"
  if (-not (Test-Path $idfPath)) {
    git clone --depth 1 --branch $Ref https://github.com/espressif/esp-idf.git $idfPath | Write-Output
  } else {
    Push-Location $idfPath; try { git fetch --depth 1 origin $Ref | Write-Output; git checkout -q $Ref; git reset --hard -q FETCH_HEAD } finally { Pop-Location }
  }
  $idfTools = Join-Path $idfPath "tools/idf_tools.py"
  if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    if (Get-Command py -ErrorAction SilentlyContinue) { $python = 'py -3' } else { throw 'Python 3.x is required to bootstrap ESP-IDF.' }
  } else { $python = 'python' }
  Write-Section "Downloading tools (targets: $InstallTargets)"
  & $python $idfTools install --targets $InstallTargets --non-interactive | Write-Output
  & $python $idfTools install-python-env --non-interactive | Write-Output
  if ($IsWindows) { $export = Join-Path $idfPath 'export.ps1'; if (-not (Test-Path $export)) { throw "Missing export.ps1 at $export" }; Write-Section "Sourcing $export"; . $export | Out-Null }
  else { $export = Join-Path $idfPath 'export.sh'; if (-not (Test-Path $export)) { throw "Missing export.sh at $export" }; Write-Section "Sourcing $export via bash"; bash -lc ". '$export' && env" | ForEach-Object { $kv = $_ -split '=',2; if ($kv.Length -eq 2) { Set-Item -Path "env:$($kv[0])" -Value $kv[1] } } }
  return $idfPath
}

function Ensure-IdfEnvironment {
  if (Get-Command idf.py -ErrorAction SilentlyContinue) { return }
  if ($env:IDF_PATH -and (Test-Path (Join-Path $env:IDF_PATH 'tools/idf.py'))) {
    $exportScript = if ($IsWindows) { Join-Path $env:IDF_PATH 'export.ps1' } else { Join-Path $env:IDF_PATH 'export.sh' }
    Write-Section "Sourcing ESP-IDF environment from $exportScript"
    if ($IsWindows) { . $exportScript | Out-Null } else { bash -lc ". '$exportScript' && env" | ForEach-Object { $kv = $_ -split '=',2; if ($kv.Length -eq 2) { Set-Item -Path "env:$($kv[0])" -Value $kv[1] } } }
    return
  }
  $dest = Get-DefaultInstallPath
  $idfPath = Install-ESP-IDF -Ref $IdfRef -Dest $dest -InstallTargets $Target
  $env:IDF_PATH = $idfPath
}

function Invoke-IdfPy([string[]]$Arguments) {
  Write-Host "idf.py $($Arguments -join ' ')" -ForegroundColor DarkGray
  & idf.py @Arguments
}

try {
  Ensure-InRepoRoot
  Ensure-IdfEnvironment

  $commonArgs = @()
  if ($Port) { $commonArgs += @('-p', $Port) }
  if ($Baud) { $commonArgs += @('-b', $Baud) }
  if ($IdfArgs) { $commonArgs += $IdfArgs }

  if ($FullClean) {
    Write-Section 'Full clean'
    Invoke-IdfPy @('-B','build')
    Invoke-IdfPy ($commonArgs + 'fullclean')
  } elseif ($Clean) {
    Write-Section 'Clean'
    Invoke-IdfPy ($commonArgs + 'clean')
  }

  Write-Section "Set target: $Target"
  Invoke-IdfPy ($commonArgs + @('set-target', $Target))

  if ($Menuconfig) {
    Write-Section 'menuconfig'
    Invoke-IdfPy ($commonArgs + 'menuconfig')
  }

  if ($Erase) {
    Write-Section 'erase-flash'
    Invoke-IdfPy ($commonArgs + 'erase-flash')
  }

  Write-Section 'Build'
  Invoke-IdfPy ($commonArgs + 'build')

  if ($Flash -and $Monitor) {
    Write-Section 'Flash + Monitor'
    Invoke-IdfPy ($commonArgs + @('flash','monitor'))
  } elseif ($Flash) {
    Write-Section 'Flash'
    Invoke-IdfPy ($commonArgs + 'flash')
  } elseif ($Monitor) {
    Write-Section 'Monitor'
    Invoke-IdfPy ($commonArgs + 'monitor')
  }

  Write-Host "`nDone." -ForegroundColor Green
} catch {
  Write-Error $_
  exit 1
}


