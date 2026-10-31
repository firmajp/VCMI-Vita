# SPDX-License-Identifier: CC0-1.0
$ErrorActionPreference = 'Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
Add-Type -AssemblyName System.Drawing
$bitmap = [System.Drawing.Bitmap]::new(960,544)
$g = [System.Drawing.Graphics]::FromImage($bitmap)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$gradient = [System.Drawing.Drawing2D.LinearGradientBrush]::new([System.Drawing.Point]::new(0,0),[System.Drawing.Point]::new(960,544),[System.Drawing.Color]::FromArgb(9,18,32),[System.Drawing.Color]::FromArgb(17,37,52))
$g.FillRectangle($gradient,0,0,960,544)
$contour = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(30,57,70),1)
for($row=0;$row -lt 13;$row++) {
    $points = [System.Collections.Generic.List[System.Drawing.PointF]]::new()
    for($x=480;$x -lt 1080;$x+=5) {
        $y=100+$row*18+25*[Math]::Sin($x/150+$row*.2)+12*[Math]::Sin($x/60-$row*.3)
        $points.Add([System.Drawing.PointF]::new($x,$y))
    }
    $g.DrawLines($contour,$points.ToArray())
}
$ring = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(58,83,95),1)
foreach($r in @(47,74,88)) { $g.DrawEllipse($ring,792-$r,136-$r,2*$r,2*$r) }
for($i=0;$i -lt 24;$i++) {
    $angle=$i*2*[Math]::PI/24
    $g.DrawLine($ring,[single](792+[Math]::Sin($angle)*84),[single](136-[Math]::Cos($angle)*84),[single](792+[Math]::Sin($angle)*88),[single](136-[Math]::Cos($angle)*88))
}
$gold = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(205,177,111))
$dim = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(62,91,103))
$g.FillPolygon($gold,[System.Drawing.Point[]]@([System.Drawing.Point]::new(792,68),[System.Drawing.Point]::new(802,136),[System.Drawing.Point]::new(792,124),[System.Drawing.Point]::new(782,136)))
$g.FillPolygon($dim,[System.Drawing.Point[]]@([System.Drawing.Point]::new(792,204),[System.Drawing.Point]::new(782,136),[System.Drawing.Point]::new(792,148),[System.Drawing.Point]::new(802,136)))
$g.DrawLine($ring,744,136,840,136)
$fonts = [System.Drawing.Text.PrivateFontCollection]::new()
$fonts.AddFontFile((Resolve-Path 'src/vcmi/Mods/vcmi/Content/Data/NotoSans-Medium.ttf').Path)
$title = [System.Drawing.Font]::new($fonts.Families[0],76,[System.Drawing.FontStyle]::Regular,[System.Drawing.GraphicsUnit]::Pixel)
$small = [System.Drawing.Font]::new($fonts.Families[0],16,[System.Drawing.FontStyle]::Regular,[System.Drawing.GraphicsUnit]::Pixel)
$ink = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(237,235,221))
$g.DrawString('VCMI',$title,$ink,78,76)
$x=85
foreach($c in 'VITA'.ToCharArray()) { $g.DrawString([string]$c,$small,$gold,$x,184);$x+=20 }
$g.DrawLine([System.Drawing.Pen]::new($gold,2),84,230,196,230)
$g.DrawLine($ring,212,230,876,230)
$card = [System.Drawing.Drawing2D.GraphicsPath]::new()
$card.AddArc(70,264,32,32,180,90);$card.AddArc(858,264,32,32,270,90)
$card.AddArc(858,463,32,32,0,90);$card.AddArc(70,463,32,32,90,90);$card.CloseFigure()
$g.FillPath([System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(10,23,37)),$card)
$g.DrawPath($ring,$card)
foreach($x in @(467,480,493)) { $g.FillEllipse($gold,$x-2,514,4,4) }
$bitmap.Save((Join-Path (Get-Location) 'packaging/vitaBoot.png'),[System.Drawing.Imaging.ImageFormat]::Png)
$status = [System.Drawing.Font]::new($fonts.Families[0],20,[System.Drawing.FontStyle]::Regular,[System.Drawing.GraphicsUnit]::Pixel)
$y=299
foreach($line in @('Loading creatures','Loading factions and towns','Loading map objects')) {
    $g.DrawString($line,$status,$ink,96,$y);$y+=38
}
$g.FillRectangle([System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(39,62,76)),96,457,768,4)
$g.FillRectangle($gold,276,457,154,4)
$bitmap.Save((Join-Path (Get-Location) 'docs/loading-preview.png'),[System.Drawing.Imaging.ImageFormat]::Png)
$status.Dispose()
$g.Dispose();$bitmap.Dispose();$gradient.Dispose();$title.Dispose();$small.Dispose();$fonts.Dispose()
Write-Output 'Boot artwork generated: 960x544, original geometry and bundled OFL font.'

Pop-Location
