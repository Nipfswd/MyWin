/*++ BUILD Version: 0002

Copyright (c) 2026  NaznaOS Project

Module Name:

    iop.h

Abstract:


Author:


Revision History:

    Noah Juopperi (noahju) 17-Apr-2026      Added Hiber file compression

--*/

#include "bldr.h"
#include "msg.h"
#include "stdio.h"
#include "stdlib.h"
#include "xpress.h"

extern UCHAR WakeDispatcherStart;
extern UCHAR WakeDispatcherEnd;

#if defined(_X86_)
extern UCHAR WakeDispatcherAmd64Start;
extern UCHAR WakeDispatcherAmd64End;
#endif

//
//
// Hiber globals
//
// HiberFile    - File handle
// HiberBuffer  - PAGE of ram
// HiberIoError - SEt to true to indicate an IO read error occured during restore
//

ULONG       HiberFile;
PUCHAR      HiberBuffer;
ULONG       HiberBufferPage;
BOOLEAN     HiberIoError;
BOOLEAN     HiberOutOfRemap;
BOOLEAN     HiberAbort;
LARGE_INTEGER HiberStartTime;
LARGE_INTEGER HiberEndTime;
ULONG       HiberNoExecute = 0;

//
// HiberImageFeatureFlags - Feature flags from hiber image header
// HiberBreakOnWake - BreakOnWake flag from hiber image header
//

BOOLEAN HiberBreakOnWake;
ULONG HiberImageFeatureFlags;

#if defined(_ALPHA_) || defined(_IA64_)

//
// On Alpha, the address of the KPROCESSOR_STATE read from the hiber file
// must be saved where WakeDispatch can find it (it's at a fixed offset
// relative to HiberVa on x86).
//

PKPROCESSOR_STATE HiberWakeState;

#else   // x86

//
// HiberPtes - Virtual address of ptes to use for restoriation.  There
// are at least HIBER_PTES consecutive ptes for use, and are for the
// address of HiberVa
//
// HiberVa - The virtual address the HiberPtes map
//
// HiberIdentityVa - The restoration images HiberVa
//
// HiberPageFrames - Page frames of the hiber ptes (does not include dest pte)
//

PVOID HiberPtes = NULL;
PUCHAR HiberVa = NULL;
PVOID HiberIdentityVa = NULL;
ULONG64 HiberIdentityVaAmd64 = 0;
ULONG HiberNoHiberPtes;
ULONG HiberPageFrames[HIBER_PTES];

#endif  // Alpha/x86

PFN_NUMBER HiberImagePageSelf;
ULONG HiberNoMappings;
ULONG HiberFirstRemap;
ULONG HiberLastRemap;

extern
ULONG
BlGetKey(
    VOID
    );

extern
ULONG
BlDetermineOSVisibleMemory(
    VOID
    );


VOID
BlUpdateProgressBar(
    ULONG fPercentage
    );

VOID
BlOutputStartupMsg(
    ULONG   uMsgID
    );

VOID
BlOutputTrailerMsg(
    ULONG   uMsgID
    );

//
// Defines for Hiber restore UI
//

ULONG   HbCurrentScreen;

#define BAR_X                       7
#define BAR_Y                      10
#define PERCENT_BAR_WIDTH          66

#define PAUSE_X                     7
#define PAUSE_Y                     7

#define FAULT_X                     7
#define FAULT_Y                     7

UCHAR szHiberDebug[] = "debug";
UCHAR szHiberFileName[] = "\\hiberfil.sys";

//
// HiberFile Compression Related definnes
//

#define PAGE_MASK   (PAGE_SIZE - 1)
#define PAGE_PAGES(n)   (((n) + PAGE_MASK) >> PAGE_SHIFT)

//
// The size of the buffer for compressed data

#define COMPRESSION_BUFFER_SIZE     64 << PAGE_SHIFT

//

#define MAX_COMPRESSION_BUFFER_EXTRA_PAGES \
    PAGE_PAGES (PAGE_MASK + 2*XPRESS_HEADER_SIZE)
#define MAX_COMPRESSION_BUFFER_EXTRA_SIZE \
    (MAX_COMPRESSION_BUFFER_EXTRA_PAGES << PAGE_SHIFT)

#define LZNT1_COMPRESSION_BUFFER_PAGES  16
#define LZNT1_COMPRESSION_BUFFER_SIZE \
    (LZNT1_COMPRESSION_BUFFER_PAGES << PAGE_SHIFT)

#define XPRESS_COMPRESSION_BUFFER_PAGES \
    PAGE_PAGES (XPRESS_MAX_SIZE + MAX_COMPRESSION_BUFFER_EXTRA_SIZE)
