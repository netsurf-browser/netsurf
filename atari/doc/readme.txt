Missing Features:
-----------------

- Also look at BUGS file!
- No networking with classic TOS.
- Frames are not implemented - if you hit an page with frames, you are advised to close the window and open a 
  new one. 
- Many of the Menu items are not working yet.
- No Bookmarks & No History
- No Download Manager 

Things to test:
---------------

- Networking code
- SSL
- Window Redraw - sometimes it looks like the page is redrawn all the times... (ebay...) 
  When you start to scroll, this stops. 
  Find out if the redraw stops at some point without the need of the user interaction.
- Navigate to a lot of pages, note the ones that crashed, please don't report pages that you visited 
  after visiting pages with frames... Make sure you don't call pages with frames. I know that it's not working,
  and I know that it can cause BUGS.  
- Have a look at the memory usage... 
- Copy & Paste ( please don't report unsupported characters, I know it's not perfect)
  But you can use CTRL+A to select everything. Try it. 


 