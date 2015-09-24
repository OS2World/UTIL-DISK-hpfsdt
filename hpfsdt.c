/************************************************************************/
/*                                                                      */
/*  HPFSDT.C - Toggle the "dirty" flag on HPFS drives under OS/2        */
/*                                                                      */
/*  (C)1999 Mike Ruskai, all rights reserved, blah, blah, blah          */
/*                                                                      */
/*  Contact:  thanmeister@geocities.com                                 */
/*                                                                      */
/*  The following code reads the partition status byte contained in     */
/*  the HPFS SpareBlock structure, located at logical sector number 17  */
/*  on an HPFS volume, toggles the first bit, and writes it back to     */
/*  the same location on the drive.                                     */
/*                                                                      */
/*  This code was written for compilation by IBM's VisualAge C++        */
/*  version 3 compiler, though it should work with any 32-bit OS/2      */
/*  compiler (you may have to remove the _Inline linkage).              */
/*                                                                      */
/*  You may use and modify this code, so long as proper credit is       */
/*  given, and the appropriate warnings about usage are preserved.      */
/*                                                                      */
/*  And speaking of credit, I'd like to thank the UseNet presence of    */
/*  Peter Fitzsimmons, whose articles (archived at DejaNews) provided   */
/*  the necessary examples for doing a couple of the things below.      */
/*  I'd also like to thank EDM/2, the online developer's magazine for   */
/*  OS/2, for publishing the articles on HPFS which were my source of   */
/*  information on how to do what this program does.                    */
/*                                                                      */
/************************************************************************/

#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSFILEMGR
#define INCL_DOSERRORS
#include <os2.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define STDOUT 1
#define STDERR 2

#define TOGGLE_DIRTY_CLEAN 1
#define TOGGLE_CLEAN_DIRTY 2
#define FAIL_USAGE 3
#define FAIL_BAD_DRIVE_STRING 4
#define FAIL_INVALID_DRIVE 5
#define FAIL_UNKNOWN_FSTYPE 6
#define FAIL_NOT_HPFS 7
#define FAIL_OPEN_DRIVE 8
#define FAIL_LOCK_DRIVE 9
#define FAIL_UNKNOWN_SECTOR_SIZE 10
#define FAIL_BAD_SECTOR_SIZE 11
#define FAIL_FILE_POINTER 12
#define FAIL_READ_DRIVE 13
#define FAIL_WRITE_DRIVE 14
#define FAIL_MEMORY_ALLOCATION 15

_Inline void Usage(void);
_Inline void rcReportError(char *fName, ULONG rc);
_Inline void ReportError(char *eString);

char msgBuf[128], miscBuf[33];
ULONG cWritten;