#define XPRESS_COMPRESSION_BUFFER_SIZE \
    (XPRESS_COMPRESSION_BUFFER_PAGES << PAGE_SHIFT)

#define MAX_COMPRESSION_BUFFER_PAGES \
    max (LZNT1_COMPRESSION_BUFFER_PAGES, XPRESS_COMPRESSION_BUFFER_PAGES)
#define MAX_COMPRESSION_BUFFER_SIZE \
    (MAX_COMPRESSION_BUFFER_PAGES << PAGE_SHIFT)


// Buffer to store decoded data
typedef struct {
    PUCHAR DataPtr, PreallocatedDataBuffer;
    LONG   DataSize;

    struct {
        struct {
            LONG Size;
            ULONG Checksum;
        } Compressed, Uncompressed;

        LONG XpressEncoded;
    } Header;

    LONG DelayedCnt;      // # of delayed pages
    ULONG DelayedChecksum;    // last checksum value
    ULONG DelayedBadChecksum;

    struct {
        PUCHAR DestVa;  // delayed DestVa
        PFN_NUMBER DestPage;// delayed page number
        ULONG  RangeCheck;  // last range checksum
        LONG   Flags;   // 1 = clear checksum, 2 = compare checksum
    } Delayed[XPRESS_MAX_PAGES];
} DECOMPRESSED_BLOCK, *PDECOMPRESSED_BLOCK;

typedef struct {
    struct {
        PUCHAR Beg;
        PUCHAR End;
    } Current, Buffer, Aligned;
    PFN_NUMBER FilePage;
    BOOLEAN    NeedSeek;
} COMPRESSED_BUFFER, *PCOMPRESSED_BUFFER;

#define HIBER_PERF_STATS 0

//
// Internal prototypes
//

#if !defined (HIBER_DEBUG)
#define CHECK_ERROR(a,b)    if(a) { *Information = __LINE__; return b; }
#define DBGOUT(_x_)
#else
#define CHECK_ERROR(a,b) if(a) {HbPrintMsg(b);HbPrint(TEXT("\r\n")); *Information = __LINE__; HbPause(); return b; }
#define DBGOUT(_x_) BlPrint _x_
#endif

ULONG
HbRestoreFile (
    IN PULONG       Information,
    OUT OPTIONAL PCHAR       *BadLinkName
    );

VOID
HbPrint (
    IN PTCHAR   str
    );

BOOLEAN
HbReadNextCompressedPageLZNT1 (
    PUCHAR DestVa,
    PCOMPRESSED_BUFFER CompressedBuffer
    );

BOOLEAN
HbReadNextCompressedChunkLZNT1 (
    PUCHAR DestVa,
    PCOMPRESSED_BUFFER CompressedBuffer
    );

BOOLEAN
HbReadNextCompressedPages (
    LONG BytesNeeded,
    PCOMPRESSED_BUFFER CompressedBuffer
    );

BOOLEAN
HbReadNextCompressedBlock (
    PDECOMPRESSED_BLOCK Block,
    PCOMPRESSED_BUFFER CompressedBuffer
    );

BOOLEAN
HbReadDelayedBlock (
    BOOLEAN ForceDecoding,
    PFN_NUMBER DestPage,
    ULONG RangeCheck,
    PDECOMPRESSED_BLOCK Block,
    PCOMPRESSED_BUFFER CompressedBuffer
    );

BOOLEAN
HbReadNextCompressedBlockHeader (
    PDECOMPRESSED_BLOCK Block,
    PCOMPRESSED_BUFFER CompressedBuffer
    );

ULONG
BlHiberRestore (
    IN ULONG DriveId,
    OUT PCHAR *BadLinkName
    );

BOOLEAN
HbReadNextCompressedChunk (
    PUCHAR DestVa,
    PPFN_NUMBER FilePage,
    PUCHAR CompressBuffer,
    PULONG DataOffset,
    PULONG BufferOffset,
    ULONG MaxOffset
    );


#if defined (HIBER_DEBUG) || HIBER_PERF_STATS

// HIBER_DEBUG bit mask:
//  2 - general bogosity
//  4 - remap trace


VOID HbFlowControl(VOID)
{
    UCHAR c;
    ULONG count;

    if (ArcGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
        ArcRead(ARC_CONSOLE_INPUT, &c, 1, &count);
        if (c == 'S' - 0x40) {
            ArcRead(ARC_CONSOLE_INPUT, &c, 1, &count);
        }
    }
}

VOID HbPause(VOID)
{
    UCHAR c;
    ULONG count;

#if defined(ENABLE_LOADER_DEBUG)
    DbgBreakPoint();
#else
    HbPrint(TEXT("Press any key to continue . . ."));
    ArcRead(ARC_CONSOLE_INPUT, &c, 1, &count);
    HbPrint(TEXT("\r\n"));
#endif
}

