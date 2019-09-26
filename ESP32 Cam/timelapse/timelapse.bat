ECHO OFF
set /p fold=What folder are the photos in (no traling slash)?:
set /p of=Output filename?:
REM ECHO .\ffmpeg -framerate %fr% -i %fold%\%%d.jpg -c:v libx264 %of%.mp4
ffmpeg\bin\ffmpeg.exe -framerate 30 -i output\%fold%\%%d.jpg -c:v libx264 timelapse_output\%of%.mp4
ECHO Finished
PAUSE