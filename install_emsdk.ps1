$ErrorActionPreference = "Stop"
Write-Host "Cloning emsdk..."
git clone https://github.com/emscripten-core/emsdk.git C:\storage\fin_proj\emsdk
cd C:\storage\fin_proj\emsdk
Write-Host "Installing latest emsdk..."
.\emsdk.bat install latest
Write-Host "Activating latest emsdk..."
.\emsdk.bat activate latest
Write-Host "Done."
