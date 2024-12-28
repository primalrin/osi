gcc -shared -o mckusick_karels.dll mckusick_karels.c "-Wl,--out-implib,libmckusick_karels.a"
gcc -shared -o buddy.dll buddy.c "-Wl,--out-implib,libbuddy.a"
gcc main.c -o main.exe -L. -lmckusick_karels -lbuddy

.\main.exe mckusick_karels.dll
.\main.exe buddy.dll
.\main.exe

NtTrace D:\code\osi\lab4\main.exe mckusick_karels.dll > mckusick_karels.log
NtTrace D:\code\osi\lab4\main.exe buddy.dll > buddy.log
NtTrace D:\code\osi\lab4\main.exe > main.log