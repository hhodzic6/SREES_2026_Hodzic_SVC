function Convert-DarkPlotToLight { param($srcPath, $dstPath)

    param($srcPath, $dstPath)
    $img = New-Object System.Drawing.Bitmap($srcPath)
    $rect = New-Object System.Drawing.Rectangle(0, 0, $img.Width, $img.Height)
    $bmpData = $img.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadWrite, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $bytes = $bmpData.Stride * $img.Height
    $buffer = New-Object byte[] $bytes
    [System.Runtime.InteropServices.Marshal]::Copy($bmpData.Scan0, $buffer, 0, $bytes)

    for ($i = 0; $i -lt $bytes; $i += 3) {
        $b = [int]$buffer[$i]; $g = [int]$buffer[$i+1]; $r = [int]$buffer[$i+2]
        $maxc = [Math]::Max($r, [Math]::Max($g, $b))
        $minc = [Math]::Min($r, [Math]::Min($g, $b))
        $spread = $maxc - $minc

        if ($maxc -lt 12) {
            # pure black background -> white
            $buffer[$i] = 255; $buffer[$i+1] = 255; $buffer[$i+2] = 255
        } elseif ($spread -lt 10 -and $maxc -lt 90) {
            # greyish dim pixel = gridline -> light grey
            $buffer[$i] = 215; $buffer[$i+1] = 215; $buffer[$i+2] = 215
        } else {
            # colored plot line (possibly dim) -> normalize to strong saturated color
            if ($maxc -gt 0) {
                $scale = 235.0 / $maxc
            } else { $scale = 1.0 }
            $nr = [Math]::Min(255, [int]($r * $scale))
            $ng = [Math]::Min(255, [int]($g * $scale))
            $nb = [Math]::Min(255, [int]($b * $scale))
            # darken slightly overall so it reads as a clear line on white, not washed out
            $buffer[$i] = [byte]$nb; $buffer[$i+1] = [byte]$ng; $buffer[$i+2] = [byte]$nr
        }
    }

    [System.Runtime.InteropServices.Marshal]::Copy($buffer, 0, $bmpData.Scan0, $bytes)
    $img.UnlockBits($bmpData)

    $encoderParams = New-Object System.Drawing.Imaging.EncoderParameters(1)
    $encoderParams.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality, 92L)
    $jpegCodec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
    $img.Save($dstPath, $jpegCodec, $encoderParams)
    $img.Dispose()

}
