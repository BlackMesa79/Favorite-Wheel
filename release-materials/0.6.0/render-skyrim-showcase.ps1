$ErrorActionPreference='Stop'
$projectRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $projectRoot
try {
    $renderer=Join-Path $projectRoot 'build/windows/x64/release/WheelPreview.exe'
    if(-not(Test-Path -LiteralPath $renderer)){throw 'Build WheelPreview with xmake before running this script.'}
    & $renderer 'build/skyrim-showcase060-en.bmp' 2560 1440 showcase en skyrim 1.25 0 44 46 1 16 *> 'build/skyrim-showcase060-en.log'
    if($LASTEXITCODE -ne 0){throw 'Offscreen rendering checks failed.'}
    Add-Type -AssemblyName System.Drawing
    $original=[Drawing.Image]::FromFile((Join-Path $projectRoot 'build/skyrim-showcase060-en.bmp'))
    $canvasImage=[Drawing.Bitmap]::new($original)
    $canvas=[Drawing.Graphics]::FromImage($canvasImage)
    $canvas.TextRenderingHint=[Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $titleFont=[Drawing.Font]::new('Segoe UI',38,[Drawing.FontStyle]::Regular,[Drawing.GraphicsUnit]::Pixel)
    $subtitleFont=[Drawing.Font]::new('Segoe UI',24,[Drawing.FontStyle]::Regular,[Drawing.GraphicsUnit]::Pixel)
    $captionFont=[Drawing.Font]::new('Segoe UI',20,[Drawing.FontStyle]::Regular,[Drawing.GraphicsUnit]::Pixel)
    $white=[Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(233,231,224))
    $muted=[Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(151,157,159))
    $canvas.DrawString('Favorite Wheel - Radial Actions',$titleFont,$white,96,45)
    $canvas.DrawString('Skyrim theme | 0.6.0',$subtitleFont,$muted,98,99)
    $label='OFFSCREEN UI PREVIEW'
    $labelSize=$canvas.MeasureString($label,$captionFont)
    $canvas.DrawString($label,$captionFont,$muted,([float](2560-96-$labelSize.Width)),59)
    $canvas.DrawString('Sample inventory',$captionFont,$muted,98,1376)
    $output=Join-Path $PSScriptRoot 'FavoriteWheel-Skyrim-theme-en-2560x1440.png'
    $canvasImage.Save($output,[Drawing.Imaging.ImageFormat]::Png)
    $preview=[Drawing.Bitmap]::new(1280,720)
    $previewCanvas=[Drawing.Graphics]::FromImage($preview)
    $previewCanvas.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $previewCanvas.DrawImage($canvasImage,0,0,1280,720)
    $encoder=[Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object MimeType -eq 'image/jpeg'
    $parameters=[Drawing.Imaging.EncoderParameters]::new(1)
    $parameters.Param[0]=[Drawing.Imaging.EncoderParameter]::new([Drawing.Imaging.Encoder]::Quality,[long]86)
    $previewPath=Join-Path $projectRoot 'build/skyrim-showcase060-en-preview.jpg'
    $preview.Save($previewPath,$encoder,$parameters)
    $parameters.Dispose();$previewCanvas.Dispose();$preview.Dispose()
    $canvas.Dispose();$canvasImage.Dispose();$original.Dispose()
    $titleFont.Dispose();$subtitleFont.Dispose();$captionFont.Dispose();$white.Dispose();$muted.Dispose()
    Get-Item -LiteralPath $output,$previewPath | Select-Object Name,Length
} finally {Pop-Location}
