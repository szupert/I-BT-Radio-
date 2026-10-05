param(
  [Parameter(Mandatory=$true)][string]$InputImage,
  [Parameter(Mandatory=$true)][string]$OutputImage
)

Add-Type -AssemblyName System.Drawing
$source = [System.Drawing.Bitmap]::FromFile((Resolve-Path -LiteralPath $InputImage))
try {
  if ($source.Width -ne 714 -or $source.Height -ne 797) {
    throw "Expected the supplied 714x797 PCM5102A composite photo; got $($source.Width)x$($source.Height)."
  }
  $back = $source.Clone([System.Drawing.Rectangle]::new(0,450,714,347),$source.PixelFormat)
  try {
    # The supplied composite shows the DAC rear rotated 180 degrees from its front.
    $back.RotateFlip([System.Drawing.RotateFlipType]::Rotate180FlipNone)
    $scale = 2
    $photoTop = 100
    $canvas = [System.Drawing.Bitmap]::new(714*$scale,100+347*$scale+196)
    try {
      $g = [System.Drawing.Graphics]::FromImage($canvas)
      try {
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.Clear([System.Drawing.Color]::FromArgb(13,20,35))
        $titleFont = [System.Drawing.Font]::new('Segoe UI',24,[System.Drawing.FontStyle]::Bold)
        $smallFont = [System.Drawing.Font]::new('Segoe UI',12,[System.Drawing.FontStyle]::Regular)
        $legendFont = [System.Drawing.Font]::new('Segoe UI',15,[System.Drawing.FontStyle]::Bold)
        $white = [System.Drawing.Brushes]::White
        $g.DrawString('PCM5102A — hátoldali forrasztási átkötések',$titleFont,$white,18,12)
        $g.DrawString('A hátoldali fotórészletet 180°-kal elforgattam, hogy az iránya a DAC elejéhez igazodjon.',$smallFont,[System.Drawing.Brushes]::LightGray,20,53)
        $g.DrawImage($back,0,$photoTop,714*$scale,347*$scale)

        # Coordinates are on the rotated photo in native pixels. On this board:
        # H1/H2 -> centre + right (L); H3 -> left + centre (H); H4 -> centre + right (L).
        $marks = @(
          @{x=518;y=143;w=37;h=37;n='1';label='H1L · FLT'},
          @{x=438;y=143;w=37;h=37;n='2';label='H2L · DEMP'},
          @{x=498;y=222;w=37;h=37;n='3';label='H3L · XSMT'},
          @{x=438;y=222;w=37;h=37;n='4';label='H4L · FMT'}
        )
        foreach ($m in $marks) {
          $r = [System.Drawing.Rectangle]::new($m.x*$scale,($photoTop+$m.y*$scale),$m.w*$scale,$m.h*$scale)
          $redPen = [System.Drawing.Pen]::new([System.Drawing.Color]::Red,5)
          $g.DrawRectangle($redPen,$r)
          $redPen.Dispose()
          $badge = [System.Drawing.Rectangle]::new(($m.x*$scale)-28,($photoTop+$m.y*$scale)-8,26,26)
          $g.FillEllipse([System.Drawing.Brushes]::DarkRed,$badge)
          $g.DrawEllipse([System.Drawing.Pens]::White,$badge)
          $numFont = [System.Drawing.Font]::new('Segoe UI',12,[System.Drawing.FontStyle]::Bold)
          $g.DrawString($m.n,$numFont,$white,$badge.X+7,$badge.Y+3)
          $numFont.Dispose()
        }

        $legendY = $photoTop + 347*$scale
        $g.DrawString('I²S-hez ajánlott állapot a képen jelölt panelváltozaton:',$smallFont,[System.Drawing.Brushes]::LightGray,18,$legendY+12)
        $g.DrawString('1  H1L / FLT   → L  (jobb oldali két pad)',$legendFont,$white,22,$legendY+42)
        $g.DrawString('2  H2L / DEMP → L  (jobb oldali két pad)',$legendFont,$white,720,$legendY+42)
        $g.DrawString('3  H3L / XSMT → H  (bal oldali két pad)',$legendFont,$white,22,$legendY+82)
        $g.DrawString('4  H4L / FMT   → L  (jobb oldali két pad)',$legendFont,$white,720,$legendY+82)
        $g.DrawString('Áramtalaníts forrasztás előtt. Ha a DAC most jól szól, ne módosítsd; előbb ellenőrizd a meglévő hidakat.',$smallFont,[System.Drawing.Brushes]::Khaki,18,$legendY+128)
        $canvas.Save((Join-Path (Get-Location) $OutputImage),[System.Drawing.Imaging.ImageFormat]::Png)
        $titleFont.Dispose();$smallFont.Dispose();$legendFont.Dispose()
      } finally { $g.Dispose() }
    } finally { $canvas.Dispose() }
  } finally { $back.Dispose() }
} finally { $source.Dispose() }
