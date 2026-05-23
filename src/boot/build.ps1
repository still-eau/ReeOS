# PowerShell build script for the ReeOS Bootloader.
# Compiles Stage 2 (boot2.asm), Stage 1 (boot.asm), builds the image ReeOS.img,
# and cleans up temporary .bin files.

# bail out immediately on any errors
$ErrorActionPreference = "Stop"

# locate the current directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrEmpty($ScriptDir)) {
    $ScriptDir = Get-Location
}

Write-Host "`n======================================================================" -ForegroundColor Cyan
Write-Host "                  ReeOS Bootloader Builder v1.0" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan

# check if nasm is in PATH
Write-Host "[*] Checking for NASM assembler..." -ForegroundColor Yellow
if (-not (Get-Command "nasm" -ErrorAction SilentlyContinue)) {
    Write-Error "NASM is not installed or not in your system PATH. Please install NASM to compile the bootloader."
}
$nasmVersion = nasm -v
Write-Host "    [OK] Found NASM: $nasmVersion" -ForegroundColor Green

# setup absolute paths
$boot2Src = Join-Path $ScriptDir "boot2.asm"
$boot2Bin = Join-Path $ScriptDir "boot2.bin"
$bootSrc  = Join-Path $ScriptDir "boot.asm"
$finalImg = Join-Path $ScriptDir "..\..\ReeOS.img" # save output image at project root

# Step 1: Compile Stage 2 to raw binary
Write-Host "`n[*] Step 1: Compiling Stage 2 (boot2.asm)..." -ForegroundColor Yellow
try {
    nasm -I "$ScriptDir/" -f bin "$boot2Src" -o "$boot2Bin"
    Write-Host "    [OK] Stage 2 compiled successfully -> $boot2Bin" -ForegroundColor Green
} catch {
    Write-Host "    [ERROR] Failed to compile Stage 2!" -ForegroundColor Red
    throw $_
}

# Step 2: Compile Stage 1 (which will include Stage 2 and pad the image)
Write-Host "`n[*] Step 2: Compiling Stage 1 & packaging image (boot.asm -> ReeOS.img)..." -ForegroundColor Yellow
try {
    nasm -I "$ScriptDir/" -f bin "$bootSrc" -o "$finalImg"
    Write-Host "    [OK] Bootable disk image created successfully -> $finalImg" -ForegroundColor Green
} catch {
    Write-Host "    [ERROR] Failed to package bootable disk image!" -ForegroundColor Red
    if (Test-Path "$boot2Bin") { Remove-Item "$boot2Bin" -Force }
    throw $_
}

# Step 3: Cleanup temporary binary files
Write-Host "`n[*] Step 3: Cleaning up intermediate build artifacts..." -ForegroundColor Yellow
if (Test-Path "$boot2Bin") {
    Remove-Item "$boot2Bin" -Force
    Write-Host "    [OK] Removed temporary file $boot2Bin" -ForegroundColor Green
}

# print compile summary report
$imgFile = Get-Item "$finalImg"
$imgSize = $imgFile.Length
Write-Host "`n======================================================================" -ForegroundColor Green
Write-Host "  BUILD SUCCESSFUL!" -ForegroundColor Green
Write-Host "  Output Image : $finalImg" -ForegroundColor Green
Write-Host "  Image Size   : $imgSize bytes ($($imgSize / 512) sectors)" -ForegroundColor Green
Write-Host "======================================================================" -ForegroundColor Green

Write-Host "`nTo run and test this bootloader in QEMU, run:" -ForegroundColor Cyan
Write-Host "qemu-system-x86_64 -drive format=raw,file=ReeOS.img" -ForegroundColor Yellow
Write-Host ""
