# =============================================================================
#  ReeOS - Kernel Build Script  (Clang/LLD, no linker script)
# =============================================================================
#
#  Toolchain required (must be in PATH):
#    nasm          - assembler           https://www.nasm.us/
#    clang         - C compiler          https://llvm.org/
#    ld.lld        - LLVM ELF linker     (bundled with LLVM)
#    llvm-objcopy  - binary extraction   (bundled with LLVM)
#
#  On Windows, install LLVM from https://github.com/llvm/llvm-project/releases
#  or via winget : winget install LLVM.LLVM
#  NASM          : winget install NASM.NASM
#
#  Usage:
#    .\build.ps1           - compile kernel.bin only
#    .\build.ps1 -Inject   - compile + inject into ReeOS.img at sector 10
#
# =============================================================================

param(
    [switch]$Inject
)

$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Directories
# ---------------------------------------------------------------------------
$KernelDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir   = Join-Path $KernelDir "..\.."
$BuildDir  = Join-Path $KernelDir "build"
$ArchDir   = Join-Path $KernelDir "arch\x86_64"
$ImgPath   = Join-Path $RootDir "ReeOS.img"

$KernelElf = Join-Path $BuildDir "kernel.elf"
$KernelBin = Join-Path $BuildDir "kernel.bin"

# ---------------------------------------------------------------------------
# Toolchain
# ---------------------------------------------------------------------------
$CC      = "clang"
$LD      = "ld.lld"
$OBJCOPY = "llvm-objcopy"
$NASM    = "nasm"

# ---------------------------------------------------------------------------
# Source files  (order matters: kernel_entry.asm must assemble first)
# ---------------------------------------------------------------------------
$AsmSources = @(
    (Join-Path $KernelDir "kernel_entry.asm"),
    (Join-Path $ArchDir   "isr.asm")
)

$CSources = @(
    (Join-Path $KernelDir "kernel_main.c"),
    (Join-Path $ArchDir   "gdt.c"),
    (Join-Path $ArchDir   "idt.c"),
    (Join-Path $ArchDir   "interrupts.c"),
    (Join-Path $ArchDir   "pic.c"),
    (Join-Path $KernelDir "utils\logger.c"),
    (Join-Path $RootDir   "src\include\stdio.c")
)

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
$CFlags = @(
    "--target=x86_64-unknown-none-elf",  # bare-metal ELF, no host OS
    "-ffreestanding",
    "-fno-stack-protector",
    "-fno-pic",
    "-mno-red-zone",
    "-mno-mmx", "-mno-sse", "-mno-sse2", # no vector registers in kernel
    "-Wall", "-Wextra",
    "-O2",
    "-std=c11",
    "-I$ArchDir",       # for #include "gdt.h", "idt.h", etc.
    "-I$KernelDir",     # for #include "utils/logger.h", etc.
    "-I$(Join-Path $RootDir 'src\include')" # for #include "stdio.h"
)

# Linker flags - no .ld script: addresses are set directly on the command line.
# ld.lld is the LLVM ELF linker; it accepts the same -T flags as GNU ld.
# kernel_entry.o is passed first so _start lands exactly at 0x100000.
#
# Section layout (tight packing, no gaps):
#   0x100000  .text   — executable code
#   0x108000  .rodata — read-only strings/constants (32 KB headroom for code)
#   0x10C000  .data   — initialised data
#   0x110000  .bss    — zero-initialised (not stored on disk, cleared at boot)
#
# With this layout -O binary produces a ~32 KB binary (code + rodata + data)
# which fits comfortably in 32 sectors (16 KB).  If .text ever exceeds 32 KB
# increase the .rodata base address accordingly.
$LDFlags = @(
    "-m", "elf_x86_64",
    "--no-dynamic-linker",
    "-nostdlib",
    "-Ttext=0x100000",                  # .text  starts at physical 0x100000
    "--section-start=.rodata=0x108000", # .rodata 32 KB after .text
    "-Tdata=0x10C000",                  # .data  right after rodata
    "-Tbss=0x110000",                   # .bss   after data (not emitted by objcopy)
    "-e", "_start"                      # entry symbol
)

# ---------------------------------------------------------------------------
# Banner
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "==============================================================" -ForegroundColor Cyan
Write-Host "         ReeOS Kernel Builder v1.1 (Clang/LLD, no .ld)" -ForegroundColor Cyan
Write-Host "==============================================================" -ForegroundColor Cyan

# ---------------------------------------------------------------------------
# Toolchain check
# ---------------------------------------------------------------------------
Write-Host "`n[*] Checking toolchain..." -ForegroundColor Yellow
foreach ($tool in @($NASM, $CC, $LD, $OBJCOPY)) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Write-Error "Tool not found in PATH: $tool`n  -> Install LLVM: winget install LLVM.LLVM"
    }
    $ver = (& $tool --version 2>&1 | Select-Object -First 1)
    Write-Host "    [OK] $tool : $ver" -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# Create build directory
# ---------------------------------------------------------------------------
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
    Write-Host "`n[*] Created build directory: $BuildDir" -ForegroundColor Yellow
}

# ---------------------------------------------------------------------------
# Step 1 - Assemble .asm -> .o  (ELF64 format)
# ---------------------------------------------------------------------------
Write-Host "`n[*] Step 1: Assembling ASM sources..." -ForegroundColor Yellow

$ObjFiles = [System.Collections.ArrayList]@()

