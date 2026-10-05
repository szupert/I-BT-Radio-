Add-Type -AssemblyName System.Drawing

$directory = Split-Path -Parent $MyInvocation.MyCommand.Path

function Remove-TitleMark([string]$path, [int]$y0, [int]$y1) {
  $bitmap = [Drawing.Bitmap]::FromFile($path)
  $temporary = "$path.branding-clean.tmp.png"
  try {
    if ($bitmap.Width -ne 320 -or $bitmap.Height -ne 240) {
      throw "Expected a 320x240 skin image: $path"
    }
    # Erase only the small gold lightning-style mark at the left of the title bar.
    # Copy the adjacent clean title-bar background row-by-row; all other pixels stay intact.
    for ($y = $y0; $y -le $y1; $y++) {
      $background = $bitmap.GetPixel(16, $y)
      for ($x = 4; $x -le 15; $x++) { $bitmap.SetPixel($x, $y, $background) }
    }
    $bitmap.Save($temporary, [Drawing.Imaging.ImageFormat]::Png)
  }
  finally { $bitmap.Dispose() }
  Move-Item -LiteralPath $temporary -Destination $path -Force
}

function Convert-ToRgb565Le([string]$pngPath, [string]$rawPath) {
  $bitmap = [Drawing.Bitmap]::FromFile($pngPath)
  try {
    $bytes = [byte[]]::new($bitmap.Width * $bitmap.Height * 2)
    $i = 0
    for ($y = 0; $y -lt $bitmap.Height; $y++) {
      for ($x = 0; $x -lt $bitmap.Width; $x++) {
        $p = $bitmap.GetPixel($x, $y)
        $r5 = [int][Math]::Floor(($p.R * 31 + 127) / 255.0)
        $g6 = [int][Math]::Floor(($p.G * 63 + 127) / 255.0)
        $b5 = [int][Math]::Floor(($p.B * 31 + 127) / 255.0)
        $v = ($r5 -shl 11) -bor ($g6 -shl 5) -bor $b5
        $bytes[$i++] = [byte]($v -band 0xff)
        $bytes[$i++] = [byte](($v -shr 8) -band 0xff)
      }
    }
    [IO.File]::WriteAllBytes($rawPath, $bytes)
  }
  finally { $bitmap.Dispose() }
}

$player = Join-Path $directory 'player_eq.png'
Remove-TitleMark $player 2 14
Remove-TitleMark $player 119 131
Convert-ToRgb565Le $player (Join-Path $directory 'player_eq.rgb565le')

$simulated = Join-Path $directory 'preview_simulated.png'
Remove-TitleMark $simulated 2 14
Remove-TitleMark $simulated 119 131

$playlist = Join-Path $directory 'playlist.png'
# The playlist has no lightning mark in the supplied asset; regenerate its raw file to verify format.
Convert-ToRgb565Le $playlist (Join-Path $directory 'playlist.rgb565le')

Write-Output 'Removed title-bar marks and regenerated both RGB565LE assets.'