int main(int argc, char *argv[])
{
    HFILE driveHandle;
    ULONG openAction, openMode, bufLen, dPointer, cRead,
          bpParmLen, bpDataLen;
    int exitval;
    LONG offset;
    APIRET rc;
    char driveString[3], driveLetter, *fsStuff, partStatus, pDirty=1,
         pStat[8], cStat[8], bpParm[2], bpData[50];
    USHORT sectorSize;
    FSQBUFFER2 *fsBuffer;
    BIOSPARAMETERBLOCK bpBlock;

    if (argc!=2)
        {
        Usage();
        return FAIL_USAGE;
        }

    memset(driveString, 0, sizeof(driveString));
    strncpy(driveString, argv[1], sizeof(driveString)-1);

    /*
        The following verifies that the drive letter provided
        is a letter from A to Z, and that the second character
        is a colon
    */

    driveLetter=toupper(driveString[0]);
    if (driveLetter<65 || driveLetter>90 || driveString[1]!=':')
        {
        ReportError("Invalid drive string provided.");
        Usage();
        return FAIL_BAD_DRIVE_STRING;
        }

    /*
        The following uses DosQueryFSAttach() to verify that the
        file system of the specified drive is indeed HPFS, as
        continuing otherwise would have somewhat unpredictable
        results.  A lexically valid, but non-existent drive would
        also be caught at this point.
    */

    fsBuffer=(PFSQBUFFER2)malloc(2048);
    if (fsBuffer==NULL)
        {
        ReportError("Memory allocation error.");
        return FAIL_MEMORY_ALLOCATION;
        }
    memset(fsBuffer, 0, 2048);
    bufLen=2048;
    rc=DosQueryFSAttach(driveString, 0, FSAIL_QUERYNAME, fsBuffer, &bufLen);
    if (rc!=NO_ERROR)
        {
        if (rc==ERROR_INVALID_DRIVE)
            {
            ReportError("Invalid drive specified.");
            return FAIL_INVALID_DRIVE;
            }
        else
            {
            ReportError("Unable to determine file system.");
            rcReportError("DosQueryFSAttach", rc);
            return FAIL_UNKNOWN_FSTYPE;
            }
        }
    fsStuff=(char*)(fsBuffer->szName+fsBuffer->cbName+1);
    if (strcmp(fsStuff, "HPFS")!=0)
        {
        ReportError("Specified drive is not using the HPFS file system.");
        Usage();
        return FAIL_NOT_HPFS;
        }
    free(fsBuffer);

    /*
        This next bit uses DosOpen() to open the entire drive for reading
        and writing.
    */

    openMode=0;
    openMode=openMode | OPEN_FLAGS_DASD | OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYREADWRITE;
    rc=DosOpen(driveString, &driveHandle, &openAction, 0L, 0L, FILE_OPEN, openMode, 0L);
    if (rc!=NO_ERROR)
        {
        ReportError("Unable to open drive.");
        rcReportError("DosOpen", rc);
        return FAIL_OPEN_DRIVE;
        }

    /*
        Once successfully opened, the drive needs to be locked, so that
        we know it's safe to modify the SpareBlock.  This is done with
        a category 8, function 0x00 call to DosDevIOCtl().  This will fail 
        on drives which are in use (open programs, open files, etc.).
    */

    rc=DosDevIOCtl(driveHandle, IOCTL_DISK, DSK_LOCKDRIVE, 0, 0, 0, 0, 0, 0);
    if (rc!=NO_ERROR)
        {
        DosClose(driveHandle);
        ReportError("Unable to lock drive.");
        rcReportError("DosDevIOCtl", rc);
        return FAIL_LOCK_DRIVE;
        }

    /*
        Here we use a category 8, function 0x63 call to DosDevIOCtl()
        to determine the physical sector size of the drive in question.
        This information is necessary to correctly set the file
        pointer to the location of the partition status byte.  The only
        checking done is to verify that the returned value is greater
        than zero.  Otherwise, the program relies entirely on the 
        validity of the data in the BIOS Parameter Block, which is just
        a label for certain information contained in the boot sector.
        If the boot sector is corrupted, and an incorrect sector size
        is returned, then the results of the program are unpredictable.
    */

    memset(bpParm, 0, sizeof(bpParm));
    memset(bpData, 0, sizeof(bpData));
    bpParmLen=0;
    bpDataLen=0;
    rc=DosDevIOCtl(driveHandle, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                   bpParm, sizeof(bpParm), &bpParmLen,
                   bpData, sizeof(bpData), &bpDataLen);

    if (rc!=NO_ERROR)
        {
        DosDevIOCtl(driveHandle, IOCTL_DISK, DSK_UNLOCKDRIVE, 0, 0, 0, 0, 0, 0);
        DosClose(driveHandle);
        ReportError("Unable to determine sector size.");
        rcReportError("DosDevIOCtl", rc);
        return FAIL_UNKNOWN_SECTOR_SIZE;
        }
    
    memset(&bpBlock, 0, sizeof(bpBlock));
    memcpy(&bpBlock, bpData, sizeof(bpBlock));
    sectorSize=0;
    sectorSize=bpBlock.usBytesPerSector;
    if (sectorSize==0)
        {
        DosDevIOCtl(driveHandle, IOCTL_DISK, DSK_UNLOCKDRIVE, 0, 0, 0, 0, 0, 0);
        DosClose(driveHandle);
        ReportError("Bad sector size returned by DosDevIOCtl().");
        return FAIL_BAD_SECTOR_SIZE;
        }

    /*
        Here we set the file read/write pointer to logical byte number
        eight on logical sector number 17.  Then we read the byte.
    */

    offset=(LONG)sectorSize*17L+8;
    rc=DosSetFilePtr(driveHandle, offset, FILE_BEGIN, &dPointer);
    if (rc!=NO_ERROR)
        {
        DosDevIOCtl(driveHandle, IOCTL_DISK, DSK_UNLOCKDRIVE, 0, 0, 0, 0, 0, 0);
        DosClose(driveHandle);
        rcReportError("DosSetFilePtr", rc);
        return FAIL_FILE_POINTER;
        }

    partStatus=0;
    rc=DosRead(driveHandle, &partStatus, sizeof(partStatus), &cRead);
    if (rc!=NO_ERROR)
        {
        DosDevIOCtl(driveHandle, IOCTL_DISK, DSK_UNLOCKDRIVE, 0, 0, 0, 0, 0, 0);
        DosClose(driveHandle);
        rcReportError("DosRead", rc);
        return FAIL_READ_DRIVE;
        }

    /*
        If a bitwise AND of the partition status byte and the value
        1 returns 1, the partition is dirty, and the value of the whole
        byte is decremented.  Otherwise, the partition is clean, and the
        byte is incremented.  The status strings for before and after are
        set accordingly.
    */

    if (partStatus & pDirty)
        {
        partStatus--;
        strcpy(pStat, "\"dirty\"");
        strcpy(cStat, "\"clean\"");
        exitval=TOGGLE_DIRTY_CLEAN;
        }
    else
        {
        partStatus++;
        strcpy(pStat, "\"clean\"");
        strcpy(cStat, "\"dirty\"");
        exitval=TOGGLE_CLEAN_DIRTY;
        }

    /*
        The file pointer is moved back to the appropriate position, and
        the modified byte is written back to the drive.
    */

    rc=DosSetFilePtr(driveHandle, offset, FILE_BEGIN, &dPointer);
    if (rc!=NO_ERROR)
        {
        DosDevIOCtl(driveHandle, IOCTL_DISK, DSK_UNLOCKDRIVE, 0, 0, 0, 0, 0, 0);
        DosClose(driveHandle);
        rcReportError("DosSetFilePtr", rc);
        return FAIL_FILE_POINTER;
        }
    rc=DosWrite(driveHandle, &partStatus, sizeof(partStatus), &cWritten);
    if (rc!=NO_ERROR)
        {
        DosDevIOCtl(driveHandle, IOCTL_DISK, DSK_UNLOCKDRIVE, 0, 0, 0, 0, 0, 0);
        DosClose(driveHandle);
        rcReportError("DosWrite", rc);
        return FAIL_WRITE_DRIVE;
        }

    /*
        Finally, we report which way the toggle went, unlock the drive,
        and close the file handle.  All done.
    */

    strcpy(msgBuf, "\r\nDrive was ");
    strcat(msgBuf, pStat);
    strcat(msgBuf, " - changed to ");
    strcat(msgBuf, cStat);
    strcat(msgBuf, ".\r\n");
    DosWrite(STDOUT, msgBuf, strlen(msgBuf), &cWritten);

    DosDevIOCtl(driveHandle, IOCTL_DISK, DSK_UNLOCKDRIVE, 0, 0, 0, 0, 0, 0);
    DosClose(driveHandle);

    return exitval;
}

/*
    This just shows the program usage, which is exceedingly simple.
*/

_Inline void Usage(void)
{
    ReportError("Usage: hpfsdt.exe <drive letter>");
    ReportError("<drive letter> - letter of HPFS drive to toggle dirty bit on");
    return;
}

/*
    These next two just clean up error reporting in the main function.
    The first gives a generic API function failure message.  The second
    just writes a string.  Both go to standard error.
*/

_Inline void rcReportError(char *fName, ULONG rc)
{
    strcpy(msgBuf, "\r\n");
    strcat(msgBuf, fName);
    strcat(msgBuf, "() failed with error ");
    strcat(msgBuf, _ltoa(rc, miscBuf, 10));
    strcat(msgBuf, ".\r\n");
    DosWrite(STDERR, msgBuf, strlen(msgBuf), &cWritten);
    return;
}

_Inline void ReportError(char *eString)
{
    strcpy(msgBuf, "\r\n");
    strcat(msgBuf, eString);
    strcat(msgBuf, "\r\n");
    DosWrite(STDERR, msgBuf, strlen(msgBuf), &cWritten);
    return;
}
