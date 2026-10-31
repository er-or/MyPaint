# Builds MyPaint.ico from an SVG (default _dev\mypaint_icon.svg), e.g.
#   .\make_icon.ps1 -Source _dev\mypaint_icon_palette.svg
#
# Sizes of SmallMax px and below come from SmallSource when that file exists
# (default _dev\mypaint_icon_small.svg): a version tuned to stay clear when
# tiny, e.g. with thicker edges. Pass -SmallSource '' to use Source for all.
#
# Each icon size is rendered straight from the vector artwork with WPF, so
# small sizes stay crisp. This is not a full SVG renderer, just enough for an
# icon: <g> (with transform="" if needed), <path>, <circle> and <ellipse>, with
# fill / stroke / stroke-width / fill-rule set as attributes or in style="",
# inherited from parent groups.
# Colours can be #rgb, #rrggbb, rgb(r, g, b) or a name.

param(
    [string]$Source      = "_dev\mypaint_icon.svg",
    [string]$SmallSource = "_dev\mypaint_icon_small.svg",
    [int]$SmallMax       = 32
)

Add-Type -AssemblyName PresentationCore, WindowsBase

function Read-Svg($path) {
    if (-not $path) { return $null }
    $full = if ([IO.Path]::IsPathRooted($path)) { $path } else { Join-Path $PSScriptRoot $path }
    if (-not (Test-Path $full)) { return $null }
    return [xml](Get-Content $full -Raw)
}
$mainSvg  = Read-Svg $Source
if (-not $mainSvg) { throw "Icon source not found: $Source" }
$smallSvg = Read-Svg $SmallSource

function Get-Brush($c) {
    if (-not $c -or $c -eq 'none') { return $null }
    if ($c -match '^\s*rgb\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)\s*$') {
        $color = [System.Windows.Media.Color]::FromRgb([byte]$matches[1], [byte]$matches[2], [byte]$matches[3])
    } else {
        $color = [System.Windows.Media.ColorConverter]::ConvertFromString($c)
    }
    $b = New-Object System.Windows.Media.SolidColorBrush $color
    $b.Freeze(); return $b
}

# Presentation attributes of an element, from its attributes and style="",
# layered over what it inherits from its parent group
$styleProps = 'fill', 'stroke', 'stroke-width', 'fill-rule'
function Get-Style($el, $inherited) {
    $s = @{}; foreach ($k in $inherited.Keys) { $s[$k] = $inherited[$k] }
    foreach ($k in $styleProps) { if ($el.HasAttribute($k)) { $s[$k] = $el.GetAttribute($k) } }
    if ($el.HasAttribute('style')) {
        foreach ($decl in $el.GetAttribute('style') -split ';') {
            $kv = $decl -split ':', 2
            if ($kv.Count -eq 2 -and $styleProps -contains $kv[0].Trim()) { $s[$kv[0].Trim()] = $kv[1].Trim() }
        }
    }
    return $s
}

# transform="..." as a WPF transform: translate(), scale(), rotate(a) and matrix(),
# applied left to right as in SVG
function Get-Transform($text) {
    $m = [System.Windows.Media.Matrix]::Identity
    foreach ($t in [regex]::Matches($text, '(\w+)\s*\(([^)]*)\)')) {
        $v = @($t.Groups[2].Value.Trim() -split '[\s,]+' | Where-Object { $_ } | ForEach-Object { [double]$_ })
        # (built with the constructor: Matrix is a struct, and PowerShell can
        # lose changes made by its in-place methods)
        $e = switch ($t.Groups[1].Value) {
            'translate' { 1, 0, 0, 1, $v[0], $(if ($v.Count -gt 1) { $v[1] } else { 0 }) }
            'scale'     { $v[0], 0, 0, $(if ($v.Count -gt 1) { $v[1] } else { $v[0] }), 0, 0 }
            'rotate'    { $r = $v[0] * [Math]::PI / 180
                          [Math]::Cos($r), [Math]::Sin($r), -[Math]::Sin($r), [Math]::Cos($r), 0, 0 }
            'matrix'    { $v[0], $v[1], $v[2], $v[3], $v[4], $v[5] }
            default     { 1, 0, 0, 1, 0, 0 }
        }
        $step = New-Object System.Windows.Media.Matrix ([double]$e[0]), ([double]$e[1]), ([double]$e[2]), ([double]$e[3]), ([double]$e[4]), ([double]$e[5])
        $m = [System.Windows.Media.Matrix]::Multiply($step, $m)   # inner transform runs first
    }
    return New-Object System.Windows.Media.MatrixTransform $m
}

