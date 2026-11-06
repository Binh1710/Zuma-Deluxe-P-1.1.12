$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$levelsRoot = Join-Path $projectRoot 'levels'
$sourcePath = Join-Path $levelsRoot 'levels.xml'
$outputPath = Join-Path $PSScriptRoot 'zuma-deluxe-plus-145-levels.html'
$source = Get-Content -Raw -Encoding UTF8 -LiteralPath $sourcePath

function Get-Attribute([string]$text, [string]$name) {
    $match = [regex]::Match($text, "(?i)\b$([regex]::Escape($name))\s*=\s*`"([^`"]*)`"")
    if ($match.Success) { return $match.Groups[1].Value }
    return ''
}

function Get-Number([string]$value, [double]$default) {
    if ($value) { return [double]::Parse($value, [Globalization.CultureInfo]::InvariantCulture) }
    return $default
}

function Html([string]$value) {
    return [System.Net.WebUtility]::HtmlEncode($value)
}

# [IO.Path]::GetRelativePath is missing on Windows PowerShell 5.1
function Get-RelativePath([string]$path) {
    $base = [Uri]((Join-Path $PSScriptRoot '.') + '\')
    return [Uri]::UnescapeDataString($base.MakeRelativeUri([Uri]$path).ToString())
}

function Find-MapImage([string]$id, [string]$imageBase) {
    $folder = Join-Path $levelsRoot $id
    if (-not (Test-Path -LiteralPath $folder)) { return $null }

    $extensions = @('.jpg', '.png', '.gif', '.jpeg')
    $bases = @()
    if ($imageBase) { $bases += $imageBase }
    $bases += @('a', $id)

    foreach ($base in ($bases | Select-Object -Unique)) {
        foreach ($extension in $extensions) {
            $candidate = Join-Path $folder ($base + $extension)
            if (Test-Path -LiteralPath $candidate) {
                $relative = (Get-RelativePath $candidate)
                return [Uri]::EscapeUriString($relative)
            }
        }
    }

    $fallback = Get-ChildItem -LiteralPath $folder -File |
        Where-Object { $_.Extension -match '^\.(jpg|jpeg|png|gif)$' -and -not $_.BaseName.StartsWith('_') } |
        Select-Object -First 1
    if ($fallback) {
        $relative = (Get-RelativePath $fallback.FullName)
        return [Uri]::EscapeUriString($relative)
    }
    return $null
}

$curveCache = @{}
function Read-Curve([string]$id, [string]$curveBase) {
    $path = Join-Path (Join-Path $levelsRoot $id) ($curveBase + '.dat')
    if ($curveCache.ContainsKey($path)) { return $curveCache[$path] }
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    $bytes = [IO.File]::ReadAllBytes($path)
    if ($bytes.Length -lt 20 -or [Text.Encoding]::ASCII.GetString($bytes, 0, 4) -ne 'CURV') { return $null }
    $count = [BitConverter]::ToUInt32($bytes, 16)
    if ($count -lt 2 -or (20 + $count * 10) -gt $bytes.Length) { return $null }
    $points = [Collections.Generic.List[object]]::new()
    $length = 0.0
    $previousX = 0
    $previousY = 0
    for ($i = 0; $i -lt $count; $i++) {
        $offset = 20 + $i * 10
        $x = [BitConverter]::ToInt32($bytes, $offset)
        $y = [BitConverter]::ToInt32($bytes, $offset + 4)
        # Some community-made curves contain a generator metadata block after
        # the CURV header instead of the classic control-point table.
        if ([Math]::Abs([long]$x) -gt 5000 -or [Math]::Abs([long]$y) -gt 5000) {
            $curveCache[$path] = $null
            return $null
        }
        if ($i -gt 0) {
            $dx = $x - $previousX
            $dy = $y - $previousY
            $length += [Math]::Sqrt($dx * $dx + $dy * $dy)
        }
        $points.Add([pscustomobject]@{ X = [double]$x; Y = [double]$y })
        $previousX = $x
        $previousY = $y
    }
    $result = [pscustomobject]@{ Path = $path; Points = $points; Length = [Math]::Round($length); Count = $count }
    $curveCache[$path] = $result
    return $result
}

function Sample-Curve($curve, [double]$step = 14) {
    $samples = [Collections.Generic.List[object]]::new()
    if (-not $curve -or $curve.Points.Count -lt 2) { return $samples }
    for ($i = 1; $i -lt $curve.Points.Count; $i++) {
        $a = $curve.Points[$i - 1]
        $b = $curve.Points[$i]
        $dx = $b.X - $a.X
        $dy = $b.Y - $a.Y
        $distance = [Math]::Sqrt($dx * $dx + $dy * $dy)
        $parts = [Math]::Max(1, [Math]::Ceiling($distance / $step))
        for ($part = 0; $part -lt $parts; $part++) {
            $t = $part / $parts
            $samples.Add([pscustomobject]@{ X = $a.X + $dx * $t; Y = $a.Y + $dy * $t })
        }
    }
    $samples.Add($curve.Points[$curve.Points.Count - 1])
    return $samples
}

function Measure-RouteOverlap($routes) {
    if ($routes.Count -lt 2) { return [pscustomobject]@{ Percent = 0; Label = 'Không áp dụng'; Note = 'Map chỉ có một tuyến.' } }
    $threshold = 28.0
    $cellSize = 32.0
    $highest = 0.0
    for ($left = 0; $left -lt $routes.Count; $left++) {
        for ($right = $left + 1; $right -lt $routes.Count; $right++) {
            $samplesA = @(Sample-Curve $routes[$left])
            $samplesB = @(Sample-Curve $routes[$right])
            if (-not $samplesA.Count -or -not $samplesB.Count) { continue }
            if ($samplesA.Count -gt $samplesB.Count) { $temp = $samplesA; $samplesA = $samplesB; $samplesB = $temp }
            $grid = @{}
            foreach ($point in $samplesB) {
                $gx = [Math]::Floor($point.X / $cellSize)
                $gy = [Math]::Floor($point.Y / $cellSize)
                $key = "$gx,$gy"
                if (-not $grid.ContainsKey($key)) { $grid[$key] = [Collections.Generic.List[object]]::new() }
                $grid[$key].Add($point)
            }
            $near = 0
            foreach ($point in $samplesA) {
                $gx = [Math]::Floor($point.X / $cellSize)
                $gy = [Math]::Floor($point.Y / $cellSize)
                $found = $false
                for ($ox = -1; $ox -le 1 -and -not $found; $ox++) {
                    for ($oy = -1; $oy -le 1 -and -not $found; $oy++) {
                        $key = "$($gx + $ox),$($gy + $oy)"
                        if (-not $grid.ContainsKey($key)) { continue }
                        foreach ($other in $grid[$key]) {
                            $dx = $point.X - $other.X
                            $dy = $point.Y - $other.Y
                            if (($dx * $dx + $dy * $dy) -le ($threshold * $threshold)) { $found = $true; break }
                        }
                    }
                }
                if ($found) { $near++ }
            }
            $percent = 100.0 * $near / $samplesA.Count
            if ($percent -gt $highest) { $highest = $percent }
        }
    }
    $highest = [Math]::Round($highest)
    if ($highest -ge 24) { return [pscustomobject]@{ Percent = $highest; Label = 'Cao'; Note = 'Nhiều đoạn của hai tuyến chạy sát/chồng nhau; bi ở tuyến trước có thể che hoặc gây nhầm tuyến sau.' } }
    if ($highest -ge 10) { return [pscustomobject]@{ Percent = $highest; Label = 'Trung bình'; Note = 'Có một số đoạn hai tuyến chạy sát nhau, cần chú ý tránh bắn nhầm.' } }
    return [pscustomobject]@{ Percent = $highest; Label = 'Thấp'; Note = 'Hai tuyến phần lớn tách biệt, ít nguy cơ che lẫn nhau.' }
}

# Line of sight from the frog: a route point counts as "blocked" when it lies under a tunnel
# cutout, or when a straight shot to it would first hit balls of another stretch of track.
# The end of each route (where the chain kills you) weighs most.
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;

public static class ZumaSight {
    const double Step = 8, HitRadius = 26, SelfGap = 40;

    static List<double[]> Sample(double[] xy) {
        var s = new List<double[]>();
        for (int i = 2; i < xy.Length; i += 2) {
            double ax = xy[i - 2], ay = xy[i - 1], dx = xy[i] - ax, dy = xy[i + 1] - ay;
            int parts = Math.Max(1, (int)Math.Ceiling(Math.Sqrt(dx * dx + dy * dy) / Step));
            for (int k = 0; k < parts; k++) s.Add(new[] { ax + dx * k / parts, ay + dy * k / parts });
        }
        s.Add(new[] { xy[xy.Length - 2], xy[xy.Length - 1] });
        return s;
    }

    static bool[,] LoadMask(string[] paths) {
        bool[,] mask = null;
        foreach (var path in paths) {
            using (var bmp = new Bitmap(path)) {
                if (mask == null) mask = new bool[640, 480];
                for (int y = 0; y < 480; y++)
                    for (int x = 0; x < 640; x++) {
                        int bx = x * bmp.Width / 640, by = y * bmp.Height / 480;
                        if (bmp.GetPixel(bx, by).GetBrightness() > 0.5f) mask[x, y] = true;
                    }
            }
        }
        return mask;
    }

    // returns { blocked (weighted, worst route mixed with mean), blocked share of the worst route's last 30%, worst route index }
    public static double[] Analyze(double[][] routes, double fx, double fy, string[] maskPaths) {
        var mask = LoadMask(maskPaths);
        var samples = new List<List<double[]>>();
        foreach (var r in routes) samples.Add(Sample(r));
        Func<double[], bool> hidden = p => {
            int x = (int)p[0], y = (int)p[1];
            return mask != null && x >= 0 && y >= 0 && x < 640 && y < 480 && mask[x, y];
        };
        var all = new List<double[]>();
        foreach (var s in samples) foreach (var p in s) if (!hidden(p)) all.Add(p);

        double worst = 0, sum = 0, worstEnd = 0; int worstIndex = 0;
        for (int r = 0; r < samples.Count; r++) {
            var s = samples[r];
            double w = 0, wb = 0; int endTotal = 0, endBlocked = 0;
            for (int i = 0; i < s.Count; i++) {
                var p = s[i];
                if (p[0] < 0 || p[1] < 0 || p[0] > 640 || p[1] > 480) continue;
                bool blocked = hidden(p);
                double vx = p[0] - fx, vy = p[1] - fy, len = Math.Sqrt(vx * vx + vy * vy);
                if (!blocked && len > 1) {
                    double ux = vx / len, uy = vy / len;
                    foreach (var q in all) {
                        double qx = q[0] - fx, qy = q[1] - fy;
                        double t = qx * ux + qy * uy;
                        if (t <= 0 || t >= len - HitRadius) continue;
                        double ex = q[0] - p[0], ey = q[1] - p[1];
                        if (ex * ex + ey * ey < SelfGap * SelfGap) continue;
                        double perp = Math.Abs(qx * uy - qy * ux);
                        if (perp < HitRadius) { blocked = true; break; }
                    }
                }
                double f = s.Count > 1 ? (double)i / (s.Count - 1) : 1;
                double weight = 0.2 + 0.8 * f * f;
                w += weight; if (blocked) wb += weight;
                if (f >= 0.7) { endTotal++; if (blocked) endBlocked++; }
            }
            double share = w > 0 ? wb / w : 0;
            sum += share;
            if (share >= worst) { worst = share; worstIndex = r; worstEnd = endTotal > 0 ? (double)endBlocked / endTotal : 0; }
        }
        double mean = samples.Count > 0 ? sum / samples.Count : 0;
        return new[] { 0.7 * worst + 0.3 * mean, worstEnd, worstIndex };
    }
}
'@

function Measure-Sight($graphics, $routes) {
    if (-not $routes.Count -or $graphics.Space) { return $null }
    $folder = Join-Path $levelsRoot $graphics.Id
    $masks = @(foreach ($cutout in $graphics.Cutouts) {
        Get-ChildItem -LiteralPath $folder -File -ErrorAction SilentlyContinue |
            Where-Object { $_.BaseName -ieq ('_' + $cutout) -and $_.Extension -match '^\.(gif|png|jpg|jpeg)$' } |
            Select-Object -First 1 -ExpandProperty FullName
    })
    $routeArrays = [double[][]]@($routes | ForEach-Object { ,[double[]]@($_.Points | ForEach-Object { $_.X; $_.Y }) })
    $result = [ZumaSight]::Analyze($routeArrays, [double]$graphics.Gx, [double]$graphics.Gy, [string[]]$masks)
    return [pscustomobject]@{ Percent = [Math]::Round(100 * $result[0]); EndPercent = [Math]::Round(100 * $result[1]); WorstRoute = [int]$result[2] + 1 }
}

$graphicsById = @{}
$graphicsMatches = [regex]::Matches(
    $source,
    '<Graphics\s+([^>]*\bid="[^"]+"[^>]*)>(.*?)</Graphics>',
    [Text.RegularExpressions.RegexOptions]::IgnoreCase -bor [Text.RegularExpressions.RegexOptions]::Singleline
)
foreach ($match in $graphicsMatches) {
    $attributes = $match.Groups[1].Value
    $body = $match.Groups[2].Value
    $id = (Get-Attribute $attributes 'id').Trim().ToLowerInvariant()
    if (-not $id) { continue }
    $tracks = [regex]::Matches($attributes, '(?i)\bcurve\d*\s*=').Count
    $curves = @([regex]::Matches($attributes, '(?i)\bcurve\d*\s*=\s*"([^"]+)"') | ForEach-Object { $_.Groups[1].Value })
    $graphicsById[$id] = [pscustomobject]@{
        Id = $id
        Name = Get-Attribute $attributes 'dispname'
        ImageBase = Get-Attribute $attributes 'image'
        Tracks = $tracks
        Curves = $curves
        Treasure = [regex]::Matches($body, '(?i)<TreasurePoint\b').Count
        HasTunnel = [regex]::IsMatch($body, '(?i)<Cutout\b')
        HasSprite = [regex]::IsMatch($body, '(?i)<Sprite\b')
        Danger = Get-Attribute $attributes 'danger'
        Space = (Get-Attribute $attributes 'space') -eq 'true'
        Gx = Get-Number (Get-Attribute $attributes 'gx') 320
        Gy = Get-Number (Get-Attribute $attributes 'gy') 240
        Cutouts = @([regex]::Matches($body, '(?i)<Cutout\b[^>]*\bimage="([^"]+)"') | ForEach-Object { $_.Groups[1].Value })
    }
}

$settingsById = @{}
$settingMatches = [regex]::Matches($source, '<Settings\s+([^>]*)/>', [Text.RegularExpressions.RegexOptions]::IgnoreCase)
foreach ($match in $settingMatches) {
    $attributes = $match.Groups[1].Value
    $id = (Get-Attribute $attributes 'id').Trim().ToLowerInvariant()
    if (-not $id -or $settingsById.ContainsKey($id)) { continue }
    $settingsById[$id] = [pscustomobject]@{
        Id = $id
        Speed = Get-Attribute $attributes 'speed'
        Start = Get-Attribute $attributes 'start'
        Score = Get-Attribute $attributes 'score'
        Colors = Get-Attribute $attributes 'colors'
        SlowFactor = Get-Attribute $attributes 'slowfactor'
        Single = Get-Attribute $attributes 'single'
        ParTime = Get-Attribute $attributes 'partime'
    }
}

$progressionById = @{}
[regex]::Matches($source, '<Level\s+[^>]*graphics="([^"]+)"[^>]*progression="([sdt])"[^>]*/>', [Text.RegularExpressions.RegexOptions]::IgnoreCase) |
    ForEach-Object { $progressionById[$_.Groups[1].Value.Trim().ToLowerInvariant()] = $_.Groups[2].Value.ToLowerInvariant() }

# Weighted pressure score. Each factor is ranked against all levels (0 = easiest, 1 = hardest),
# then combined; the final grade comes from the level's place in the whole set.
$difficultyFactors = @(
    [pscustomobject]@{ Key = 'Pace';    Weight = 0.26; Label = 'tốc độ so với độ dài đường'; EasyLabel = 'bi chạy chậm so với độ dài đường' }
    [pscustomobject]@{ Key = 'Sight';   Weight = 0.20; Label = 'đường bắn bị che (nhất là đoạn cuối)'; EasyLabel = 'đường bắn thoáng' }
    [pscustomobject]@{ Key = 'Colors';  Weight = 0.15; Label = 'nhiều màu bi'; EasyLabel = 'ít màu bi' }
    [pscustomobject]@{ Key = 'Fill';    Weight = 0.10; Label = 'lượng bi đầu màn so với đường'; EasyLabel = 'ít bi đầu màn' }
    [pscustomobject]@{ Key = 'Tracks';  Weight = 0.08; Label = 'nhiều tuyến cùng lúc'; EasyLabel = 'chỉ một tuyến' }
    [pscustomobject]@{ Key = 'Score';   Weight = 0.08; Label = 'mục tiêu điểm Zuma cao'; EasyLabel = 'mục tiêu điểm thấp' }
    [pscustomobject]@{ Key = 'Slow';    Weight = 0.06; Label = 'bi ít chậm lại gần lỗ (slowfactor thấp)'; EasyLabel = 'bi chậm lại nhiều gần lỗ' }
    [pscustomobject]@{ Key = 'Speed';   Weight = 0.04; Label = 'tốc độ đoàn bi'; EasyLabel = 'tốc độ đoàn bi thấp' }
    [pscustomobject]@{ Key = 'Overlap'; Weight = 0.03; Label = 'các tuyến chạy sát/che nhau'; EasyLabel = 'các tuyến tách biệt' })
# share of levels per grade, easiest first
$difficultyGrades = @(
    [pscustomobject]@{ Name = 'Dễ';       Share = 0.20 }
    [pscustomobject]@{ Name = 'Trung bình'; Share = 0.30 }
    [pscustomobject]@{ Name = 'Khó';      Share = 0.25 }
    [pscustomobject]@{ Name = 'Rất khó';  Share = 0.15 }
    [pscustomobject]@{ Name = 'Cực khó';  Share = 0.10 }
)

function Get-DifficultyFactors($graphics, $settings, [double]$averageLength, [double]$overlapPercent, [double]$sightPercent) {
    $speed = Get-Number $settings.Speed 0.5
    $length = [Math]::Max($averageLength, 300)
    return @{
        Pace    = $speed * 1000 / $length                      # how fast the chain eats the route
        Colors  = Get-Number $settings.Colors 4                # classic stages without colors= use 4
        Fill    = (Get-Number $settings.Start 0) * 32 / $length # ~32 px per ball
        Tracks  = [double]$graphics.Tracks
        Score   = Get-Number $settings.Score 0
        Slow    = -(Get-Number $settings.SlowFactor 4)         # lower slowfactor = less slow-down = harder
        Speed   = $speed
        Overlap = $overlapPercent
        Sight   = $sightPercent
    }
}

function Get-PercentRank([double[]]$values, [double]$value) {
    if ($values.Count -le 1) { return 0.5 }
    $below = @($values | Where-Object { $_ -lt $value }).Count
    $equal = @($values | Where-Object { $_ -eq $value }).Count
    return ($below + ($equal - 1) / 2) / ($values.Count - 1)
}

function Set-Difficulty($levels) {
    $columns = @{}
    foreach ($factor in $difficultyFactors) { $columns[$factor.Key] = [double[]]@($levels | ForEach-Object { $_.Factors[$factor.Key] }) }
    foreach ($level in $levels) {
        $parts = foreach ($factor in $difficultyFactors) {
            $rank = Get-PercentRank $columns[$factor.Key] $level.Factors[$factor.Key]
            [pscustomobject]@{ Factor = $factor; Rank = $rank; Points = $factor.Weight * $rank }
        }
        $level.Pressure = [Math]::Round(100 * ($parts | Measure-Object Points -Sum).Sum)
        $level.Parts = $parts
    }
    $sorted = @($levels | Sort-Object Pressure)
    $index = 0
    $limit = 0.0
    foreach ($grade in $difficultyGrades) {
        $limit += $grade.Share * $sorted.Count
        while ($index -lt $sorted.Count -and ($index -lt [Math]::Round($limit) -or $grade -eq $difficultyGrades[-1])) {
            $sorted[$index].Difficulty = $grade.Name
            $index++
        }
    }
    # same pressure must get the same grade
    for ($i = 1; $i -lt $sorted.Count; $i++) {
        if ($sorted[$i].Pressure -eq $sorted[$i - 1].Pressure) { $sorted[$i].Difficulty = $sorted[$i - 1].Difficulty }
    }
    foreach ($level in $levels) {
        $hard = @($level.Parts | Where-Object { $_.Rank -ge 0.6 } | Sort-Object Points -Descending | Select-Object -First 3)
        $easy = @($level.Parts | Where-Object { $_.Rank -le 0.3 } | Sort-Object { $_.Factor.Weight * (1 - $_.Rank) } -Descending | Select-Object -First 2)
        $note = "Điểm áp lực $($level.Pressure)/100."
        if ($hard.Count) { $note += ' Nặng nhất: ' + (($hard | ForEach-Object { $_.Factor.Label }) -join ', ') + '.' }
        if ($easy.Count) { $note += ' Nhẹ: ' + (($easy | ForEach-Object { $_.Factor.EasyLabel }) -join ', ') + '.' }
        $level.Note = $note
    }
}

$stages = @()
for ($stageNumber = 1; $stageNumber -le 13; $stageNumber++) {
    $stageMatch = [regex]::Match($source, "(?i)\bstage$stageNumber\s*=\s*`"([^`"]+)`"")
    $difficultyMatch = [regex]::Match($source, "(?i)\bdiffi$stageNumber\s*=\s*`"([^`"]+)`"")
    $mapIds = @($stageMatch.Groups[1].Value.Split(',') | ForEach-Object { $_.Trim().ToLowerInvariant() } | Where-Object { $_ })
    $settingIds = @($difficultyMatch.Groups[1].Value.Split(',') | ForEach-Object { $_.Trim().ToLowerInvariant() } | Where-Object { $_ })
    $levels = @()
    for ($index = 0; $index -lt $mapIds.Count; $index++) {
        $mapId = $mapIds[$index]
        $settingId = $settingIds[$index]
        $graphics = $graphicsById[$mapId]
        $settings = $settingsById[$settingId]
        $progression = $progressionById[$mapId]
        $gauntletSettings = $settingsById[$progression + '35']
        if (-not $graphics) { throw "Missing Graphics definition: $mapId" }
        if (-not $settings) { throw "Missing Settings definition: $settingId" }
        $name = if ($graphics.Name) { $graphics.Name } elseif ($mapId -eq 'space') { 'Space' } else { $mapId }
        $image = Find-MapImage $mapId $graphics.ImageBase
        $routes = @($graphics.Curves | ForEach-Object { Read-Curve $mapId $_ } | Where-Object { $_ })
        $routeLengths = @($routes | ForEach-Object { [long]$_.Length })
        $totalLength = ($routeLengths | Measure-Object -Sum).Sum
        $averageLength = if ($routeLengths.Count) { [Math]::Round($totalLength / $routeLengths.Count) } else { 0 }
        if ($graphics.Tracks -ge 2 -and $routeLengths.Count -lt 2) {
            $asymmetry = 0
            $balanceLabel = 'Chưa xác định'
            $balanceNote = 'File CURV dùng định dạng generator tùy biến nên không thể so sánh độ dài tuyến một cách đáng tin cậy.'
        } elseif ($routeLengths.Count -ge 2) {
            $minimumLength = ($routeLengths | Measure-Object -Minimum).Minimum
            $maximumLength = ($routeLengths | Measure-Object -Maximum).Maximum
            $asymmetry = if ($minimumLength -gt 0) { [Math]::Round(100 * ($maximumLength - $minimumLength) / $minimumLength) } else { 0 }
            if ($asymmetry -le 8) { $balanceLabel = 'Cân bằng'; $balanceNote = 'Các tuyến có độ dài gần tương đương.' }
            elseif ($asymmetry -le 20) { $balanceLabel = 'Lệch nhẹ'; $balanceNote = 'Một tuyến dài hơn nhưng chênh lệch chưa lớn.' }
            elseif ($asymmetry -le 40) { $balanceLabel = 'Bất cân xứng'; $balanceNote = 'Một tuyến dài hơn rõ rệt, nhịp áp lực giữa các tuyến khác nhau.' }
            else { $balanceLabel = 'Rất lệch'; $balanceNote = 'Độ dài các tuyến chênh lệch mạnh; tuyến ngắn thường gây áp lực sớm hơn.' }
        } else {
            $asymmetry = 0
            $balanceLabel = 'Không áp dụng'
            $balanceNote = 'Map chỉ có một tuyến.'
        }
        $overlap = if ($graphics.Tracks -ge 2 -and $routes.Count -lt 2) {
            [pscustomobject]@{ Percent = 0; Label = 'Chưa xác định'; Note = 'Không đủ dữ liệu điểm điều khiển để đo mức hai tuyến chạy sát hoặc chồng nhau.' }
        } else {
            Measure-RouteOverlap $routes
        }
        $sight = Measure-Sight $graphics $routes
        $scoreValue = if ($settings.Score) { [int]$settings.Score } else { 0 }
        $startValue = if ($settings.Start) { [int]$settings.Start } else { 0 }
        $scoreNote = if ($scoreValue -ge 10000) { 'Mục tiêu rất cao, yêu cầu duy trì kiểm soát lâu.' } elseif ($scoreValue -ge 5000) { 'Mục tiêu cao; sai sót tích lũy ảnh hưởng rõ.' } elseif ($scoreValue -ge 3000) { 'Mục tiêu trung bình.' } else { 'Mục tiêu ngắn, thiên về vượt màn nhanh.' }
        $startNote = if ($startValue -ge 80) { 'Áp lực đầu màn rất cao.' } elseif ($startValue -ge 65) { 'Áp lực đầu màn cao.' } elseif ($startValue -ge 50) { 'Lượng bi xuất phát trung bình.' } else { 'Lượng bi xuất phát thấp.' }
        $lengthNote = if (-not $routeLengths.Count) { 'File CURV dùng định dạng generator tùy biến; chưa thể tính độ dài đáng tin cậy.' } elseif ($averageLength -ge 2600) { 'Đường rất dài.' } elseif ($averageLength -ge 1800) { 'Đường dài.' } elseif ($averageLength -ge 1100) { 'Đường trung bình.' } else { 'Đường tương đối ngắn.' }
        $features = @()
        if ($graphics.Tracks -eq 1) { $features += 'Một tuyến' }
        elseif ($graphics.Tracks -eq 2) { $features += 'Hai tuyến' }
        else { $features += "$($graphics.Tracks) tuyến" }
        if ($graphics.HasTunnel) { $features += 'Có tunnel che đường' }
        if ($graphics.HasSprite) { $features += 'Có lớp sprite đặc biệt' }
        if ($graphics.Treasure -gt 0) { $features += "$($graphics.Treasure) TreasurePoint" }
        if ($graphics.Space) { $features += 'Chế độ Space' }
        if ($graphics.Danger) { $features += "danger=$($graphics.Danger)" }
        if ($settings.Single) { $features += "single=$($settings.Single)" }
        if ($settingId -eq 'bonus') { $features += 'Bonus level' }

        $levels += [pscustomobject]@{
            Number = "$stageNumber-$($index + 1)"
            Stage = $stageNumber
            Id = $mapId
            Name = $name
            SettingId = $settingId
            Progression = $progression
            GauntletSettings = $gauntletSettings
            IsBonus = $settingId -eq 'bonus'
            Graphics = $graphics
            Settings = $settings
            Image = $image
            Sight = $sight
            Factors = $null
            Pressure = 0
            Parts = @()
            Difficulty = ''
            Note = ''
            Features = $features
            Routes = $routes
            RouteLengths = $routeLengths
            TotalLength = $totalLength
            AverageLength = $averageLength
            LengthNote = $lengthNote
            Asymmetry = $asymmetry
            BalanceLabel = $balanceLabel
            BalanceNote = $balanceNote
            Overlap = $overlap
            ScoreNote = $scoreNote
            StartNote = $startNote
        }
    }
    $stages += [pscustomobject]@{ Number = $stageNumber; Levels = $levels }
}

# routes the CURV reader cannot measure get the median length, so pace is not inflated
$allLevels = @($stages | ForEach-Object { $_.Levels })
$knownLengths = @($allLevels | Where-Object { $_.AverageLength -gt 0 } | ForEach-Object { [double]$_.AverageLength } | Sort-Object)
$medianLength = $knownLengths[[int][Math]::Floor($knownLengths.Count / 2)]
# maps whose sight cannot be measured get the median as well
$knownSight = @($allLevels | Where-Object Sight | ForEach-Object { 0.4 * $_.Sight.Percent + 0.6 * $_.Sight.EndPercent } | Sort-Object)
$medianSight = $knownSight[[int][Math]::Floor($knownSight.Count / 2)]
foreach ($level in $allLevels) {
    $length = if ($level.AverageLength) { $level.AverageLength } else { $medianLength }
    # the stretch near the hole is where blocked shots kill you
    $sightPercent = if ($level.Sight) { 0.4 * $level.Sight.Percent + 0.6 * $level.Sight.EndPercent } else { $medianSight }
    $level.Factors = Get-DifficultyFactors $level.Graphics $level.Settings $length $level.Overlap.Percent $sightPercent
}
# bonus levels (e.g. one colour, every shot pops) are not comparable: keep them out of the ranking
Set-Difficulty @($allLevels | Where-Object { -not $_.IsBonus })
foreach ($level in $allLevels | Where-Object IsBonus) {
    $level.Difficulty = 'Bonus'
    $level.Pressure = '—'
    $level.Note = "Màn thưởng ($($level.Settings.Colors) màu, speed $($level.Settings.Speed)); không xếp hạng cùng các màn thường."
}

$totalLevels = ($stages | ForEach-Object { $_.Levels.Count } | Measure-Object -Sum).Sum
$totalBonus = ($stages.Levels | Where-Object IsBonus).Count
$totalTracks1 = ($stages.Levels | Where-Object { $_.Graphics.Tracks -eq 1 }).Count
$totalTracks2 = ($stages.Levels | Where-Object { $_.Graphics.Tracks -eq 2 }).Count
$totalTracks3 = ($stages.Levels | Where-Object { $_.Graphics.Tracks -ge 3 }).Count
$missingImages = @($stages.Levels | Where-Object { -not $_.Image })

function Scale([double]$value, [double]$minimum, [double]$maximum, [switch]$Inverse) {
    if ($maximum -le $minimum) { return 0.5 }
    $scaled = ($value - $minimum) / ($maximum - $minimum)
    if ($Inverse) { return 1.0 - $scaled }
    return $scaled
}

$allLevels = @($stages | ForEach-Object { $_.Levels })
$gauntletGroups = @()
foreach ($trackGroup in 1, 2, 3) {
    $group = @($allLevels | Where-Object { if ($trackGroup -eq 3) { $_.Graphics.Tracks -ge 3 } else { $_.Graphics.Tracks -eq $trackGroup } })
    $knownLengths = @($group | Where-Object { $_.RouteLengths.Count } | ForEach-Object { ($_.RouteLengths | Measure-Object -Minimum).Minimum })
    $medianLength = if ($knownLengths.Count) { ($knownLengths | Sort-Object)[[Math]::Floor($knownLengths.Count / 2)] } else { 1500 }
    $lengthMin = if ($knownLengths.Count) { ($knownLengths | Measure-Object -Minimum).Minimum } else { 500 }
    $lengthMax = if ($knownLengths.Count) { ($knownLengths | Measure-Object -Maximum).Maximum } else { 3000 }
    $speeds = @($group | ForEach-Object { [double]::Parse($_.GauntletSettings.Speed, [Globalization.CultureInfo]::InvariantCulture) })
    $starts = @($group | ForEach-Object { [double]$_.GauntletSettings.Start })
    $scores = @($group | ForEach-Object { [double]$_.GauntletSettings.Score })
    $speedMin = ($speeds | Measure-Object -Minimum).Minimum; $speedMax = ($speeds | Measure-Object -Maximum).Maximum
    $startMin = ($starts | Measure-Object -Minimum).Minimum; $startMax = ($starts | Measure-Object -Maximum).Maximum
    $scoreMin = ($scores | Measure-Object -Minimum).Minimum; $scoreMax = ($scores | Measure-Object -Maximum).Maximum
    $sights = @($group | ForEach-Object { $_.Factors.Sight })
    $sightMin = ($sights | Measure-Object -Minimum).Minimum; $sightMax = ($sights | Measure-Object -Maximum).Maximum
    foreach ($level in $group) {
        $speed = [double]::Parse($level.GauntletSettings.Speed, [Globalization.CultureInfo]::InvariantCulture)
        $start = [double]$level.GauntletSettings.Start
        $score = [double]$level.GauntletSettings.Score
        $shortest = if ($level.RouteLengths.Count) { ($level.RouteLengths | Measure-Object -Minimum).Minimum } else { $medianLength }
        $risk = 0.75 * (30 * (Scale $speed $speedMin $speedMax) +
                12 * (Scale $start $startMin $startMax) +
                15 * (Scale $score $scoreMin $scoreMax) +
                25 * (Scale $shortest $lengthMin $lengthMax -Inverse) +
                8 * ([Math]::Min(100, $level.Overlap.Percent) / 100) +
                7 * ([Math]::Min(100, $level.Asymmetry) / 100) +
                $(if ($level.Graphics.HasTunnel) { 5 } else { 0 }) -
                2 * [Math]::Min(1, $level.Graphics.Treasure / 5)) +
                25 * (Scale $level.Factors.Sight $sightMin $sightMax)
        Add-Member -InputObject $level -NotePropertyName GauntletRisk -NotePropertyValue ([Math]::Round($risk, 1))
        Add-Member -InputObject $level -NotePropertyName GauntletShortest -NotePropertyValue $shortest
    }
    $ranked = @($group | Sort-Object GauntletRisk -Descending)
    for ($rank = 0; $rank -lt $ranked.Count; $rank++) {
        Add-Member -InputObject $ranked[$rank] -NotePropertyName GauntletRank -NotePropertyValue ($rank + 1)
        $tier = if ($rank -eq 0) { 'Khó nhất' } elseif ($rank -lt [Math]::Ceiling($ranked.Count * 0.2)) { 'Nhóm khó' } elseif ($rank -ge [Math]::Floor($ranked.Count * 0.8)) { 'Nhóm dễ' } else { 'Trung bình' }
        Add-Member -InputObject $ranked[$rank] -NotePropertyName GauntletTier -NotePropertyValue $tier
    }
    $gauntletGroups += [pscustomobject]@{ Tracks = $trackGroup; Ranked = $ranked }
}

# card wording: top third of the set = heavily blocked, bottom third = open
$sightHigh = $knownSight[[int][Math]::Floor($knownSight.Count * 2 / 3)]
$sightLow = $knownSight[[int][Math]::Floor($knownSight.Count / 3)]
$builder = [Text.StringBuilder]::new()
[void]$builder.AppendLine(@'
<!doctype html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Zuma Deluxe Plus — Catalog 145 Level</title>
<style>
:root{color-scheme:dark;--bg:#0d1117;--panel:#161b22;--panel2:#1f2630;--line:#30363d;--text:#e6edf3;--muted:#9da7b3;--gold:#f2c14e;--cyan:#62d9ff;--green:#64d98b;--red:#ff7b72}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at top,#182335 0,#0d1117 38rem);color:var(--text);font-family:Segoe UI,Arial,sans-serif}header{padding:36px 24px 22px;max-width:1500px;margin:auto}h1{margin:0 0 8px;font-size:clamp(28px,5vw,48px)}.lead{color:var(--muted);max-width:1000px;line-height:1.6}.stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:10px;margin:22px 0}.stat{background:#121923;border:1px solid var(--line);border-radius:14px;padding:14px}.stat b{display:block;color:var(--gold);font-size:24px}.method{background:#111923;border-left:3px solid var(--cyan);border-radius:0 12px 12px 0;padding:12px 15px;color:#c7d2df;line-height:1.5}.controls{position:sticky;top:0;z-index:5;background:rgba(13,17,23,.92);backdrop-filter:blur(12px);border-block:1px solid var(--line);padding:12px 24px}.controls-inner{max-width:1500px;margin:auto;display:flex;gap:10px;flex-wrap:wrap}.controls input,.controls select{background:var(--panel);color:var(--text);border:1px solid var(--line);border-radius:10px;padding:10px 12px;font-size:15px}.controls input{flex:1;min-width:240px}.wrap{max-width:1500px;margin:auto;padding:22px 24px 60px}.stage{margin:0 0 28px}.stage-head{display:flex;align-items:end;justify-content:space-between;border-bottom:1px solid var(--line);padding:10px 0;margin-bottom:14px}.stage-head h2{margin:0;font-size:28px}.stage-head span{color:var(--muted)}.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(320px,1fr));gap:16px}.card{background:linear-gradient(180deg,var(--panel2),var(--panel));border:1px solid var(--line);border-radius:16px;overflow:hidden;box-shadow:0 9px 24px rgba(0,0,0,.2)}.visual{aspect-ratio:4/3;background:#080b10;display:grid;place-items:center;overflow:hidden;position:relative}.visual img{width:100%;height:100%;object-fit:cover}.placeholder{color:var(--muted);text-align:center;padding:20px}.level-no{position:absolute;top:10px;left:10px;background:rgba(0,0,0,.78);color:#fff;border:1px solid rgba(255,255,255,.2);border-radius:8px;padding:5px 8px;font-weight:700}.bonus{position:absolute;top:10px;right:10px;background:#785d00;color:#fff1b3;border-radius:8px;padding:5px 8px;font-weight:700}.body{padding:15px}.body h3{margin:0 0 4px;font-size:20px}.map-id{color:var(--cyan);font-family:Consolas,monospace;font-size:13px}.metrics{display:grid;grid-template-columns:repeat(3,1fr);gap:7px;margin:13px 0}.metric{background:#10161e;border:1px solid #28313d;border-radius:9px;padding:8px}.metric small{display:block;color:var(--muted);font-size:11px}.metric b{font-size:15px}.assessment{display:grid;gap:7px;margin:12px 0}.assessment-row{background:#111820;border:1px solid #27313d;border-radius:9px;padding:8px 10px;color:#cbd5df;line-height:1.35}.assessment-row strong{color:var(--cyan)}.assessment-row small{display:block;color:var(--muted);margin-top:2px}.tags{display:flex;flex-wrap:wrap;gap:6px;margin:10px 0}.tag{font-size:12px;background:#263243;color:#dce8f8;border-radius:999px;padding:5px 8px}.difficulty{color:var(--gold);font-weight:700}.note{color:#c8d1dc;line-height:1.45;margin:8px 0 0}.g35{background:#101720;border:1px solid var(--line);border-radius:16px;padding:18px;margin-bottom:28px}.g35 h2{margin:0 0 8px}.g35-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:12px}.g35-group{background:#161e28;border:1px solid #2b3644;border-radius:12px;padding:14px}.g35-group h3{margin:0 0 10px;color:var(--gold)}.rank-list{margin:0;padding-left:22px;line-height:1.6}.rank-list code{color:var(--cyan)}.warning{color:#ffd7a3}.g35-badge{margin-top:10px;background:#24202d;border:1px solid #493c5c;border-radius:10px;padding:9px;color:#e5d8fa}.hidden{display:none!important}footer{color:var(--muted);text-align:center;padding:28px;border-top:1px solid var(--line)}
</style>
</head>
<body>
'@)

[void]$builder.AppendLine('<header>')
[void]$builder.AppendLine('<h1>Zuma Deluxe Plus — 145 Level</h1>')
[void]$builder.AppendLine('<p class="lead">Catalog toàn bộ Adventure từ 1-1 đến 13-3, chia theo Stage. Mỗi thẻ sử dụng trực tiếp asset trong game và đối chiếu với <code>levels/levels.xml</code>.</p>')
[void]$builder.AppendLine('<div class="stats">')
[void]$builder.AppendLine("<div class='stat'><b>$totalLevels</b><span>level Adventure</span></div>")
[void]$builder.AppendLine("<div class='stat'><b>13</b><span>Stage</span></div>")
[void]$builder.AppendLine("<div class='stat'><b>$totalBonus</b><span>bonus level</span></div>")
[void]$builder.AppendLine("<div class='stat'><b>$totalTracks1 / $totalTracks2 / $totalTracks3</b><span>map 1 / 2 / 3+ tuyến</span></div>")
$factorText = ($difficultyFactors | ForEach-Object { "$($_.Label) $([Math]::Round($_.Weight * 100))%" }) -join ' · '
$gradeText = ($difficultyGrades | ForEach-Object { "$($_.Name) $([Math]::Round($_.Share * 100))%" }) -join ' · '
[void]$builder.AppendLine("</div><div class='method'><strong>Độ khó:</strong> mỗi yếu tố được xếp hạng so với $(@($allLevels | Where-Object { -not $_.IsBonus }).Count) level thường (màn Bonus đứng riêng) (0 = dễ nhất, 1 = khó nhất) rồi cộng theo trọng số thành điểm áp lực 0–100: $(Html $factorText). Mức độ khó chia theo thứ hạng điểm áp lực: $(Html $gradeText).</div>")
[void]$builder.AppendLine('<div class="method"><strong>Cách đo đường:</strong> độ dài đường được ước tính từ các điểm điều khiển trong file CURV <code>.dat</code>. “Bất cân xứng” so sánh độ dài giữa các tuyến. “Chồng/che tuyến” đo tỷ lệ hình học hai tuyến chạy cách nhau không quá 28 px; đây là chỉ báo nguy cơ bi che lẫn nhau hoặc khiến người chơi bắn nhầm, không phải thứ tự lớp vẽ tuyệt đối.</div></header>')
[void]$builder.AppendLine('<div class="controls"><div class="controls-inner"><input id="search" placeholder="Tìm level, tên hoặc map ID…"><select id="stageFilter"><option value="all">Tất cả Stage</option>')
foreach ($stage in $stages) { [void]$builder.AppendLine("<option value='$($stage.Number)'>Stage $($stage.Number) — $($stage.Levels.Count) level</option>") }
[void]$builder.AppendLine('</select><select id="trackFilter"><option value="all">Mọi số tuyến</option><option value="1">1 tuyến</option><option value="2">2 tuyến</option><option value="3">3+ tuyến</option></select></div></div>')
[void]$builder.AppendLine('<main class="wrap">')
[void]$builder.AppendLine('<section class="g35"><h2>Gauntlet cấp 35 — xếp hạng theo số tuyến</h2>')
[void]$builder.AppendLine('<p class="warning"><strong>Lưu ý cấu hình:</strong> các setting s35/d35/t35 có tồn tại, nhưng LevelProgression hiện chỉ liên kết đến cấp 30. Bảng này dùng trực tiếp thông số cấp 35 đã định nghĩa để mô phỏng đúng ý đồ max level.</p><div class="g35-grid">')
foreach ($gauntletGroup in $gauntletGroups) {
    $label = if ($gauntletGroup.Tracks -eq 3) { '3 tuyến' } else { "$($gauntletGroup.Tracks) tuyến" }
    $hardest = $gauntletGroup.Ranked | Select-Object -First 5
    $easiest = $gauntletGroup.Ranked | Select-Object -Last 5 | Sort-Object GauntletRisk
    [void]$builder.AppendLine("<div class='g35-group'><h3>$label — $($gauntletGroup.Ranked.Count) map</h3><strong>Khó nhất</strong><ol class='rank-list'>")
    foreach ($item in $hardest) { [void]$builder.AppendLine("<li>$($item.Number) — $(Html $item.Name) <code>$($item.GauntletRisk)</code></li>") }
    [void]$builder.AppendLine('</ol><strong>Dễ nhất</strong><ol class="rank-list">')
    foreach ($item in $easiest) { [void]$builder.AppendLine("<li>$($item.Number) — $(Html $item.Name) <code>$($item.GauntletRisk)</code></li>") }
    [void]$builder.AppendLine('</ol></div>')
}
[void]$builder.AppendLine('</div><p>Điểm rủi ro Gauntlet 35 kết hợp speed, start, điểm Zuma, tuyến ngắn nhất, tunnel, bất cân xứng, chồng tuyến và TreasurePoint. Điểm cao hơn nghĩa là khó hơn trong cùng nhóm số tuyến.</p></section>')

foreach ($stage in $stages) {
    [void]$builder.AppendLine("<section class='stage' data-stage='$($stage.Number)'>")
    [void]$builder.AppendLine("<div class='stage-head'><h2>Stage $($stage.Number)</h2><span>$($stage.Levels.Count) level</span></div><div class='grid'>")
    foreach ($level in $stage.Levels) {
        $trackFilter = if ($level.Graphics.Tracks -ge 3) { 3 } else { $level.Graphics.Tracks }
        $searchText = Html("$($level.Number) $($level.Name) $($level.Id)").ToLowerInvariant()
        [void]$builder.AppendLine("<article class='card' data-search='$searchText' data-tracks='$trackFilter'>")
        [void]$builder.AppendLine("<div class='visual'><span class='level-no'>$($level.Number)</span>")
        if ($level.IsBonus) { [void]$builder.AppendLine("<span class='bonus'>BONUS</span>") }
        if ($level.Image) {
            [void]$builder.AppendLine("<img loading='lazy' src='$($level.Image)' alt='$(Html $level.Name)'>")
        } else {
            [void]$builder.AppendLine("<div class='placeholder'>Không có ảnh nền riêng<br><small>Map dùng chế độ đặc biệt</small></div>")
        }
        [void]$builder.AppendLine('</div><div class="body">')
        [void]$builder.AppendLine("<h3>$(Html $level.Name)</h3><div class='map-id'>$($level.Id) · settings: $($level.SettingId)</div>")
        $levelTrackGroup = if ($level.Graphics.Tracks -ge 3) { 3 } else { $level.Graphics.Tracks }
        $levelGroupCount = ($gauntletGroups | Where-Object Tracks -eq $levelTrackGroup | Select-Object -First 1).Ranked.Count
        [void]$builder.AppendLine("<div class='g35-badge'><strong>Gauntlet 35: #$($level.GauntletRank) / $levelGroupCount trong nhóm $($level.Graphics.Tracks) tuyến</strong><br>$(Html $level.GauntletTier) · risk $($level.GauntletRisk) · $($level.Progression)35: speed $($level.GauntletSettings.Speed), start $($level.GauntletSettings.Start), Zuma $($level.GauntletSettings.Score)</div>")
        [void]$builder.AppendLine('<div class="metrics">')
        [void]$builder.AppendLine("<div class='metric'><small>Tuyến</small><b>$($level.Graphics.Tracks)</b></div>")
        [void]$builder.AppendLine("<div class='metric'><small>Speed</small><b>$($level.Settings.Speed)</b></div>")
        [void]$builder.AppendLine("<div class='metric'><small>Start</small><b>$($level.Settings.Start)</b></div>")
        [void]$builder.AppendLine("<div class='metric'><small>Zuma</small><b>$($level.Settings.Score)</b></div>")
        [void]$builder.AppendLine("<div class='metric'><small>Màu</small><b>$($level.Settings.Colors)</b></div>")
        [void]$builder.AppendLine("<div class='metric'><small>Ace Time</small><b>$($level.Settings.ParTime)s</b></div>")
        [void]$builder.AppendLine("<div class='metric'><small>Dài trung bình</small><b>$($level.AverageLength) px</b></div>")
        [void]$builder.AppendLine("<div class='metric'><small>Cân xứng</small><b>$($level.BalanceLabel)</b></div>")
        [void]$builder.AppendLine("<div class='metric'><small>Chồng tuyến</small><b>$($level.Overlap.Label)</b></div>")
        $routeText = if ($level.RouteLengths.Count) { (($level.RouteLengths | ForEach-Object { "$_ px" }) -join ' · ') } else { 'Không đọc được dữ liệu CURV' }
        [void]$builder.AppendLine('</div><div class="assessment">')
        [void]$builder.AppendLine("<div class='assessment-row'><strong>Điểm Zuma: $($level.Settings.Score)</strong><small>$(Html $level.ScoreNote)</small></div>")
        [void]$builder.AppendLine("<div class='assessment-row'><strong>Xuất phát: $($level.Settings.Start)</strong><small>$(Html $level.StartNote)</small></div>")
        [void]$builder.AppendLine("<div class='assessment-row'><strong>Độ dài từng đường: $routeText</strong><small>$(Html $level.LengthNote) Tổng độ dài: $($level.TotalLength) px.</small></div>")
        [void]$builder.AppendLine("<div class='assessment-row'><strong>Bất cân xứng: $($level.BalanceLabel)$(if($level.Asymmetry){" — $($level.Asymmetry)%"})</strong><small>$(Html $level.BalanceNote)</small></div>")
        $sightText = if ($level.Sight) { "$($level.Sight.Percent)% (đoạn cuối tuyến $($level.Sight.WorstRoute): $($level.Sight.EndPercent)%)" } else { 'Chưa đo được' }
        $sightNote = if (-not $level.Sight) { 'Không đọc được dữ liệu đường để mô phỏng tia bắn.' } elseif ($level.Factors.Sight -ge $sightHigh) { 'Phần lớn đường, nhất là đoạn gần lỗ, bị tuyến khác hoặc tunnel chắn trước ếch.' } elseif ($level.Factors.Sight -ge $sightLow) { 'Mức che trung bình so với các map khác; một số đoạn phải bắn vòng hoặc chờ thời điểm.' } else { 'Đường bắn thoáng hơn phần lớn các map, nhất là đoạn gần lỗ.' }
        [void]$builder.AppendLine("<div class='assessment-row'><strong>Đường bắn bị che: $sightText</strong><small>$(Html $sightNote)</small></div>")
        [void]$builder.AppendLine("<div class='assessment-row'><strong>Nguy cơ một đường che/chồng đường khác: $($level.Overlap.Label) — $($level.Overlap.Percent)%</strong><small>$(Html $level.Overlap.Note)</small></div>")
        [void]$builder.AppendLine('</div><div class="tags">')
        foreach ($feature in $level.Features) { [void]$builder.AppendLine("<span class='tag'>$(Html $feature)</span>") }
        [void]$builder.AppendLine("</div><div class='difficulty'>$($level.Difficulty) · $($level.Pressure)/100</div><p class='note'>$(Html $level.Note)</p></div></article>")
    }
    [void]$builder.AppendLine('</div></section>')
}

[void]$builder.AppendLine(@'
</main>
<footer>Dữ liệu được tạo từ levels/levels.xml và asset map hiện tại của Zuma Deluxe Plus.</footer>
<script>
const search=document.querySelector('#search'),stageFilter=document.querySelector('#stageFilter'),trackFilter=document.querySelector('#trackFilter');
function filter(){const q=search.value.trim().toLowerCase(),stage=stageFilter.value,tracks=trackFilter.value;document.querySelectorAll('.stage').forEach(section=>{let visible=0;section.querySelectorAll('.card').forEach(card=>{const ok=(!q||card.dataset.search.includes(q))&&(tracks==='all'||card.dataset.tracks===tracks);card.classList.toggle('hidden',!ok);if(ok)visible++});section.classList.toggle('hidden',(stage!=='all'&&section.dataset.stage!==stage)||visible===0)});}
[search,stageFilter,trackFilter].forEach(element=>element.addEventListener('input',filter));
</script>
</body>
</html>
'@)

[IO.File]::WriteAllText($outputPath, $builder.ToString(), [Text.UTF8Encoding]::new($false))
Write-Output "Created: $outputPath"
Write-Output "Stages: $($stages.Count); Levels: $totalLevels; Missing images: $($missingImages.Count)"
if ($missingImages.Count) { Write-Output ('No image: ' + (($missingImages | ForEach-Object Number) -join ', ')) }
