HPFSDT.EXE is a program that will toggle the status of an HPFS partition
between "dirty" and "clean".

It does this by toggling the first bit of the partition status byte, in the 
HPFS SpareBlock; which is logical byte 8 of logical sector 17.

To use this program, you must be using OS/2 version 2.0 or later.  It has
only been tested on OS/2 Warp v4, but does not use any functionality beyond
what's present in OS/2 2.0 (as far as I know).  It will run from a floppy 
boot of OS/2, at least one using the disks created by the "Create Utility 
Disks" program in OS/2.  Make sure that the floppy drive is either in the 
libpath, or the current working drive, as there are some OS/2 DLL's 
required.

Why would you use this program?  There are two legitimate reasons that I
can think of:

1)  You are developing something that's crashing your computer often, and
    are tired of waiting for CHKDSK to complete each time you start up.

2)  You are experiencing problems where CHKDSK is unable to complete on a
    drive, and you'd like to try getting your data off before reformatting
    and discovering whether or not the drive is shot (this is the exact
    reason I needed this program myself, causing its creation).

It's certainly possible to do what this program does with a sector editor.  
GammaTech's GTDISK program also has an option to toggle the dirty flag on a 
drive.  The former requires knowledge of which byte on which sector to 
modify, and the latter requires the purchase of a commercial software 
package (the GammaTech Utilities 3.0, which is worth buying).

This is merely a free and simple alternative, which I wrote because my copy 
of the GammaTech Utilities was located on a drive I couldn't access without 
toggling the dirty bit (see reason number two above).  I'm releasing it 
because I'm quite sure others would find it useful.

Usage:

hpfsdt.exe <drive letter>

<drive letter> - drive letter of the HPFS drive to toggle the dirty bit on

Upon running the program, you'll either get an indication of how the dirty 
bit was toggled, or an error message indicating what failed.  The computer 
must be rebooted before OS/2 will read a drive that was booted with the 
dirty flag on.

Exit Levels:

1  - Drive changed from "dirty" to "clean".
2  - Drive changed from "clean" to "dirty".
3  - Wrong number of arguments, usage displayed.
4  - Invalid drive string provided.
5  - Valid string, but invalid drive provided.
6  - Unable to determine file system type.
7  - Drive provided does not use HPFS.
8  - Unable to open drive.
9  - Unable to lock drive.
10 - Unable to determine sector size.
11 - Sector size of zero (invalid) returned by DosDevIOCtl().
12 - Unable to set file pointer.
13 - Unable to read from drive.
14 - Unable to write to drive.
15 - Memory allocation failure.

WARNING:  An improperly stopped HPFS drive often does have minor problems 
that require repair by CHKDSK.  You should not use this program without a 
good reason, as you could potentially aggravate file system problems.  If 
the boot sector of the drive is corrupted, and reports an incorrect sector 
size, the results of running this program are unpredictable.

DISCLAIMER:  This program directly modifies your hard drive, and there are 
opportunities for drive corruption to cause incorrect program behavior.  
The author is in no way responsible for any damage whatsoever that the 
program may do.

USAGE & DISTRIBUTION:  This program may be used and distributed freely.  No
fee can be charged for this program by anyone.  This does not include the
cost of distribution and media for collections of freeware. All
distributions must include the following files:

    1) hpfsdt.exe  - the executable program
    2) hpfsdt.c    - the program source
    3) readme.txt  - the documentation (this file)
    4) file_id.diz - the program description for file listings
    5) comment.arc - the archive comment text

The files may be rearchived, but the contents of comment.arc must be
displayed upon extraction.

CONTACT:  If you have a comment, question,  or suggestion; address it to 
Mike Ruskai <thanmeister@geocities.com>.  