VOID HbPrintNum(ULONG n)
{
    TCHAR buf[9];

    _stprintf(buf, TEXT("%ld"), n);
    HbPrint(buf);
    HbFlowControl();
}

VOID HbPrintHex(ULONG n)
{
    TCHAR buf[11];

    _stprintf(buf, TEXT("0x%08lX"), n);
    HbPrint(buf);
    HbFlowControl();
}

#define SHOWNUM(x) ((void) (HbPrint(#x TEXT(" = ")), HbPrintNum((ULONG) (x)), HbPrint(TEXT("\r\n"))))
#define SHOWHEX(x) ((void) (HbPrint(#x TEXT(" = ")), HbPrintHex((ULONG) (x)), HbPrint(TEXT("\r\n"))))

#endif // HIBER_DEBUG

#if !defined(i386) && !defined(_ALPHA_)
ULONG
HbSimpleCheck (
    IN ULONG                PartialSum,
    IN PVOID                SourceVa,
    IN ULONG                Length
    );
#else

// Use the TCP/IP Check Sum routine if available

ULONG
tcpxsum(
   IN ULONG cksum,
   IN PUCHAR buf,
   IN ULONG len
   );

#define HbSimpleCheck(a,b,c) tcpxsum(a,(PUCHAR)b,c)
#endif

//
// The macros below helps to access 64-bit structures of Amd64 from
// their 32-bit definitions in loader. If the hiber image is not
// for Amd64, these macros simply reference structure fields directly. 
//

//
// Define macros to read a field in a structure. 
// 
// READ_FIELD (struct_type, struct_ptr, field, field_type)
//
// Areguments:
//
//     struct_type - type of the structure
//
//     struct_ptr  - base address of the structure
//
//     field       - field name 
//
//     field_type  - data type of the field
//

#if defined(_X86_) 

