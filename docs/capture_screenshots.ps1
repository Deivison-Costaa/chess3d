# docs/capture_screenshots.ps1
# Captura screenshots do Chess3D para a documentação.
#
# - Lança o .exe auto-contido (build de packaging) e captura o MENU principal.
# - Clica em "Iniciar" (Humano vs IA, default) e captura o TABULEIRO em jogo.
# - Captura via CopyFromScreen (região da janela na tela) — método correto para
#   janelas OpenGL; PrintWindow costuma sair preto com aceleração de hardware.
#
# Uso:  pwsh -File docs/capture_screenshots.ps1
# Obs.: as coordenadas do clique são relativas ao canto da janela; se o seu
#       layout/resolução for muito diferente, ajuste $startBtnDx/$startBtnDy.

param(
    [string]$Exe = "$PSScriptRoot\..\build-package\bin\chess3d.exe",
    [string]$OutDir = "$PSScriptRoot\img",
    [int]$startBtnDx = 449,   # offset X do botão "Iniciar" a partir do canto da janela
    [int]$startBtnDy = 449    # offset Y do botão "Iniciar"
)

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Cap {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint x, uint y, uint d, IntPtr e);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
Add-Type -AssemblyName System.Drawing

New-Item -ItemType Directory -Force $OutDir | Out-Null

function Get-Chess3dWindow {
    $p = Get-Process chess3d -ErrorAction Stop | Select-Object -First 1
    for ($i = 0; $i -lt 20 -and $p.MainWindowHandle -eq 0; $i++) {
        Start-Sleep -Milliseconds 500; $p.Refresh()
    }
    return $p
}

function Save-Window([IntPtr]$h, [string]$path) {
    [Cap]::ShowWindow($h, 9) | Out-Null         # SW_RESTORE
    [Cap]::SetForegroundWindow($h) | Out-Null
    Start-Sleep -Milliseconds 700
    $r = New-Object Cap+RECT
    [Cap]::GetWindowRect($h, [ref]$r) | Out-Null
    $w = $r.Right - $r.Left; $hh = $r.Bottom - $r.Top
    $bmp = New-Object Drawing.Bitmap $w, $hh
    $g = [Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $bmp.Save($path)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "salvo $path (${w}x${hh})"
    return $r
}

function Click-Screen([int]$x, [int]$y) {
    [Cap]::SetCursorPos($x, $y) | Out-Null
    Start-Sleep -Milliseconds 200
    [Cap]::mouse_event(0x02, 0, 0, 0, [IntPtr]::Zero)  # LEFTDOWN
    Start-Sleep -Milliseconds 60
    [Cap]::mouse_event(0x04, 0, 0, 0, [IntPtr]::Zero)  # LEFTUP
}

Get-Process chess3d -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500

$proc = Start-Process $Exe -PassThru
$proc = Get-Chess3dWindow
Start-Sleep 4                       # deixa renderizar o menu
$h = $proc.MainWindowHandle

# 1) Menu principal
$rect = Save-Window $h "$OutDir\menu.png"

# 2) Tabuleiro em jogo (clica "Iniciar" — Humano vs IA por default)
[Cap]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 400
Click-Screen ($rect.Left + $startBtnDx) ($rect.Top + $startBtnDy)
Start-Sleep 3
[Cap]::SetCursorPos(($rect.Right - 40), ($rect.Top + 120)) | Out-Null  # tira o cursor do tabuleiro
Start-Sleep 1
Save-Window $h "$OutDir\jogo.png" | Out-Null

Get-Process chess3d -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Host "pronto."
