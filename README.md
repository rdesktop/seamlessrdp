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
autotools-based build system.


## Building from a source archive (seamlessrdp-1.0.tar.gz)

    cd seamlessrdp-1.0
    ./configure
    make


## Building from a git checkout

    cd ServerExe
    ./autogen.sh
    ./configure
    make


## Cross-compiling from Linux

With a cross-compiling environment for Windows installed, tell
configure that you want to build for a Windows platform by running
`./configure` with `--host` set to a suitable triplet for your
cross-compiling setup. Examples:

    ./configure --host=i686-pc-mingw32  # for 32-bit Windows
    ./configure --host=x86_64-w64-mingw32  # for 64-bit Windows


# Installing

After compiling with `make`, you can create a zip file with the
required contents.

    zip -j seamlessrdp.zip .libs/seamlessrdpshell.exe .libs/seamlessrdp??.dll .libs/seamlessrdphook??.exe

This creates a `seamlessrdp.zip` file that can be transfered onto to
your Windows server and unpacked to any location you'd like, such as
`C:\SeamlessRDP\`.


# Example usage

Starting notepad.exe via SeamlessRDP using rdesktop:

    rdesktop -A 'C:\SeamlessRDP\seamlessrdpshell.exe' -s 'notepad.exe'
