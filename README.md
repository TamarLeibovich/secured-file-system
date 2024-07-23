# Secured File System - How To Use?

## Pre Set Up:
> **Note:** I must suggest you to run it on a virtual machine only, due to end of support.

*This minifilter driver will only work on Windows 10/11, version 22H2 only, due to specific offsets & functions.*

You first need to configure your system to install drivers properly. Follow these steps:
- disable secure boot following [this guide](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/disabling-secure-boot?view=windows-11)
- turn on testsigning (make sure cmd has admin privileges): 
```commandline
bcdedit /set testsigning on
```

You need to reboot your machine after this process in order for the changes to take effect.

Moreover, you must install python 3.12.x in the directory ```C:\Python\``` from [python's official site](https://www.python.org/downloads/).

## Setting up the system:
### installation:

first, clone the repository in the `C:` directory by running the following:

```commandline
git clone https://github.com/TamarLeibovich/secured-file-system.git
```

in an administrative cmd, run the following:

```commandline
C:\secured-file-system\installation.bat
```

this will install everything automatically, so you can start using the system freely and securely. 

## Short Explanation
this project is meant to secure your file system - each time the user clicks on a file in the file system via explorer.exe,
a password prompt will show up, and the correct password (which you have initialized at installation) must be inserted
in order for the file to open successfully.

> **Note:** password is required for file opening attempts, including at Desktop or Taskbar. For example, attempting to open
> Paint.exe or Task Manager via the Taskbar will trigger a password prompt. That's because explorer.exe is responsible for file
> operations at these locations.

## Demonstration

the project's demonstration video can be found on this repository - just download the mp4 video, or clone the repository.