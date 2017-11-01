# SeamlessRDP

SeamlessRDP is an extension to RDP servers that allows publishing
Windows applications from an RDP server to your local desktop, similar
to RAIL/RemoteApp.

SeamlessRDP requires Windows Server 2008r2 or later to operate
correctly.


# Contributing

The development of SeamlessRDP takes place on
[GitHub](https://github.com/rdesktop/seamlessrdp/). Feel free to get
involved! We welcome all contributions.


# Building SeamlessRDP

The ServerExe directory contains the server-side components of
SeamlessRDP: the SeamlessRDP shell and window hooks. It uses a
autotools-based build system. It is designed to support both 32 and
64bit applications and therefore building is a bit complicated.


## Build instructions (Linux)

You need to have both mingw32 and mingw64 installed to be able to
cross compile so that you can build both 32 and 64bit binaries for
Windows. To build 64bit version of SeamlessRDP, mingw32 is required to
be built due to that the 64bit version of SeamlessRDP can launch 32bit
applications.

Enter the ServerExe directory.

	cd ServerExe

Skip the following step to setup build environment if you have
downloaded a release tarball of SeamlessRDP.

	./autogen.sh

Start with building the 32bit binaries as following:

	./configure --host=i686-pc-mingw32
	make

This produces seamlessrdp32.dll, seamlesrdphook32.exe and 32bit
version of seamlessrdpshell.exe. If you only want a 32bit version of
SeamlessRDP, you are now done with the building and can continue with
the Installation step below.

If you want to build 64bit version, build the 64 binaries like this:

	./configure --host==x86_64-w64-mingw32
	make

This step produces seamlessrdp64.dll, seamlessrdphook64.exe and a
64bit version of seamlesrdpshell.exe, which completes the set of files
for 64bit version of SeamlessRDP.


# Installing

After building SeamlessRDP, you can create a zip file with the
required contents.

    zip -j seamlessrdp.zip .libs/seamlessrdpshell.exe .libs/seamlessrdp??.dll .libs/seamlessrdphook??.exe

This creates a `seamlessrdp.zip` file that can be transfered onto to
your Windows server and unpacked to any location you'd like, such as
`C:\SeamlessRDP\`.


# Example usage

Starting notepad.exe via SeamlessRDP using rdesktop:

    rdesktop -A 'C:\SeamlessRDP\seamlessrdpshell.exe' -s 'notepad.exe'
