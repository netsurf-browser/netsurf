Usage Instructions for Monkey NetSurf
=====================================

This document provides usage instructions for the Monkey version of NetSurf.

Monkey NetSurf has been tested on Ubuntu and Debian.

Overview
--------

### What it is

The NetSurf Monkey front end is a developer debug tool used to test how the
core interacts with the user interface.  It allows the developers to profile
NetSurf and to interact with the core directly as though the developer were a
front end.
 
### What it is not

Monkey is not a tool for building web-crawling robots or indeed anything other
than a debug tool for the NetSurf developers.

### How to interact with `nsmonkey`

In brief, `nsmonkey` will produce tagged output on stdout and expect
commands on stdin.  Windows are numbered and for the most part
tokens are space separated.  In some cases (e.g. title or status)
the final element on the output line is a string which might have
spaces embedded within it.  As such, output from `nsmonkey` should be
parsed a token at a time, so that when such a string is encountered,
the parser can stop splitting and return the rest.

Commands to `nsmonkey` are namespaced.  For example commands related to
browser windows are prefixed by `WINDOW`.

### Top level tags for `nsmonkey`

* `QUIT`

* `WINDOW`

* `OPTIONS`

### Top level response tags for nsmonkey

* `GENERIC`: Generic messages such as poll loops etc.

* `WARN`, `ERROR`, `DIE`: Error messages of varying importance

* `WINDOW`: Anything about browser windows in general

* `DOWNLOAD_WINDOW`: Anything about the download window.

* `SSLCERT`: Anything about SSL certificates

* `401LOGIN`: Anything about HTTP Basic Authentication logins

* `PLOT`: Plot calls which come from the core.

In the below, _%something%_ indicates a substitution made by Monkey.

* _%url%_ will be a URL
* _%id%_ will be an opaque ID
* _%n%_ will be a number
* _%bool%_ will be TRUE or FALSE
* _%str%_ is a string and will only ever be at the end of an output line.

### Warnings, errors etc

*  Warnings (tagged `WARN`) come from the NetSurf core.
*  Errors (tagged `ERROR`) tend to come from Monkey's parsers
*  Death (tagged `DIE`) comes from the core and kills Monkey dead.

Commands
--------

### Generic commands

*   `QUIT`

    Cause monkey to quit cleanly.
    This will cleanly destroy open windows etc.

*   `OPTIONS` _%str_

    Cause monkey to set options.  The passed options should be in the same
    form as the command line, e.g. `OPTIONS --enable_javascript=1`
    

### Window commands

*   `WINDOW NEW` [_%url%_]

    Create a new browser window, optionally giving the core
    a URL to immediately navigate to.
    Minimally you will receive a `WINDOW NEW WIN` _%id%_ response.

*   `WINDOW DESTROY` _%id%_

    Destroy the given browser window.
    Minimally you will receive a `WINDOW DESTROY WIN` _%id%_ response.

*   `WINDOW GO` _%id%_ _%url%_ [_%url%_]

    Cause the given browser window to visit the given URL.
    Optionally you can give a referrer URL to also use (simulating
    a click in the browser on a link).
    Minimally you can expect throbber, url etc responses.

*   `WINDOW REDRAW` _%id%_ [_%num% %num% %num% %num%_]

    Cause a browser window to redraw.  Optionally you can give a
    set of coordinates to simulate a partial expose of the window.
    Said coordinates are in traditional X0 Y0 X1 Y1 order.
    The coordinates are in canvas, not window, coordinates.  So you
    should take into account the scroll offsets when issuing this
    command.
    Minimally you can expect redraw start/stop messages and you
    can likely expect some number of `PLOT` results.

*   `WINDOW RELOAD` _%id%_

    Cause a browser window to reload its current content.
    Expect responses similar to a GO command.


Responses
---------

### Generic messages

*   `GENERIC STARTED`

    Monkey has started and is ready for commands

*   `GENERIC CLOSING_DOWN`

    Monkey has been told to shut down and is doing so

*   `GENERIC FINISHED`

    Monkey has finished and will now exit

*   `GENERIC LAUNCH URL` _%url%_

    The core asked monkey to launch the given URL

