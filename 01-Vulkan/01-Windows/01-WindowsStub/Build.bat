cls

del *.exe *.res *.obj

cl.exe /c /EHsc VK.c

rc.exe VK.rc

link.exe VK.obj VK.res User32.lib GDI32.lib /SUBSYSTEM:WINDOWS