foreach ($src in $AsmSources) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($src)
    $obj  = Join-Path $BuildDir ($name + ".o")
    Write-Host "    nasm -f elf64  $([System.IO.Path]::GetFileName($src))" -ForegroundColor DarkGray
    & $NASM -f elf64 $src -o $obj
    if ($LASTEXITCODE -ne 0) { Write-Error "NASM failed: $src" }
    [void]$ObjFiles.Add($obj)
    Write-Host "    [OK] $([System.IO.Path]::GetFileName($obj))" -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# Step 2 - Compile .c -> .o
# ---------------------------------------------------------------------------
Write-Host "`n[*] Step 2: Compiling C sources..." -ForegroundColor Yellow

foreach ($src in $CSources) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($src)
    $obj  = Join-Path $BuildDir ($name + ".o")
    Write-Host "    gcc  $([System.IO.Path]::GetFileName($src))" -ForegroundColor DarkGray
    & $CC @CFlags -c $src -o $obj
    if ($LASTEXITCODE -ne 0) { Write-Error "GCC failed: $src" }
    [void]$ObjFiles.Add($obj)
    Write-Host "    [OK] $([System.IO.Path]::GetFileName($obj))" -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# Step 3 - Link: kernel_entry.o MUST be the first object
# ---------------------------------------------------------------------------
Write-Host "`n[*] Step 3: Linking..." -ForegroundColor Yellow

$entryObj = Join-Path $BuildDir "kernel_entry.o"
$restObjs = $ObjFiles | Where-Object { $_ -ne $entryObj }
$linkObjs = @($entryObj) + $restObjs

Write-Host "    ld  $($linkObjs | ForEach-Object { [System.IO.Path]::GetFileName($_) })" -ForegroundColor DarkGray
& $LD @LDFlags $linkObjs -o $KernelElf
if ($LASTEXITCODE -ne 0) { Write-Error "Linker failed" }
Write-Host "    [OK] kernel.elf" -ForegroundColor Green

# ---------------------------------------------------------------------------
# Step 4 - Extract flat binary
# ---------------------------------------------------------------------------
# With the tight linker layout (.text/.rodata/.data packed together, .bss last)
# a simple -O binary produces a compact binary.  --strip-debug keeps the size
# small by dropping DWARF sections (debug info is in kernel.elf).
Write-Host "`n[*] Step 4: Extracting flat binary..." -ForegroundColor Yellow
& $OBJCOPY -O binary --strip-debug $KernelElf $KernelBin
if ($LASTEXITCODE -ne 0) { Write-Error "objcopy failed" }

# Clean up any leftover partial section bins from previous builds
foreach ($part in @("text.bin", "rodata.bin", "data.bin")) {
    $p = Join-Path $BuildDir $part
    if (Test-Path $p) { Remove-Item $p -Force }
}

$binSize   = (Get-Item $KernelBin).Length
$binSectors = [math]::Ceiling($binSize / 512)
Write-Host "    [OK] kernel.bin  ->  $binSize bytes ($binSectors sectors)" -ForegroundColor Green

# ---------------------------------------------------------------------------
# Step 5 (optional) - Inject kernel into disk image at sector 10
# ---------------------------------------------------------------------------
if ($Inject) {
    Write-Host "`n[*] Step 5: Injecting kernel into disk image at sector 10..." -ForegroundColor Yellow

    if (-not (Test-Path $ImgPath)) {
        Write-Error "Disk image not found: $ImgPath  (run boot build first)"
    }

    $sectorSize    = 512
    # BIOS INT 13h uses 1-indexed sector numbers: CL=10 means LBA 9 = byte offset 9*512.
    # boot2.asm reads with 'mov cl, 10' so the kernel must be at byte offset 9*512 = 4608.
    $kernelOffset  = 9  * $sectorSize    # byte offset 4608  (= BIOS sector 10, 1-indexed)
    $maxBytes      = 120 * $sectorSize   # 61440 bytes reserved in boot.asm (120 sectors)

    if ($binSize -gt $maxBytes) {
        $msg = "Kernel is $binSize bytes but only $maxBytes bytes are reserved in the image."
        Write-Warning $msg
        Write-Warning "Increase the sector count in boot2.asm (load_kernel) and boot.asm (times pad)."
    }

    $img = [System.IO.File]::ReadAllBytes($ImgPath)
    $bin = [System.IO.File]::ReadAllBytes($KernelBin)

    # Grow the buffer if the image is smaller than needed
    $needed = $kernelOffset + $bin.Length
    if ($img.Length -lt $needed) {
        $grown = New-Object byte[] $needed
        [System.Array]::Copy($img, $grown, $img.Length)
        $img = $grown
    }

    [System.Array]::Copy($bin, 0, $img, $kernelOffset, $bin.Length)
    [System.IO.File]::WriteAllBytes($ImgPath, $img)

    Write-Host "    [OK] $($bin.Length) bytes written at byte offset $kernelOffset (sector 10) in ReeOS.img" -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "==============================================================" -ForegroundColor Green
Write-Host "  BUILD SUCCESSFUL" -ForegroundColor Green
Write-Host "  ELF : $KernelElf" -ForegroundColor Green
Write-Host "  BIN : $KernelBin  ($binSize bytes)" -ForegroundColor Green
if ($Inject) {
    Write-Host "  IMG : $ImgPath  (kernel injected at sector 10)" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Run in QEMU:" -ForegroundColor Cyan
    Write-Host "  qemu-system-x86_64 -drive format=raw,file=`"$ImgPath`"" -ForegroundColor Yellow
}
Write-Host "==============================================================" -ForegroundColor Green
Write-Host ""
