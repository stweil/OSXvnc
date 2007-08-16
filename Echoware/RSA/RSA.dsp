# Microsoft Developer Studio Project File - Name="RSA" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=RSA - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "RSA.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "RSA.mak" CFG="RSA - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "RSA - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "RSA - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "RSA - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release6"
# PROP Intermediate_Dir "Release6"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\Release6\RSA.lib"

!ELSEIF  "$(CFG)" == "RSA - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "RSA___Win32_Debug"
# PROP BASE Intermediate_Dir "RSA___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug6"
# PROP Intermediate_Dir "Debug6"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\Debug6\RSA.lib"

!ENDIF 

# Begin Target

# Name "RSA - Win32 Release"
# Name "RSA - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\Code.cpp
# End Source File
# Begin Source File

SOURCE=.\Commonf.cpp
# End Source File
# Begin Source File

SOURCE=.\Gsimply.cpp
# End Source File
# Begin Source File

SOURCE=.\Hnfunct.cpp
# End Source File
# Begin Source File

SOURCE=.\Hnumber.cpp
# End Source File
# Begin Source File

SOURCE=.\Keys.cpp
# End Source File
# Begin Source File

SOURCE=.\Keys_main.cpp
# End Source File
# Begin Source File

SOURCE=.\Mynum.cpp
# End Source File
# Begin Source File

SOURCE=.\Myprint.cpp
# End Source File
# Begin Source File

SOURCE=.\Operator.cpp
# End Source File
# Begin Source File

SOURCE=.\PrimeGen.cpp
# End Source File
# Begin Source File

SOURCE=.\stdafx.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\Commonf.h
# End Source File
# Begin Source File

SOURCE=.\Function.h
# End Source File
# Begin Source File

SOURCE=.\Gsimply.h
# End Source File
# Begin Source File

SOURCE=.\Hnfunct.h
# End Source File
# Begin Source File

SOURCE=.\HNumber.h
# End Source File
# Begin Source File

SOURCE=.\MyNum.h
# End Source File
# Begin Source File

SOURCE=.\Myprint.h
# End Source File
# Begin Source File

SOURCE=.\MyTypes.h
# End Source File
# Begin Source File

SOURCE=.\Operator.h
# End Source File
# Begin Source File

SOURCE=.\Profiler.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\TMSG.H
# End Source File
# End Group
# End Target
# End Project
