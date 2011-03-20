

                               NETSURF

                                  -

                           NATIVE ATARI ALPHA

                  Version 3.0 (Development Version)

Ported by:     m0n0
Release date:  20.02.2011
Contact:       ole@monochrom.net
WWW:           http://netsurf-browser.org


Table of Contents:
------------------

0x01 - What is it? 
0x02 - System Requirements 
0x03 - Features
0x04 - Missing features 
0x05 - Things to test
0x06 - Additional Notes 
0x07 - Known bugs
0x08 - Technical information


What is it? A web browser!
--------------------------

 NetSurf is a multi-platform web browser which is written with 
 portability and speed in mind. 
 This is the native Port for the FreeMiNT OS.  
 More info at project website: www.netsurf-browser.org


Minimum System Requirements:
----------------------------

 - 32 MB RAM ( 48 MB recommended for demanding websites )
 - 32 MHz ( 60 Mhz recommended )
 - At least 15 Bit Graphics card. 
 - FreeMiNT 1.17.0 release kernel (Please look at FAQ to read
   about TOS support) for full & correct network support.
   

Main Features:
--------------

 - Very good HTML 4 & CSS 2.1 rendering
 - HTTPS 
 - Freetype2 font rendering


Missing Features:
-----------------

 This section describes Features that NetSurf-Core offers but which are not
 handled by the GEM frontend currently. 
 
- Frames 
- Hotlist / Bookmarks 
- Configuration dialog (use texteditor instead)
- Grapical website history dialog 


Installation Notes:
-------------------

 Unpack the compressed archive that you downloaded, 
 change into the new directory and run ns.prg.
 If something isn't working - run ns.prg within an console and 
 enable logging:

 ./ns.prg -v 

 that makes it possible, that you can identify the problem.


Things to test:
---------------

 - Navigate to a lot of pages, note the ones that crashed, please don't report pages that you visited 
   after visiting pages with frames... Make sure you don't call pages with frames. I know that it's not working,
   and I know that it can cause BUGS.  
 - Have a look at the memory usage... 


Additional Notes
----------------
 
 If you would like to see the above mentioned features or 
 can't run NetSurf because you only have a 16 or 256 Color system 
 get in contact.

 Please also check the FAQ document.

 Want to have other software ported? Get in contact and make me rich >;-)
 If you want to help with netsurf, contact me for further info
 or visit the netsurf svn and add something usefull :)

 This is "just an early" alpha release. I wanted to get things moving on
 and I think it is good to show the Atari-Users what has been archived
 so far. This release lacks some features and some of the code written
 was just coded with an "I have to get this done quickly" attitude.
 This is especially true for the drawin routines... It doesn't offer
 offscreen bitmaps, which was one of my goals for a release. But 
 I dropped that in favor of an not-so-delayed release.


Known Bugs
----------

 - Redraw artefacts/fale clipping when moving other applications above 
   NetSurf.
 - Visiting frames results in unknown behavior
   (Some pages containing iframes still display, 
    but there are problems with focusing the correct window, 
    which results in missing redraws - clicking the mouse within 
    the window or scrolling resolves the problem).
 - Double Redraws when used with classic TOS systems.
 - Window can not be moved out of the desktop area to the right
 - When leaving the search dialog open and navigate to another 
   page, the behavior is unknown. 
   As a solution: always close the search before you visit another page 
   or click on a link.


Technical info & outlook
------------------------

 1. There is an stack excess lurking withing mintlibs regex implementation.
    Some pages can trigger that excess, therefore the initial stack space 
    is set to an very large default value - around 3 MB. You are free to 
    lower that value  with freemint stack tool - most pages even work fine
    with 64k stack space! Yahoo & dict.leo.org are known to trigger the 
    stack excess.   
    Last minute note: because the default maximum TPA value within MINT.CNF
    I had to adjust the stack value down to 1000k. If you encounter crashes
    during page rendering - please increase the stack size (SEE FAQ)


Greetings & Thanks
------------------

 - AtFact for providing help with resource files & images
 - The MiNT Mailing list, they all helped me a lot!
 - The NetSurf Mailing list guys, especially the Amiga guys.
 - The NetSurf developers that did a great job! 
 - Everyone that tested this Browser! 
 - Everyone that provides feedback! 
 - The forum.atari-home.de members for giving me much help 
   during setup of my atari!


----
M0N0 - 19.02.2011



 