#define READ_FIELD(struct_type, struct_ptr, field, field_type) \
           (BlAmd64UseLongMode ? \
           *((field_type *)((ULONG_PTR)struct_ptr + \
           BlAmd64FieldOffset_##struct_type(FIELD_OFFSET(struct_type, field)))) : \
           (field_type)(((struct_type *)(struct_ptr))->field))

#else 

#define READ_FIELD(struct_type, struct_ptr, field, field_type) \
           (field_type)(((struct_type *)(struct_ptr))->field)

#define WRITE_FIELD(struct_type, struct_ptr, field, field_type, data) \
               (((struct_type *)(struct_ptr))->field) = (field_type)data;
#endif

#define READ_FIELD_UCHAR(struct_type, struct_ptr, field)  \
             READ_FIELD(struct_type, struct_ptr, field, UCHAR)

#define READ_FIELD_ULONG(struct_type, struct_ptr, field)  \
             READ_FIELD(struct_type, struct_ptr, field, ULONG) \

#define READ_FIELD_ULONG64(struct_type, struct_ptr, field)  \
             READ_FIELD(struct_type, struct_ptr, field, ULONG64)

//
// Here we assume the high dword of a 64-bit pfn on Amd64 is zero. 
// Otherwise hibernation should be disabled.
//

#define READ_FIELD_PFN_NUMBER(struct_type, struct_ptr, field)  \
             READ_FIELD(struct_type, struct_ptr, field, PFN_NUMBER) \

//
// Define macros to write a field in a structure. 
// 
// WRITE_FIELD (struct_type, struct_ptr, field, field_type, data)
//
// Areguments:
//
//     struct_type - type of the structure
//
//     struct_ptr  - base address of the structure
//
//     field       - field name 
//
//     field_type  - data type of the field
//
//     data        - value to be set to the field
//

#if defined(_X86_) 

#define WRITE_FIELD(struct_type, struct_ptr, field, field_type, data) \
           if(BlAmd64UseLongMode) {  \
               *((field_type *)((ULONG_PTR)struct_ptr + \
                BlAmd64FieldOffset_##struct_type(FIELD_OFFSET(struct_type, field)))) = (field_type)data; \
           } else { \
               (((struct_type *)(struct_ptr))->field) = (field_type)data; \
           }

#else 

#define WRITE_FIELD(struct_type, struct_ptr, field, field_type, data) \
               (((struct_type *)(struct_ptr))->field) = (field_type)data;
#endif


#define WRITE_FIELD_ULONG(struct_type, struct_ptr, field, data)  \
             WRITE_FIELD(struct_type, struct_ptr, field, ULONG, data)


//
// Define macros to read a field of a element in a structure array.
// 
// READ_ELEMENT_FIELD(struct_type, array, index, field, field_type)
//
// Areguments:
//
//     struct_type - type of the structure
//
//     array       - base address of the array
//
//     index       - index of the element
//
//     field       - field name 
//
//     field_type  - data type of the field
//


#if defined(_X86_) 

#define ELEMENT_OFFSET(type, index) \
           (BlAmd64UseLongMode ? BlAmd64ElementOffset_##type(index): \
           (ULONG)(&(((type *)0)[index])))

#define READ_ELEMENT_FIELD(struct_type, array, index, field, field_type) \
             READ_FIELD(struct_type, ((PUCHAR)array + ELEMENT_OFFSET(struct_type, index)), field, field_type)  

#else 

#define READ_ELEMENT_FIELD(struct_type, array, index, field, field_type) \
             (field_type)(((struct_type *)array)[index].field)

#endif

#define READ_ELEMENT_FIELD_ULONG(struct_type, array, index, field) \
              READ_ELEMENT_FIELD(struct_type, array, index, field, ULONG)

//
// Here we assume the high dword of a 64-bit pfn on Amd64 is zero. 
// Otherwise hibernation should be disabled.
//

#define READ_ELEMENT_FIELD_PFN_NUMBER(struct_type, array, index, field) \
              READ_ELEMENT_FIELD(struct_type, array, index, field, PFN_NUMBER)

VOID
HbReadPage (
    IN PFN_NUMBER PageNo,
    IN PUCHAR Buffer
    );

VOID
HbSetImageSignature (
    IN ULONG    NewSignature
    );

VOID
HbPrint (
    IN PTCHAR   str
    )
{
    ULONG   Junk;

    ArcWrite (
        BlConsoleOutDeviceId,
        str,
        (ULONG)_tcslen(str)*sizeof(TCHAR),
        &Junk
        );
}

VOID HbPrintChar (_TUCHAR chr)
{
      ULONG Junk;

      ArcWrite(
               BlConsoleOutDeviceId,
               &chr,
               sizeof(_TUCHAR),
               &Junk
               );
}

VOID
HbPrintMsg (
    IN ULONG  MsgNo
    )
{
    PTCHAR  Str;

    Str = BlFindMessage(MsgNo);
    if (Str) {
        HbPrint (Str);
    }
}

VOID
HbScreen (
    IN ULONG Screen
    )
{
#if defined(HIBER_DEBUG)
    HbPrint(TEXT("\r\n"));
    HbPause();
#endif

    HbCurrentScreen = Screen;
    BlSetInverseMode (FALSE);
    BlPositionCursor (1, 1);
    BlClearToEndOfScreen();
    BlPositionCursor (1, 3);
    HbPrintMsg(Screen);
}

ULONG
HbSelection (
    ULONG   x,
    ULONG   y,
    PULONG  Sel,
    ULONG   Debug
    )
{
    ULONG   CurSel, MaxSel;
    ULONG   i;
    UCHAR   Key;
    PUCHAR  pDebug;

    for (MaxSel=0; Sel[MaxSel]; MaxSel++) ;
    MaxSel -= Debug;
    pDebug = szHiberDebug;

#if DBG
    MaxSel += Debug;
    Debug = 0;
#endif

    CurSel = 0;
    for (; ;) {
        //
        // Draw selections
        //

        for (i=0; i < MaxSel; i++) {
            BlPositionCursor (x, y+i);
            BlSetInverseMode ((BOOLEAN) (CurSel == i) );
            HbPrintMsg(Sel[i]);
        }

        //
        // Get a key
        //

        ArcRead(ARC_CONSOLE_INPUT, &Key, sizeof(Key), &i);
        if (Key == ASCI_CSI_IN) {
            ArcRead(ARC_CONSOLE_INPUT, &Key, sizeof(Key), &i);
            switch (Key) {
                case 'A':
                    //
                    // Cursor up
                    //
                    CurSel -= 1;
                    if (CurSel >= MaxSel) {
                        CurSel = MaxSel-1;
                    }
                    break;

                case 'B':
                    //
                    // Cursor down
                    //
                    CurSel += 1;
                    if (CurSel >= MaxSel) {
                        CurSel = 0;
                    }
                    break;
            }
        } else {
            if (Key == *pDebug) {
                pDebug++;
                if (!*pDebug) {
                    MaxSel += Debug;
                    Debug = 0;
                }
            } else {
                pDebug = szHiberDebug;
            }

            switch (Key) {
                case ASCII_LF:
                case ASCII_CR:
                    BlSetInverseMode (FALSE);
                    BlPositionCursor (1, 2);
                    BlClearToEndOfScreen ();
                    if (Sel[CurSel] == HIBER_DEBUG_BREAK_ON_WAKE) {
                        HiberBreakOnWake = TRUE;
                    }

                    return CurSel;
            }
        }
    }
}


VOID
HbCheckForPause (
    VOID
    )
{
    ULONG       uSel = 0;
    UCHAR       Key;
    ULONG       Sel[4];
    BOOLEAN     bPaused = FALSE;

    //
    // Check for space bar
    //

    if (ArcGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
        ArcRead(ARC_CONSOLE_INPUT, &Key, sizeof(Key), &uSel);

        switch (Key) {
            // space bar pressed
            case ' ':
                bPaused = TRUE;
                break;

            // user pressed F5/F8 key
            case ASCI_CSI_IN:
                ArcRead(ARC_CONSOLE_INPUT, &Key, sizeof(Key), &uSel);

                if(Key == 'O') {
                    ArcRead(ARC_CONSOLE_INPUT, &Key, sizeof(Key), &uSel);
                    bPaused = (Key == 'r' || Key == 't');
                }

                break;

            default:
                bPaused = FALSE;
                break;
        }

        if (bPaused) {
            Sel[0] = HIBER_CONTINUE;
            Sel[1] = HIBER_CANCEL;
            Sel[2] = HIBER_DEBUG_BREAK_ON_WAKE;
            Sel[3] = 0;

            HbScreen(HIBER_PAUSE);

            uSel = HbSelection (PAUSE_X, PAUSE_Y, Sel, 1);

            if (uSel == 1) {
                HiberIoError = TRUE;
                HiberAbort = TRUE;
                return ;
            } else {
                BlSetInverseMode(FALSE);

                //
                // restore hiber progress screen
                //
                BlOutputStartupMsg(BL_MSG_RESUMING_WINDOWS);
                BlOutputTrailerMsg(BL_ADVANCED_BOOT_MESSAGE);
            }
        }
    }
}


ULONG
BlHiberRestore (
    IN ULONG DriveId,
    OUT PCHAR *BadLinkName
    )
/*++

Routine Description:

    Checks DriveId for a valid hiberfile.sys and if found start the
    restoration procedure

--*/
{
    extern BOOLEAN  BlOutputDots;
    NTSTATUS        Status;
    ULONG           Msg = 0;
    ULONG           Information;
    ULONG           Sel[2];
    BOOLEAN         bDots = BlOutputDots;

    //
    // If restore was aborted once, don't bother
    //

#if defined (HIBER_DEBUG)
    HbPrint(TEXT("BlHiberRestore\r\n"));
#endif


    if (HiberAbort) {
        return ESUCCESS;
    }

    //
    // Get the hiber image. If not present, done.
    //

    Status = BlOpen (DriveId, (PCHAR)szHiberFileName, ArcOpenReadWrite, &HiberFile);
    if (Status != ESUCCESS) {
#if defined (HIBER_DEBUG)
        HbPrint(TEXT("No hiber image file.\r\n"));
#endif
        return ESUCCESS;
    }

    //
    // Restore the hiber image
    //
    BlOutputDots = TRUE;
    //
    // Set the global flag to allow blmemory.c to grab from the right
    // part of the buffer
    //
    BlRestoring=TRUE;

    Msg = HbRestoreFile (&Information, BadLinkName);

    BlOutputDots = bDots;

    if (Msg) {
        BlSetInverseMode (FALSE);

        if (!HiberAbort) {
            HbScreen(HIBER_ERROR);
            HbPrintMsg(Msg);
            Sel[0] = HIBER_CANCEL;
            Sel[1] = 0;
            HbSelection (FAULT_X, FAULT_Y, Sel, 0);
        }
        HbSetImageSignature (0);
    }

    BlClose (HiberFile);
    BlRestoring=FALSE;
    return Msg ? EAGAIN : ESUCCESS;
}


#if !defined(i386) && !defined(_ALPHA_)
ULONG
HbSimpleCheck (
    IN ULONG                PartialSum,
    IN PVOID                SourceVa,
    IN ULONG                Length
    )
/*++

Routine Description:

    Computes a checksum for the supplied virtual address and length

    This function comes from Dr. Dobbs Journal, May 1992

--*/
{

    PUSHORT     Source;

    Source = (PUSHORT) SourceVa;
    Length = Length / 2;

    while (Length--) {
        PartialSum += *Source++;
        PartialSum = (PartialSum >> 16) + (PartialSum & 0xFFFF);
    }

    return PartialSum;
}
#endif // i386


VOID
HbReadPage (
    IN PFN_NUMBER PageNo,
    IN PUCHAR Buffer
    )
/*++

Routine Description:

    This function reads the specified page from the hibernation file

Arguments:

    PageNo      - Page number to read

    Buffer      - Buffer to read the data

Return Value:

    On success Buffer, else HbIoError set to TRUE

--*/
{
    ULONG           Status;
    ULONG           Count;
    LARGE_INTEGER   li;

    li.QuadPart = (ULONGLONG) PageNo << PAGE_SHIFT;
    Status = BlSeek (HiberFile, &li, SeekAbsolute);
    if (Status != ESUCCESS) {
        HiberIoError = TRUE;
    }

    Status = BlRead (HiberFile, Buffer, PAGE_SIZE, &Count);
    if (Status != ESUCCESS) {
        HiberIoError = TRUE;
    }
}


BOOLEAN
HbReadNextCompressedPages (
    LONG BytesNeeded,
    PCOMPRESSED_BUFFER CompressedBuffer
    )
/*++

Routine Description:

    This routine makes sure that BytesNeeded bytes are available
    in CompressedBuffer and brings in more pages from Hiber file
    if necessary.

    All reads from the Hiber file occurr at the file's
    current offset forcing compressed pages to be read
    in a continuous fashion without extraneous file seeks.

Arguments:

   BytesNeeded      - Number of bytes that must be present in CompressedBuffer
   CompressedBuffer - Descriptor of data already brought in

Return Value:

   TRUE if the operation is successful, FALSE otherwise.

--*/
{
    LONG BytesLeft;
    LONG BytesRequested;
    ULONG Status;
    LONG MaxBytes;

    // Obtain number of bytes left in buffer
    BytesLeft = (LONG) (CompressedBuffer->Current.End - CompressedBuffer->Current.Beg);

    // Obtain number of bytes that are needed but not available
    BytesNeeded -= BytesLeft;

    // Preserve amount of bytes caller needs (BytesNeeded may be changed later)
    BytesRequested = BytesNeeded;

    // Do we need to read more?
    if (BytesNeeded <= 0) {
        // No, do nothing
        return(TRUE);
    }

    // Align BytesNeeded on page boundary
    BytesNeeded = (BytesNeeded + PAGE_MASK) & ~PAGE_MASK;

    // Copy left bytes to the beginning of aligned buffer retaining page alignment
    if (BytesLeft == 0) {
        CompressedBuffer->Current.Beg = CompressedBuffer->Current.End = CompressedBuffer->Aligned.Beg;
    } else {
        LONG BytesBeforeBuffer = (LONG)(CompressedBuffer->Aligned.Beg - CompressedBuffer->Buffer.Beg) & ~PAGE_MASK;
        LONG BytesLeftAligned = (BytesLeft + PAGE_MASK) & ~PAGE_MASK;
        LONG BytesToCopy;
        PUCHAR Dst, Src;

        // Find out how many pages we may keep before aligned buffer
        if (BytesBeforeBuffer >= BytesLeftAligned) {
            BytesBeforeBuffer = BytesLeftAligned;
        }

        // Avoid misaligned data accesses during copy
        BytesToCopy = (BytesLeft + 63) & ~63;

        Dst = CompressedBuffer->Aligned.Beg + BytesLeftAligned - BytesBeforeBuffer - BytesToCopy;
        Src = CompressedBuffer->Current.End - BytesToCopy;

        if (Dst != Src) {
            RtlMoveMemory (Dst, Src, BytesToCopy);
            BytesLeftAligned = (LONG) (Dst - Src);
            CompressedBuffer->Current.Beg += BytesLeftAligned;
            CompressedBuffer->Current.End += BytesLeftAligned;
        }
    }

    //
    // Increase the number of bytes read to fill our buffer up to the next
    // 64K boundary.
    //
    MaxBytes = (LONG)((((ULONG_PTR)CompressedBuffer->Current.End + 0x10000) & 0xffff) - (ULONG_PTR)CompressedBuffer->Current.End);
    if (MaxBytes > CompressedBuffer->Buffer.End - CompressedBuffer->Current.End) {
        MaxBytes = (LONG)(CompressedBuffer->Buffer.End - CompressedBuffer->Current.End);
    }
    if (MaxBytes > BytesNeeded) {
        BytesNeeded = MaxBytes;
    }


#if 0
    // for debugging only
    if (0x10000 - (((LONG) CompressedBuffer->Current.End) & 0xffff) < BytesNeeded) {
        BlPrint (("Current.Beg = %p, Current.End = %p, Current.End2 = %p\n",
                  CompressedBuffer->Current.Beg,
                  CompressedBuffer->Current.End,
                  CompressedBuffer->Current.End + BytesNeeded
                 ));
    }
#endif

    // Make sure we have enough space
    if (BytesNeeded > CompressedBuffer->Buffer.End - CompressedBuffer->Current.End) {
        // Too many bytes to read -- should never happen, but just in case...
        DBGOUT (("Too many bytes to read -- corrupted data?\n"));
        return(FALSE);
    }

    // Issue seek if necessary
    if (CompressedBuffer->NeedSeek) {
        LARGE_INTEGER li;
        li.QuadPart = (ULONGLONG) CompressedBuffer->FilePage << PAGE_SHIFT;
        Status = BlSeek (HiberFile, &li, SeekAbsolute);
        if (Status != ESUCCESS) {
            DBGOUT (("Seek to 0x%x error 0x%x\n", CompressedBuffer->FilePage, Status));
            HiberIoError = TRUE;
            return(FALSE);
        }
        CompressedBuffer->NeedSeek = FALSE;
    }

    // Read in stuff from the Hiber file into the available buffer space
    Status = BlRead (HiberFile, CompressedBuffer->Current.End, BytesNeeded, (PULONG)&BytesNeeded);

    // Check for I/O errors...
    if (Status != ESUCCESS || ((ULONG)BytesNeeded & PAGE_MASK) != 0 || (BytesNeeded < BytesRequested)) {
        // I/O Error - FAIL.
        DBGOUT (("Read error: Status = 0x%x, ReadBytes = 0x%x, Requested = 0x%x\n", Status, BytesNeeded, BytesRequested));
        HiberIoError = TRUE;
        return(FALSE);
    }

    // I/O was good - recalculate buffer offsets based on how much
    // stuff was actually read in

    CompressedBuffer->Current.End += (ULONG)BytesNeeded;
    CompressedBuffer->FilePage += ((ULONG)BytesNeeded >> PAGE_SHIFT);

    return(TRUE);
}


BOOLEAN
HbReadNextCompressedBlockHeader (
    PDECOMPRESSED_BLOCK Block,
    PCOMPRESSED_BUFFER CompressedBuffer
    )
/*++

Routine Description:

   Read next compressed block header if it's Xpress compression.

Arguments:
   Block            - Descriptor of compressed data block

   CompressedBuffer - Descriptor of data already brought in


Return Value:

   TRUE if block is not Xpress block at all or valid Xpress block, FALSE otherwise

--*/
{
    PUCHAR Buffer;
    LONG CompressedSize;         // they all must be signed -- do not change to ULONG
    LONG UncompressedSize;
    ULONG PackedSizes;

    // First make sure next compressed data block header is available
    if (!HbReadNextCompressedPages (XPRESS_HEADER_SIZE, CompressedBuffer)) {
        // I/O error or bad header -- FAIL
        return(FALSE);
    }


    // Set pointer to the beginning of buffer
    Buffer = CompressedBuffer->Current.Beg;

    // Check header magic
    Block->Header.XpressEncoded = (RtlCompareMemory (Buffer, XPRESS_HEADER_STRING, XPRESS_HEADER_STRING_SIZE) == XPRESS_HEADER_STRING_SIZE);

    if (!Block->Header.XpressEncoded) {
        // Not Xpress -- return OK
        return(TRUE);
    }

    // Skip magic string -- we will not need it anymore
    Buffer += XPRESS_HEADER_STRING_SIZE;

    // Read sizes of compressed and uncompressed data
    PackedSizes = Buffer[0] + (Buffer[1] << 8) + (Buffer[2] << 16) + (Buffer[3] << 24);
    CompressedSize = (LONG) (PackedSizes >> 10) + 1;
    UncompressedSize = ((LONG) (PackedSizes & 1023) + 1) << PAGE_SHIFT;

    Block->Header.Compressed.Size = CompressedSize;
    Block->Header.Uncompressed.Size = UncompressedSize;

    // Read checksums
    Block->Header.Uncompressed.Checksum = Buffer[4] + (Buffer[5] << 8);
    Block->Header.Compressed.Checksum = Buffer[6] + (Buffer[7] << 8);

    // Clear space occupied by compressed checksum
    Buffer[6] = Buffer[7] = 0;

    // Make sure sizes are in correct range
    if (UncompressedSize > XPRESS_MAX_SIZE ||
        CompressedSize > UncompressedSize ||
        CompressedSize == 0 ||
        UncompressedSize == 0) {
        // broken input data -- do not even try to decompress

        DBGOUT (("Corrupted header: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                 Buffer[0], Buffer[1], Buffer[2], Buffer[3], Buffer[4], Buffer[5], Buffer[6], Buffer[7]));
        DBGOUT (("CompressedSize = %d, UncompressedSize = %d\n", CompressedSize, UncompressedSize));

        return(FALSE);
    }

    // Xpress header and it looks OK so far
    return(TRUE);
}


BOOLEAN
HbReadNextCompressedBlock (
    PDECOMPRESSED_BLOCK Block,
    PCOMPRESSED_BUFFER CompressedBuffer
    )
/*++

Routine Description:

   Reads and decompresses the next compressed chunk from the Hiber file
   and stores it in a designated region of virtual memory.

   Since no master data structure exists within the Hiber file to identify
   the location of all of the compression chunks, this routine operates
   by reading sections of the Hiber file into a compression buffer
   and extracting chunks from that buffer.

   Chunks are extracted by determining if a chunk is completely present in the buffer
   using the RtlDescribeChunk API. If the chunk is not completely present,
   more of the Hiber file is read into the buffer until the chunk can
   be extracted.

   All reads from the Hiber file occurr at its current offset, forcing
   compressed chunks to be read in a continous fashion with no extraneous
   seeks.

Arguments:

   Block            - Descriptor of compressed data block
   CompressedBuffer - Descriptor of data already brought in

Return Value:

   TRUE if a chunk has been succesfully extracted and decompressed, FALSE otherwise.

--*/
{
    PUCHAR Buffer;
    LONG CompressedSize;         // they all must be signed -- do not change to ULONG
    LONG AlignedCompressedSize;
    LONG UncompressedSize;


    // First make sure next compressed data block header is available
    if (!HbReadNextCompressedBlockHeader (Block, CompressedBuffer)) {
        // I/O error -- FAIL
        return(FALSE);
    }

    // It must be Xpress
    if (!Block->Header.XpressEncoded) {
#ifdef HIBER_DEBUG
        // Set pointer to the beginning of buffer
        Buffer = CompressedBuffer->Current.Beg;

        // wrong magic -- corrupted data
        DBGOUT (("Corrupted header: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                 Buffer[0], Buffer[1], Buffer[2], Buffer[3], Buffer[4], Buffer[5], Buffer[6], Buffer[7]));
#endif /* HIBER_DEBUG */

        return(FALSE);
    }

    // Read sizes
    UncompressedSize = Block->Header.Uncompressed.Size;
    CompressedSize = Block->Header.Compressed.Size;

    // If not enough space supplied use preallocated buffer
    if (UncompressedSize != Block->DataSize) {
        Block->DataSize = UncompressedSize;
        Block->DataPtr = Block->PreallocatedDataBuffer;
    }

    // Evaluate aligned size of compressed data
    AlignedCompressedSize = (CompressedSize + (XPRESS_ALIGNMENT - 1)) & ~(XPRESS_ALIGNMENT - 1);

    // Make sure we have all compressed data and the header in buffer
    if (!HbReadNextCompressedPages (AlignedCompressedSize + XPRESS_HEADER_SIZE, CompressedBuffer)) {
        // I/O error -- FAIL
        return(FALSE);
    }

    // Set pointer to the beginning of buffer
    Buffer = CompressedBuffer->Current.Beg;

    // We will use some bytes out of buffer now -- reflect this fact
    CompressedBuffer->Current.Beg += AlignedCompressedSize + XPRESS_HEADER_SIZE;

    // evaluate and compare checksum of compressed data and header with written value
    if (Block->Header.Compressed.Checksum != 0) {
        ULONG Checksum;
        Checksum = HbSimpleCheck (0, Buffer, AlignedCompressedSize + XPRESS_HEADER_SIZE);
        if (((Checksum ^ Block->Header.Compressed.Checksum) & 0xffff) != 0) {
            DBGOUT (("Compressed data checksum mismatch (got %08lx, written %08lx)\n", Checksum, Block->Header.Compressed.Checksum));
            return(FALSE);
        }
    }

    // Was this buffer compressed at all?
    if (CompressedSize == UncompressedSize) {
        // Nope, do not decompress it -- set bounds and return OK
        Block->DataPtr = Buffer + XPRESS_HEADER_SIZE;
    } else {
        LONG DecodedSize;

        // Decompress the buffer
        DecodedSize = XpressDecode (NULL,
                                    Block->DataPtr,
                                    UncompressedSize,
                                    UncompressedSize,
                                    Buffer + XPRESS_HEADER_SIZE,
                                    CompressedSize);

        if (DecodedSize != UncompressedSize) {
            DBGOUT (("Decode error: DecodedSize = %d, UncompressedSize = %d\n", DecodedSize, UncompressedSize));
            return(FALSE);
        }
    }

#ifdef HIBER_DEBUG
    // evaluate and compare uncompressed data checksums (just to be sure)
    if (Block->Header.Uncompressed.Checksum != 0) {
        ULONG Checksum;
        Checksum = HbSimpleCheck (0, Block->DataPtr, UncompressedSize);
        if (((Checksum ^ Block->Header.Uncompressed.Checksum) & 0xffff) != 0) {
            DBGOUT (("Decoded data checksum mismatch (got %08lx, written %08lx)\n", Checksum, Block->Header.Uncompressed.Checksum));
            return(FALSE);
        }
    }