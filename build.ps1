# =============================================================================
#  ReeOS - Top-level Build Script
# =============================================================================
#
#  Builds the entire OS in the correct order:
#    1. Compile the kernel  (clang/nasm/lld  -> kernel.bin)
#    2. Inject kernel.bin   into ReeOS.img at sector 10
#    3. Assemble bootloader (nasm            -> ReeOS.img, with kernel already inside)
#
#  Usage:
#    .\build.ps1          -- full build + run in QEMU (default)
#    .\build.ps1 -NoRun   -- full build only, do not launch QEMU
#
#  Requirements (must be in PATH):
#    nasm, clang, ld.lld, llvm-objcopy, qemu-system-x86_64 (optional)
#
# =============================================================================

param(
    [switch]$NoRun
)

$ErrorActionPreference = "Stop"

$RootDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$BootDir   = Join-Path $RootDir "src\boot"
$KernelDir = Join-Path $RootDir "src\kernel"
$ImgPath   = Join-Path $RootDir "ReeOS.img"

Write-Host ""
Write-Host "##############################################################" -ForegroundColor Cyan
Write-Host "#           ReeOS Full Build  (kernel + bootloader)         #" -ForegroundColor Cyan
Write-Host "##############################################################" -ForegroundColor Cyan

# ---------------------------------------------------------------------------
# Step 1 — Build bootloader first so the image exists (with zeroed sectors)
# ---------------------------------------------------------------------------
Write-Host "`n>>> [1/3] Building bootloader..." -ForegroundColor Yellow
& "$BootDir\build.ps1"
if ($LASTEXITCODE -ne 0) { Write-Error "Bootloader build failed." }
Write-Host "    [OK] Bootloader built -> $ImgPath" -ForegroundColor Green

# ---------------------------------------------------------------------------
# Step 2 — Compile kernel and inject it into the image at sector 10
# ---------------------------------------------------------------------------
Write-Host "`n>>> [2/3] Building kernel and injecting into image..." -ForegroundColor Yellow
& "$KernelDir\build.ps1" -Inject
if ($LASTEXITCODE -ne 0) { Write-Error "Kernel build/injection failed." }
Write-Host "    [OK] Kernel injected at sector 10" -ForegroundColor Green

# ---------------------------------------------------------------------------
# Step 3 — Print summary
# ---------------------------------------------------------------------------
$imgSize = (Get-Item $ImgPath).Length
Write-Host ""
Write-Host "##############################################################" -ForegroundColor Green
Write-Host "#  BUILD SUCCESSFUL                                          #" -ForegroundColor Green
Write-Host "#  Image : $ImgPath" -ForegroundColor Green
Write-Host "#  Size  : $imgSize bytes ($([math]::Ceiling($imgSize/512)) sectors)" -ForegroundColor Green
Write-Host "##############################################################" -ForegroundColor Green

# ---------------------------------------------------------------------------
# Step 4 — Launch QEMU (unless -NoRun)
# ---------------------------------------------------------------------------
if (-not $NoRun) {
    if (Get-Command "qemu-system-x86_64" -ErrorAction SilentlyContinue) {
        Write-Host "`n>>> [3/3] Launching QEMU..." -ForegroundColor Yellow
        
        Write-Host "    Launching ReeOS.img with Serial redirection..." -ForegroundColor DarkGray
        & qemu-system-x86_64 -drive format=raw,file=ReeOS.img -d int,cpu_reset -no-reboot
    } else {
        Write-Host "`n[INFO] QEMU not found in PATH." -ForegroundColor Yellow
    }
}