*   `GENERIC THUMBNAIL URL` _%url%_

    The core asked monkey to thumbnail a content without
    a window.

*   `GENERIC POLL BLOCKING`
*   `GENERIC POLL TIMED` _%n%_

    Monkey reached a point where it could sleep waiting for
    commands or scheduled timeouts.  No fetches nor redraws
    were pending.  If there are no timeouts or other pending
    jobs then this will be a BLOCKING poll, otherwise the number
    given is in milliseconds.

### Window messages

*   `WINDOW NEW WIN` _%id%_ `FOR` _%id%_ `CLONE` _%id%_ `NEWTAB` _%bool%_

    The core asked Monkey to open a new window.  The IDs for `FOR` and
    `CLONE` are core window IDs, the `WIN` id is a Monkey window ID.

*   `WINDOW SIZE WIN` _%id%_ `WIDTH` _%n%_ `HEIGHT` _%n%_

    The window specified has been set to the shown width and height.

*   `WINDOW DESTROY WIN` _%id%_

    The core has instructed Monkey to destroy the named window.

*   `WINDOW TITLE WIN` _%id%_ `STR` _%str%_

    The core supplied a titlebar title for the given window.

*   `WINDOW REDRAW WIN` _%id%_

    The core asked that Monkey redraw the given window.

*   `WINDOW GET_DIMENSIONS WIN` _%id%_ `WIDTH` _%n%_ `HEIGHT` _%n%_

    The core asked Monkey what the dimensions of the window are.
    Monkey has to respond immediately and returned the supplied width
    and height values to the core.

*   `WINDOW NEW_CONTENT WIN` _%id%_

    The core has informed Monkey that the named window has a new
    content object.

*   `WINDOW NEW_ICON WIN` _%id%_

    The core has informed Monkey that the named window has a new
    icon (favicon) available.

*   `WINDOW START_THROBBER WIN` _%id%_

    The core asked Monkey to start the throbber for the named
    window.  This indicates to the user that the window is busy.

*   `WINDOW STOP_THROBBER WIN` _%id%_

    The core asked Monkey to stop the throbber for the named
    window.  This indicates to the user that the window is finished.

*   `WINDOW SET_SCROLL WIN` _%id%_ `X` _%n%_ `Y` _%n%_

    The core asked Monkey to set the named window's scroll offsets
    to the given X and Y position.

*   `WINDOW UPDATE_BOX WIN` _%id%_ `X` _%n%_ `Y` _%n%_ `WIDTH` _%n%_ `HEIGHT` _%n%_

    The core asked Monkey to redraw the given portion of the content
    display.  Note these coordinates refer to the content, not the
    viewport which Monkey is simulating.

