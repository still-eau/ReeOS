# PowerShell build script for the ReeOS Bootloader.
# Compiles Stage 1 and Stage 2 separately, then builds a clean, padded ReeOS.img.

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrEmpty($ScriptDir)) {
    $ScriptDir = Get-Location
}

Write-Host "`n======================================================================" -ForegroundColor Cyan
Write-Host "                  ReeOS Bootloader Builder v1.1" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan

# Vérification de NASM
Write-Host "[*] Checking for NASM assembler..." -ForegroundColor Yellow
if (-not (Get-Command "nasm" -ErrorAction SilentlyContinue)) {
    Write-Error "NASM is not installed or not in your system PATH."
}
$nasmVersion = nasm -v
Write-Host "    [OK] Found NASM: $nasmVersion" -ForegroundColor Green

# Chemins absolus
$bootSrc  = Join-Path $ScriptDir "boot.asm"   # Votre Stage 1
$bootBin  = Join-Path $ScriptDir "boot.bin"
$boot2Src = Join-Path $ScriptDir "boot2.asm"  # Votre Stage 2
$boot2Bin = Join-Path $ScriptDir "boot2.bin"
$finalImg = Join-Path $ScriptDir "..\..\ReeOS.img"

# --- ÉTAPE 1 : Compilation du Stage 1 ---
Write-Host "`n[*] Step 1: Compiling Stage 1 (boot.asm)..." -ForegroundColor Yellow
nasm -I "$ScriptDir/" -f bin "$bootSrc" -o "$bootBin"
if ((Get-Item "$bootBin").Length -ne 512) {
    Write-Error "Stage 1 structural error: output is not exactly 512 bytes!"
}
Write-Host "    [OK] Stage 1 compiled successfully (512 bytes)" -ForegroundColor Green

# --- ÉTAPE 2 : Compilation du Stage 2 ---
Write-Host "`n[*] Step 2: Compiling Stage 2 (boot2.asm)..." -ForegroundColor Yellow
nasm -I "$ScriptDir/" -f bin "$boot2Src" -o "$boot2Bin"
Write-Host "    [OK] Stage 2 compiled successfully" -ForegroundColor Green

# --- ÉTAPE 3 : Assemblage de l'image disque (Façon 'dd') ---
Write-Host "`n[*] Step 3: Packaging and padding final disk image (ReeOS.img)..." -ForegroundColor Yellow
try {
    # 1. Lire les octets du Stage 1 (512 octets)
    $stage1Bytes = [System.IO.File]::ReadAllBytes($bootBin)

    # 2. Lire les octets du Stage 2
    $stage2Bytes = [System.IO.File]::ReadAllBytes($boot2Bin)

    # 3. Créer le conteneur pour le Stage 2 rembourré (8 secteurs = 4096 octets)
    $stage2Padded = New-Object byte[] 4096
    [Buffer]::BlockCopy($stage2Bytes, 0, $stage2Padded, 0, $stage2Bytes.Length)

    # 4. Créer une fausse zone pour le noyau (120 secteurs = 61440 octets) pour que le DAP lise quelque chose
    # Note : Si vous avez un vrai kernel.bin, remplacez cette zone par la lecture de votre kernel.bin !
    $kernelPadded = New-Object byte[] 61440
    # Si vous aviez un kernel.bin, vous feriez :
    # $kernelBytes = [System.IO.File]::ReadAllBytes("chemin\vers\kernel.bin")
    # [Buffer]::BlockCopy($kernelBytes, 0, $kernelPadded, 0, $kernelBytes.Length)

    # 5. Fusionner le tout dans l'image finale
    $totalSize = $stage1Bytes.Length + $stage2Padded.Length + $kernelPadded.Length
    $finalBytes = New-Object byte[] $totalSize

    [Buffer]::BlockCopy($stage1Bytes, 0, $finalBytes, 0, $stage1Bytes.Length)
    [Buffer]::BlockCopy($stage2Padded, 0, $finalBytes, $stage1Bytes.Length, $stage2Padded.Length)
    [Buffer]::BlockCopy($kernelPadded, 0, $finalBytes, ($stage1Bytes.Length + $stage2Padded.Length), $kernelPadded.Length)

    # Écriture du fichier final
    [System.IO.File]::WriteAllBytes($finalImg, $finalBytes)
    Write-Host "    [OK] Bootable disk image created successfully -> $finalImg" -ForegroundColor Green
}
catch {
    Write-Host "    [ERROR] Failed to package disk image!" -ForegroundColor Red
    throw $_
}
finally {
    # Nettoyage des fichiers temporaires
    if (Test-Path "$bootBin") { Remove-Item "$bootBin" -Force }
    if (Test-Path "$boot2Bin") { Remove-Item "$boot2Bin" -Force }
}

# Résumé
$imgSize = (Get-Item "$finalImg").Length
Write-Host "`n======================================================================" -ForegroundColor Green
Write-Host "  BUILD SUCCESSFUL!" -ForegroundColor Green
Write-Host "  Output Image : $finalImg" -ForegroundColor Green
Write-Host "  Image Size   : $imgSize bytes ($($imgSize / 512) sectors)" -ForegroundColor Green
Write-Host "======================================================================" -ForegroundColor Green

Write-Host "`nTo run and test this bootloader in QEMU, run:" -ForegroundColor Cyan
Write-Host "qemu-system-x86_64 -drive format=raw,file=ReeOS.img" -ForegroundColor Yellow
Write-Host ""