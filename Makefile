#Nmake Makefile

CXX=cl
LD=link
CFLAGS=-nologo -DNODEBUG -DUNICODE -D_UNICODE -O1 -Oi -MT 
CXXFLAGS=-TP  -W4 -EHsc -Zc:forScope -Zc:wchar_t
LDFLAGS=/NOLOGO -OPT:REF  
LIBS=KERNEL32.lib   ADVAPI32.lib Shell32.lib USER32.lib GDI32.lib comctl32.lib Shlwapi.lib Secur32.lib



all:launcher.res launcher.obj
	$(LD) $(LDFLAGS) launcher.obj Launcher.res -OUT:launcher.exe $(LIBS)
	

clean:
	del /s /q *.res *.obj *.pdb *.exe >nul 2>nul
	
launcher.res:launcher.rc
	rc launcher.rc
	
launcher.obj:
	$(CXX) -c $(CFLAGS) $(CXXFLAGS) launcher.cpp
	
