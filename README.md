VAToPA
==============

Introduction
-------------
VAToPA is a demo for x86/x64's paging memory management learning. I conver a virtual address from ring3 to physical address in ring0, after that, I have roughly understand the paging memory management!

In x86 architecture, PAE paging mode is widely used now, you can watch it as the following picture:
![pae paging pic](/pae.jpg)

And in x64 architecture, IA-32e paging mode is used, you can watch it as the following picture:
![ia-32e paging pic](/ia32.jpg)


Supported Platforms
--------------------
- x86 and x64 Windows 7


Reference Project(s)
--------------------
- `x86/x64体系探索及编程`

- VATOPA
 - https://github.com/slivermeteor/VATOPA/

After view his code, I learned from the function MmMapIoSpace to map physical address to a virtual address that I can visit in my driver.

- BlackBone
 - https://github.com/DarthTon/Blackbone/tree/master/src/BlackBoneDrv

Some macro definition i referenced from it such as `GetPxeAddress`, of course, I have made some changes. These macro can also find in wrk source code(`MmGetPhysicalAddress`).
