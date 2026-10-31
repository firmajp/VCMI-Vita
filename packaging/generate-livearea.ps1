# SPDX-License-Identifier: CC0-1.0
# Original artwork/generator: CC0-1.0. Bundled Noto font remains under SIL OFL.
param([string]$Pngquant = 'pngquant')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = Split-Path $PSScriptRoot -Parent
$target = Join-Path $PSScriptRoot 'sce_sys'
New-Item -ItemType Directory -Force "$target/livearea/contents" | Out-Null
$image = [System.Drawing.Image]::FromFile("$PSScriptRoot/vitaBoot.png")
$fontCollection = [System.Drawing.Text.PrivateFontCollection]::new()
$fontCollection.AddFontFile("$root/src/vcmi/Mods/vcmi/Content/Data/NotoSans-Medium.ttf")
foreach($item in @(@('icon0.png',128,128),@('pic0.png',960,544),@('livearea/contents/bg.png',840,500),@('livearea/contents/startup.png',280,158))) {
    $name=$item[0];$width=[int]$item[1];$height=[int]$item[2]
    $bitmap=[System.Drawing.Bitmap]::new($width,$height)
    $g=[System.Drawing.Graphics]::FromImage($bitmap)
    $g.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.TextRenderingHint=[System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.Clear([System.Drawing.Color]::FromArgb(10,23,37))
    if($name -eq 'icon0.png') {
        $pen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(91,123,131),2)
        $g.DrawEllipse($pen,12,12,104,104)
        $g.DrawEllipse($pen,20,20,88,88)
        $g.FillPolygon([System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(215,184,112)),[System.Drawing.Point[]]@([System.Drawing.Point]::new(64,22),[System.Drawing.Point]::new(75,69),[System.Drawing.Point]::new(64,59),[System.Drawing.Point]::new(53,69)))
        $g.FillPolygon([System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(72,111,129)),[System.Drawing.Point[]]@([System.Drawing.Point]::new(64,106),[System.Drawing.Point]::new(53,59),[System.Drawing.Point]::new(64,69),[System.Drawing.Point]::new(75,59)))
        $pen.Dispose()
    } elseif($name -like '*startup.png') {
        $font=[System.Drawing.Font]::new($fontCollection.Families[0],43,[System.Drawing.FontStyle]::Regular,[System.Drawing.GraphicsUnit]::Pixel)
        $g.DrawString('VCMI',$font,[System.Drawing.Brushes]::Ivory,66,41)
        $pen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(205,177,111),2)
        $g.DrawLine($pen,52,109,228,109)
        $pen.Dispose();$font.Dispose()
    } else {
        $g.DrawImage($image,0,0,$width,$height)
    }
    $raw=Join-Path $PSScriptRoot ('raw-'+[System.IO.Path]::GetFileName($name))
    $output=Join-Path $target $name
    $bitmap.Save($raw,[System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose();$bitmap.Dispose()
    & $Pngquant --force --strip --nofs --speed 1 --output $output 128 -- $raw
    if($LASTEXITCODE) { throw "pngquant failed for $name" }
    Remove-Item -LiteralPath $raw
}
$image.Dispose();$fontCollection.Dispose()
@'
<?xml version="1.0" encoding="utf-8"?>
<livearea style="a1" format-ver="01.00" content-rev="100">
  <livearea-background><image>bg.png</image></livearea-background>
  <gate><startup-image>startup.png</startup-image></gate>
</livearea>
'@ | Set-Content -LiteralPath "$target/livearea/contents/template.xml" -Encoding utf8
Write-Output 'Original LiveArea PNG-8 artwork generated.'
