# Setup script for CubicAIO_Tool
# Creates a uv-managed virtual environment and installs all dependencies
# (runtime + dev tools: ruff, ty, pytest, pyinstaller) from pyproject.toml.

Write-Host "=== CubicAIO_Tool Setup ===" -ForegroundColor Cyan
Write-Host ""

# Check if uv is installed
Write-Host "Checking for uv..." -ForegroundColor Yellow
try {
    $uvVersion = uv --version
    Write-Host "[OK] $uvVersion" -ForegroundColor Green
} catch {
    Write-Host "[ERROR] uv is not installed!" -ForegroundColor Red
    Write-Host "Install via:" -ForegroundColor Yellow
    Write-Host "  winget install astral-sh.uv" -ForegroundColor White
    Write-Host "  -or-" -ForegroundColor Yellow
    Write-Host "  powershell -c `"irm https://astral.sh/uv/install.ps1 | iex`"" -ForegroundColor White
    exit 1
}

Write-Host ""
Write-Host "Step 1: Syncing dependencies from pyproject.toml + uv.lock..." -ForegroundColor Yellow
uv sync --all-groups
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] uv sync failed" -ForegroundColor Red
    exit 1
}
Write-Host "[OK] Dependencies installed" -ForegroundColor Green

Write-Host ""
Write-Host "=== Setup Complete ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Run the application:" -ForegroundColor Yellow
Write-Host "  uv run python CubicAIO_Tool.py" -ForegroundColor White
Write-Host ""
Write-Host "Or use the Makefile shortcuts:" -ForegroundColor Yellow
Write-Host "  make run        # launch GUI" -ForegroundColor White
Write-Host "  make test       # pytest" -ForegroundColor White
Write-Host "  make lint       # ruff check" -ForegroundColor White
Write-Host "  make build      # PyInstaller -> dist/CubicAIO_Tool.exe" -ForegroundColor White
Write-Host ""
