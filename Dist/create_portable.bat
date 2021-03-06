@echo off
set zipcmd="C:/Program Files/7-Zip/7z.exe"

For /F "tokens=2,3 delims= " %%i In (..\Source\VersionInfo.h) Do (set %%i=%%~j)
echo Creating distribution archive for Image Uploader version %_APP_VER% %BUILD%

set temp_dir=portable\temp
set filename=image-uploader-%_APP_VER%-build-%BUILD%-portable.7z
echo %temp_dir%

rmdir /q /s  %temp_dir%
del /q output\%filename%
 
mkdir %temp_dir%
mkdir %temp_dir%\Lang
mkdir %temp_dir%\Data
mkdir %temp_dir%\Modules
mkdir %temp_dir%\Data\Thumbnails\
mkdir %temp_dir%\Data\Favicons
mkdir %temp_dir%\Data\Scripts
mkdir %temp_dir%\Data\Scripts\Lang
mkdir %temp_dir%\Data\Scripts\Utils
mkdir %temp_dir%\Data\Servers
mkdir %temp_dir%\Data\Update

call signcode.bat
Copy "..\Build\release optimized\Image Uploader.exe" %temp_dir%\
Copy "..\Build\release optimized\curl-ca-bundle.crt" %temp_dir%\
Copy "..\Source\res\new-icon.ico" %temp_dir%\
Copy "..\Lang\*.lng" %temp_dir%\Lang\
Copy "..\Build\release optimized\Modules\*" %temp_dir%\Modules\
xcopy "..\Docs" %temp_dir%\Docs\ /s /e /y /i
Copy "..\Data\servers.xml" %temp_dir%\Data\
Copy "..\Data\templates.xml" %temp_dir%\Data\
Copy "..\Data\template.txt" %temp_dir%\Data\
Copy "..\Data\Favicons\*.ico" %temp_dir%\Data\Favicons\
Copy "..\Data\Scripts\*.nut" %temp_dir%\Data\Scripts\
Copy "..\Data\Scripts\Lang\*.json" %temp_dir%\Data\Scripts\Lang\
Copy "..\Data\Scripts\Utils\*.json" %temp_dir%\Data\Scripts\Utils\
Copy "..\Data\Update\iu_core.xml" %temp_dir%\Data\Update\
Copy "..\Data\Update\iu_serversinfo.xml" %temp_dir%\Data\Update\
Copy "..\Data\Update\iu_ffmpeg.xml" %temp_dir%\Data\Update\
Copy "..\Data\Thumbnails\*.*" %temp_dir%\Data\Thumbnails\
rem Copy "..\Data\Servers\*.xml" %temp_dir%\Data\Servers\
Copy "..\Build\release optimized\ExplorerIntegration.dll" %temp_dir%\
Copy "..\Build\release optimized\ExplorerIntegration64.dll" %temp_dir%\
Copy "..\Build\release\av*.dll" %temp_dir%\
Copy "..\Build\release\sw*.dll" %temp_dir%\
Copy "Dll\gdiplus.dll" %temp_dir%\
del "%temp_dir%\Lang\default.lng"
rem signtool sign  /t http://timestamp.digicert.com /f "d:\Backups\ImageUploader\3315593d7023a0aeb48042349dc4fd40.pem" "%temp_dir%\Image Uploader.exe" "%temp_dir%\ExplorerIntegration.dll" "%temp_dir%\ExplorerIntegration64.dll"

cd %temp_dir%
%zipcmd% a -mx9 ..\..\output\image-uploader-%_APP_VER%-build-%BUILD%-portable.7z "*"
cd ..\..\

rem rmdir /q /s  %temp_dir%

