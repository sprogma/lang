$FLAGS = @("-g", "-D_CRT_SECURE_NO_WARNINGS", "-fms-extensions", "-Wno-microsoft")
$FLAGS += @("-fsanitize=address")
$x = @(gci *.h | % LastWriteTime)
$f = (gci *.c)
$f | ? {($x + @(gi $_ | % LastWriteTime)) -ge ((gi "obj/$($_.Name).o" 2>$null) | % LastWriteTime)} | % { Write-Host "Building $($_.FullName)" -Foreground green ; clang $FLAGS -c ($_.FullName) -o "obj/$($_.Name).o" }
Write-Host "Linking..." -Foreground green
clang $FLAGS ($f | % Name | %{"obj/$_.o"}) -o a.exe