*   `WINDOW UPDATE_EXTENT WIN` _%id%_ `WIDTH` _%n%_ `HEIGHT` _%n%_

    The core has told us that the content in the given window has a
    total width and height as shown.  This allows us (along with the
    window's width and height) to know the scroll limits.
    
*   `WINDOW SET_STATUS WIN` _%id%_ `STR` _%str%_

    The core has told us that the given window needs its status bar
    updating with the given message.

*   `WINDOW SET_POINTER WIN` _%id%_ `POINTER` _%id%_

    The core has told us to update the mouse pointer for the given
    window to the given pointer ID.

*   `WINDOW SET_SCALE WIN` _%id%_ `SCALE` _%n%_

    The core has asked us to scale the given window by the given scale
    factor.

*   `WINDOW SET_URL WIN` _%id%_ `URL` _%url%_

    The core has informed us that the given window's URL bar needs
    updating to the given url.

*   `WINDOW GET_SCROLL WIN` _%id%_ `X` _%n%_ `Y` _%n%_

    The core asked Monkey for the scroll offsets.  Monkey returned the
    numbers shown for the window named.

*   `WINDOW SCROLL_START WIN` _%id%_

    The core asked Monkey to scroll the named window to the top/left.

*   `WINDOW POSITION_FRAME WIN` _%id%_ `X0` _%n%_ `Y0` _%n%_ `X1` _%n%_ `Y1` _%n%_

    The core asked Monkey to position the named window as a frame at
    the given coordinates of its parent.

*   `WINDOW SCROLL_VISIBLE WIN` _%id%_ `X0` _%n%_ `Y0` _%n%_ `X1` _%n%_ `Y1` _%n%_

    The core asked Monkey to scroll the named window until the
    indicated box is visible.

*   `WINDOW PLACE_CARET WIN` _%id%_ `X` _%n%_ `Y` _%n%_ `HEIGHT` _%n%_

    The core asked Monkey to render a caret in the named window at the
    indicated position with the indicated height.

*   `WINDOW REMOVE_CARET WIN` _%id%_

    The core asked Monkey to remove any caret in the named window.

*   `WINDOW SCROLL_START WIN` _%id%_ `X0` _%n%_ `Y0` _%n%_ `X1` _%n%_ `Y1` _%n%_

    The core asked Monkey to scroll the named window to the start of
    the given box.

*   `WINDOW SELECT_MENU WIN` _%id%_

    The core asked Monkey to produce a selection menu for the named
    window.

*   `WINDOW SAVE_LINK WIN` _%id%_ `URL` _%url%_ `TITLE` _%str%_

    The core asked Monkey to save a link from the given window with
    the given URL and anchor title.

*   `WINDOW THUMBNAIL WIN` _%id%_ `URL` _%url%_

    The core asked Monkey to render a thumbnail for the given window
    which is currently at the given URL.

*   `WINDOW REDRAW WIN` _%id%_ `START`

    and

    `WINDOW REDRAW WIN` _%id%_ `STOP`

    The core wraps redraws in these messages.  Thus `PLOT` responses can
    be allocated to the appropriate window.

### Download window messages

*   `DOWNLOAD_WINDOW CREATE DWIN` _%id%_ `WIN` _%id%_

    The core asked Monkey to create a download window owned by the
    given browser window.

*   `DOWNLOAD_WINDOW DATA DWIN` _%id%_ `SIZE` _%n%_ `DATA` _%str%_

    The core asked Monkey to update the named download window with
    the given byte size and data string.

*   `DOWNLOAD_WINDOW ERROR DWIN` _%id%_ `ERROR` _%str%_

    The core asked Monkey to update the named download window with
    the given error message.

*   `DOWNLOAD_WINDOW DONE DWIN` _%id%_

    The core asked Monkey to destroy the named download window.

### SSL Certificate messages

*   `SSLCERT VERIFY CERT` _%id%_ `URL` _%url%_

    The core asked Monkey to say whether or not a given SSL
    certificate is OK.

> TODO: Implement the rest of the SSL certificat verification behaviour

### 401 Login messages

*   `401LOGIN OPEN M4` _%id%_ `URL` _%url%_ `REALM` _%str%_

    The core asked Monkey to ask for identification for the named
    realm at the given URL.

> TODO: Implement support to control the 401LOGIN process

### Plotter messages

> **Note, Monkey won't clip coordinates, but sometimes the core does.**

*   `PLOT CLIP X0` _%n%_ `Y0` _%n%_ `X1` _%n%_ `Y1` _%n%_

    The core asked Monkey to clip plotting to the given clipping
    rectangle (X0,Y0) (X1,Y1)

*   `PLOT TEXT X` _%n%_ `Y` _%n%_ `STR` _%str%_

    The core asked Monkey to plot the given string at the
    given coordinates.

*   `PLOT LINE X0` _%n%_ `Y0` _%n%_ `X1` _%n%_ `Y1` _%n%_

    The core asked Monkey to plot a line with the given start
    and end coordinates.

*   `PLOT RECT X0` _%n%_ `Y0` _%n%_ `X1` _%n%_ `Y1` _%n%_

    The core asked Monkey to plot a rectangle with the given
    coordinates as the corners.

*   `PLOT BITMAP X` _%n%_ `Y` _%n%_ `WIDTH` _%n%_ `HEIGHT` _%n%_

    The core asked Monkey to plot a bitmap at the given
    coordinates, scaled to the given width/height.

> TODO: Check if other things are implemented and add them to the docs
