NetSurf web browser
===================

![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/1037/badge)[*](https://bestpractices.coreinfrastructure.org/projects/1037)

# User Interface

Netsurf is divided into a series of frontends which provide a user
interface around common core functionality. Each frontend is a
distinct implementation for a specific GUI toolkit.

Because of this the user interface has different features in
each frontend allowing the browser to be a native application.

## Frontends

As GUI toolkits are often applicable to a single Operating
System (OS) some frontends are named for their OS instead of the
toolkit e.g. RISC OS WIMP frontend is named riscos and the Windows
win32 frontend is named windows.

### amiga

Frontend specific to the amiga

### atari

Frontend specific to the atari

### beos

Frontend specific to the Haiku OS

### framebuffer

There is a basic user guide for the[framebuffer](docs/using-framebuffer.md)

### gtk

Frontend that uses the GTK+2 or GTK+3 toolkit

### monkey

This is the internal unit test frontend.

There is a basic user guide [monkey](docs/using-monkey.md)

### riscos

Frontend for the RISC OS WIMP toolkit.

### windows

Frontend which uses the Microsodt win32 GDI toolkit.

## User configuration

The behaviour of the browser can be changed from the defaults with a
configuration file. The [core user options](docs/netsurf-options.md)
of the browser are common to all versions and are augmented by each
frontend in a specific manner.


# Development

## Working with the team

Generally it is sensible to check with the other developers if you are
planning to make a change to NetSurf intended to be merged.

We are often about on the IRC channel but failing that the developer
mailing list is a good place to try.

All the project sources are held in [public git repositories](http://source.netsurf-browser.org/)

## Compilation environment

Compiling a development edition of NetSurf requires a POSIX style
environment. Typically this means a Linux based system although Free
BSD, Open BSD, Mac OS X and Haiku all known to work.

## Toolchains

Compilation for non POSIX toolkits/frontends (e.g. RISC OS) generally
relies upon a cross compilation environment which is generated using
the makefiles found in our
[toolchains](http://source.netsurf-browser.org/toolchains.git/)
repository. These toolchains are built by the Continuous Integration
(CI) system and the
[results of the system](http://ci.netsurf-browser.org/builds/toolchains/)
are published as a convenience.

## Quick setup

The [quick start guide](docs/quick-start.md) can be used to get a
development environment setup quickly and uses the
[env.sh](env_8sh_source.html) script the core team utilises.

## Manual setup

The Manual environment setup and compilation method is covered by the
details in the [netsurf libraries](docs/netsurf-libraries.md) document
for the core libraries and then one of the building documents for the
specific frontend.

- [Amiga Os cross](docs/building-AmigaCross.md) and [Amiga OS](docs/building-AmigaOS.md)
- [Framebuffer](docs/building-Framebuffer.md)
- [GTK](docs/building-GTK.md)
- [Haiku (BeOS)](docs/building-Haiku.md)
- [Windows Win32](docs/building-Windows.md)

These documents are sometimes not completely up to
date and the env.sh script should be considered canonical.

## Logging

The [logging](docs/logging.md) interface controls debug and error
messages not output through the GUI.

## Documented API

The NetSurf code makes use of Doxygen for code documentation.

There are several documents which detail specific aspects of the
codebase and APIs.

### Core window

The [core window API](docs/core-window-interface.md) allows frontends
to use generic core code for user interface elements beyond the
browser render.

### Source object caching

The [source object caching](docs/source-object-backing-store.md)
provides a way for downloaded content to be kept on a persistent
storage medium such as hard disc to make future retrieval of that
content quickly.

## Javascript

Javascript provision is split into four parts:
- An engine that takes source code and executes it.
- Interfaces between the program and the web page.
- Browser support to retrive and manage the source code to be executed.
- Browser support for the dispatch of events from user interface.

### Library

JavaScript is provided by integrating the duktape library. There are
[instructions](docs/updating-duktape.md) on how to update the library.

### Interface binding

In order for javascript programs to to interact with the page contents
it must use the Document Object Model (DOM) and Cascading Style Sheet
Object Model (CSSOM) API.

These interfaces are described using web Interface Description
Language (IDL) within the relevant specifications
(e.g. https://dom.spec.whatwg.org/).

Each interface described by the webIDL must be bound (connected) to
the browsers internal representation for the DOM or CSS, etc. The
process of [writing bindings](docs/jsbinding.md) is ongoing.
