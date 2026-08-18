$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

Push-Location $repoRoot
try {
  git config core.hooksPath .githooks
  Write-Host "Configured core.hooksPath to .githooks for $repoRoot"
} finally {
  Pop-Location
}
