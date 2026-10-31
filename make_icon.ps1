Add-Type -AssemblyName System.Drawing

# ── Load source BMP and convert to 32bppArgb ─────────────────────
$srcPath = Join-Path $PSScriptRoot "_dev\free_paint_icon.bmp"
$srcBmp  = [System.Drawing.Bitmap]::new($srcPath)

$src = [System.Drawing.Bitmap]::new($srcBmp.Width, $srcBmp.Height,
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$sg = [System.Drawing.Graphics]::FromImage($src)
$sg.DrawImage($srcBmp, 0, 0)
$sg.Dispose()
$srcBmp.Dispose()

$W = $src.Width
$H = $src.Height

# ── Background removal ────────────────────────────────────────────
# The BMP has a checkerboard background of white (255) and grey (221)
# rather than flood-filling from a single colour, we seed from the
# entire border and accept any light unsaturated pixel as background.

function Is-Background($c) {
    # Light (all channels > 180) and low saturation (max-min < 30)
    $minC = [Math]::Min([Math]::Min($c.R, $c.G), $c.B)
    $maxC = [Math]::Max([Math]::Max($c.R, $c.G), $c.B)
    return ($minC -gt 180 -and ($maxC - $minC) -lt 30)
}

$visited = New-Object 'bool[,]' $W, $H
$q = New-Object System.Collections.Generic.Queue[System.Drawing.Point]

# Seed from every pixel on all four edges
for ($x = 0; $x -lt $W; $x++) {
    foreach ($y in @(0, ($H - 1))) {
        if (-not $visited[$x, $y]) {
            $c = $src.GetPixel($x, $y)
            if (Is-Background $c) {
                $visited[$x, $y] = $true
                $q.Enqueue([System.Drawing.Point]::new($x, $y))
            }
        }
    }
}
for ($y = 1; $y -lt ($H - 1); $y++) {
    foreach ($x in @(0, ($W - 1))) {
        if (-not $visited[$x, $y]) {
            $c = $src.GetPixel($x, $y)
            if (Is-Background $c) {
                $visited[$x, $y] = $true
                $q.Enqueue([System.Drawing.Point]::new($x, $y))
            }
        }
    }
}

# BFS flood-fill — remove every reachable background pixel
while ($q.Count -gt 0) {
    $p = $q.Dequeue()
    $x = $p.X; $y = $p.Y
    $src.SetPixel($x, $y, [System.Drawing.Color]::Transparent)

    foreach ($dx in @(-1, 0, 1)) {
        foreach ($dy in @(-1, 0, 1)) {
            if ($dx -eq 0 -and $dy -eq 0) { continue }
            $nx = $x + $dx; $ny = $y + $dy
            if ($nx -ge 0 -and $nx -lt $W -and $ny -ge 0 -and $ny -lt $H -and -not $visited[$nx, $ny]) {
                $c = $src.GetPixel($nx, $ny)
                if (Is-Background $c) {
                    $visited[$nx, $ny] = $true
                    $q.Enqueue([System.Drawing.Point]::new($nx, $ny))
                }
            }
        }
    }
}

Write-Host "Background removed"

# ── Render each icon size from the cleaned source ────────────────
function New-IconBitmap {
    param([int]$sz)
    $bmp = [System.Drawing.Bitmap]::new($sz, $sz,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $g.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.DrawImage($src, 0, 0, $sz, $sz)
    $g.Dispose()
    return $bmp
}

# ── Generate all sizes ────────────────────────────────────────────
$sizes     = @(256, 64, 48, 32, 16)
$pngArrays = @()

foreach ($sz in $sizes) {
    $bmp = New-IconBitmap -sz $sz
    $ms  = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngArrays += ,([byte[]]$ms.ToArray())
    $ms.Dispose()
    $bmp.Dispose()
    Write-Host "  Rendered ${sz}x${sz}"
}

$src.Dispose()

# ── Write ICO file (PNG-inside-ICO, Vista+) ──────────────────────
$outPath = Join-Path $PSScriptRoot "MyPaint.ico"
$fs = [System.IO.File]::Open($outPath, [System.IO.FileMode]::Create)
$bw = New-Object System.IO.BinaryWriter($fs)

$count = $sizes.Count

# ICONDIR header
$bw.Write([uint16]0)       # reserved
$bw.Write([uint16]1)       # type: 1 = icon
$bw.Write([uint16]$count)  # number of images

$offset = [uint32](6 + $count * 16)

for ($i = 0; $i -lt $count; $i++) {
    $sz    = $sizes[$i]
    $bytes = $pngArrays[$i]
    $bw.Write([byte]$(if ($sz -ge 256) { 0 } else { $sz }))
    $bw.Write([byte]$(if ($sz -ge 256) { 0 } else { $sz }))
    $bw.Write([byte]0)
    $bw.Write([byte]0)
    $bw.Write([uint16]1)
    $bw.Write([uint16]32)
    $bw.Write([uint32]$bytes.Length)
    $bw.Write([uint32]$offset)
    $offset += [uint32]$bytes.Length
}

for ($i = 0; $i -lt $count; $i++) {
    $bw.Write([byte[]]$pngArrays[$i])
}

$bw.Flush(); $bw.Close(); $fs.Close()
Write-Host "Icon written: $outPath  ($([int](([System.IO.FileInfo]$outPath).Length/1024)) KB)"
