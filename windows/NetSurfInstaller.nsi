; install script for nullsoft msi installer creation
; debian package name nsis
; paths may need adapting according to local settings
; for release, would really need to compile source tree too
Page license
Page directory
Page instfiles
; install directory $INSTDIR = $PROGRAMFILES\NetSurf
; install resources $PROGRAMFILES\NetSurf\res
; install dlls $SYSDIR=%SystemRoot%\System[32]
SetCompressor lzma
InstallDir "$PROGRAMFILES\NetSurf"
LicenseData ../COPYING  ; \n -> \r\n
Name NetSurf
OutFile NetSurfInstall.exe
Icon ../windows/res/NetSurf32.ico
XPStyle on
Section
  SetShellVarContext all
  SetOutPath $SYSDIR
  File /usr/local/mingw/bin/libeay32.dll
  File /usr/local/mingw/bin/libcurl-4.dll
  File /usr/local/mingw/bin/libiconv-2.dll
  File /usr/local/mingw/bin/ssleay32.dll
  File /usr/local/mingw/bin/libgnurx-0.dll
  File /usr/local/mingw/bin/libxml2-2.dll
  File /usr/local/mingw/bin/libpng12.dll
  File /usr/local/mingw/bin/libjpeg.dll
  IfFileExists "$INSTDIR\*.*" +2
    CreateDirectory "$INSTDIR"
  SetOutPath $INSTDIR
  File ../NetSurf.exe
  IfFileExists "$INSTDIR\res\*.*" +2
    CreateDirectory "$INSTDIR\res"
  SetOutPath $INSTDIR\res
  File ../windows/res/Aliases
  File ../windows/res/default.css
  File ../windows/res/quirks.css
  File ../windows/res/messages
  File ../windows/res/preferences
  File ../windows/res/*.bmp
  File ../windows/res/*.ico
  File ../windows/res/throbber.avi
  IfFileExists $SMPROGRAMS\NetSurf\NetSurf.lnk +2
    CreateDirectory "$SMPROGRAMS\NetSurf"
  CreateShortCut "$SMPROGRAMS\NetSurf\NetSurf.lnk" "$INSTDIR\NetSurf.exe" "" "$INSTDIR\res\NetSurf32.ico"
  IfFileExists "$INSTDIR\src\*.*" +2
    CreateDirectory "$INSTDIR\src"
  SetOutPath "$INSTDIR\src"
  File ../Makefile
  File ../Makefile.config
  File ../Makefile.config.example
  File ../Makefile.defaults
  File ../Makefile.sources
  File ../Makefile.resources
  IfFileExists "$INSTDIR\src\content\*.*" +2
    CreateDirectory "$INSTDIR\src\content"
  SetOutPath "$INSTDIR\src\content"
  File /r /x .svn ../content/*.c
  File /r /x .svn ../content/*.h
  IfFileExists "$INSTDIR\src\css\*.*" +2
    CreateDirectory "$INSTDIR\src\css"
  SetOutPath "$INSTDIR\src\css"
  File /r /x .svn ../css/*.c
  File /r /x .svn ../css/*.h
  IfFileExists "$INSTDIR\src\desktop\*.*" +2
    CreateDirectory "$INSTDIR\src\desktop"
  SetOutPath "$INSTDIR\src\desktop"
  File /r /x .svn ../desktop/*.c
  File /r /x .svn ../desktop/*.h
  IfFileExists "$INSTDIR\src\image\*.*" +2
    CreateDirectory "$INSTDIR\src\image"
  SetOutPath "$INSTDIR\src\image"
  File /r /x .svn ../image/*.c
  File /r /x .svn ../image/*.h
  IfFileExists "$INSTDIR\src\render\*.*" +2
    CreateDirectory "$INSTDIR\src\render"
  SetOutPath "$INSTDIR\src\render"
  File /r /x .svn ../render/*.c
  File /r /x .svn ../render/*.h
  IfFileExists "$INSTDIR\src\utils\*.*" +2
    CreateDirectory "$INSTDIR\src\utils"
  SetOutPath "$INSTDIR\src\utils"
  File /r /x .svn ../utils/*.c
  File /r /x .svn ../utils/*.h
  IfFileExists "$INSTDIR\src\windows\*.*" +2
    CreateDirectory "$INSTDIR\src\windows"
  SetOutPath "$INSTDIR\src\windows"
  File /r /x .svn ../windows/*.c
  File /r /x .svn ../windows/*.h
  IfFileExists "$INSTDIR\src\Docs\*.*" +2
    CreateDirectory "$INSTDIR\src\Docs"
  SetOutPath "$INSTDIR\src\Docs"
  File /r /x .svn ../Docs/*.*
SectionEnd