function Draw-Node($dc, $node, $inherited) {
    foreach ($el in $node.ChildNodes) {
        if ($el.NodeType -ne 'Element') { continue }
        $st = Get-Style $el $inherited
        if ($el.LocalName -eq 'g') {
            $tf = $el.HasAttribute('transform')
            if ($tf) { $dc.PushTransform((Get-Transform $el.GetAttribute('transform'))) }
            Draw-Node $dc $el $st
            if ($tf) { $dc.Pop() }
            continue
        }
        # (if/elseif, not switch: 'continue' inside a switch doesn't leave the loop)
        if ($el.LocalName -eq 'circle') {
            $geo = New-Object System.Windows.Media.EllipseGeometry (
                (New-Object System.Windows.Point ([double]$el.cx), ([double]$el.cy)), [double]$el.r, [double]$el.r)
        } elseif ($el.LocalName -eq 'ellipse') {
            $geo = New-Object System.Windows.Media.EllipseGeometry (
                (New-Object System.Windows.Point ([double]$el.cx), ([double]$el.cy)), [double]$el.rx, [double]$el.ry)
        } elseif ($el.LocalName -eq 'path') {
            $rule = if ($st['fill-rule'] -eq 'evenodd') { 'F0 ' } else { 'F1 ' }
            $geo  = [System.Windows.Media.Geometry]::Parse($rule + $el.d)
        } else { continue }
        $pen = $null
        $stroke = Get-Brush $st['stroke']
        if ($stroke) {
            $w = if ($st['stroke-width']) { [double]$st['stroke-width'] } else { 1.0 }
            $pen = New-Object System.Windows.Media.Pen $stroke, $w
            $pen.LineJoin = 'Round'; $pen.StartLineCap = 'Round'; $pen.EndLineCap = 'Round'
        }
        $dc.DrawGeometry((Get-Brush $st['fill']), $pen, $geo)
    }
}

# ── Render one size ───────────────────────────────────────────────
function New-IconPng {
    param([int]$sz, [xml]$svg)
    $vb  = $svg.svg.viewBox -split '[ ,]+' | ForEach-Object { [double]$_ }
    $vbW = $vb[2]
    $dv = New-Object System.Windows.Media.DrawingVisual
    $dc = $dv.RenderOpen()
    $dc.PushTransform((New-Object System.Windows.Media.ScaleTransform ($sz / $vbW), ($sz / $vbW)))
    # Clip to the viewBox: artwork may run slightly past its edge
    $dc.PushClip((New-Object System.Windows.Media.RectangleGeometry (New-Object System.Windows.Rect $vb[0], $vb[1], $vb[2], $vb[3])))
    Draw-Node $dc $svg.svg @{ fill = 'black' }   # SVG's default fill
    $dc.Pop(); $dc.Pop(); $dc.Close()

    $rtb = New-Object System.Windows.Media.Imaging.RenderTargetBitmap $sz, $sz, 96, 96, ([System.Windows.Media.PixelFormats]::Pbgra32)
    $rtb.Render($dv)
    $enc = New-Object System.Windows.Media.Imaging.PngBitmapEncoder
    $enc.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($rtb))
    $ms = New-Object System.IO.MemoryStream
    $enc.Save($ms)
    return ,([byte[]]$ms.ToArray())
}

# ── Generate all sizes ────────────────────────────────────────────
$sizes     = @(256, 64, 48, 32, 24, 16)
$pngArrays = @()
foreach ($sz in $sizes) {
    $small = $smallSvg -and $sz -le $SmallMax
    $pngArrays += ,(New-IconPng -sz $sz -svg $(if ($small) { $smallSvg } else { $mainSvg }))
    Write-Host "  Rendered ${sz}x${sz}$(if ($small) { ' (small-size version)' })"
}

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
