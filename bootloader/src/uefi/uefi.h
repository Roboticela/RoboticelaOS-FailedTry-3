/* robobootloader - minimal UEFI definitions (subset of the UEFI Specification 2.10)
 *
 * Only what Stage 01 needs: system table, boot/runtime services, text output,
 * GOP, file protocols, loaded image, and the ACPI configuration-table GUIDs.
 *
 * The boot-services and runtime-services tables are ABI-critical (the firmware
 * provides them, we only call through them): every field of the spec structs
 * must be present, in order, even if robobootloader never calls it. Everything
 * else is trimmed to what the bootloader uses.
 *
 * Reference: UEFI Specification Version 2.10.
 */
#ifndef ROBO_UEFI_H
#define ROBO_UEFI_H

#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ types */

typedef void       *EFI_HANDLE;
typedef uint64_t    UINTN;
typedef int64_t     INTN;
typedef uint8_t     UINT8;
typedef uint16_t    UINT16;
typedef uint32_t    UINT32;
typedef uint64_t    UINT64;
typedef int8_t      INT8;
typedef int16_t     INT16;
typedef int32_t     INT32;
typedef int64_t     INT64;
typedef uint16_t    CHAR16;
typedef char        CHAR8;
typedef void        VOID;
typedef UINT8       BOOLEAN;

#define TRUE    ((BOOLEAN)1)
#define FALSE   ((BOOLEAN)0)

typedef UINTN   EFI_STATUS;
typedef UINT64  EFI_LBA;
typedef UINT64  EFI_PHYSICAL_ADDRESS;
typedef UINT64  EFI_EVENT;
typedef UINT64  EFI_FILE_MODE;
typedef UINT64  EFI_FILE_ATTR;

#define EFIAPI  /* default x86-64 calling convention */

#define EFI_ERROR_MASK  0x8000000000000000ULL
#define EFIERR(a)       ((EFI_STATUS)(EFI_ERROR_MASK | (UINT64)(a)))

#define EFI_SUCCESS             0
#define EFI_LOAD_ERROR          EFIERR(1)
#define EFI_INVALID_PARAMETER   EFIERR(2)
#define EFI_UNSUPPORTED         EFIERR(3)
#define EFI_BAD_BUFFER_SIZE     EFIERR(4)
#define EFI_BUFFER_TOO_SMALL    EFIERR(5)
#define EFI_NOT_READY           EFIERR(6)
#define EFI_DEVICE_ERROR        EFIERR(7)
#define EFI_WRITE_PROTECTED     EFIERR(8)
#define EFI_OUT_OF_RESOURCES    EFIERR(9)
#define EFI_VOLUME_CORRUPTED    EFIERR(10)
#define EFI_NOT_FOUND           EFIERR(14)
#define EFI_NO_MEDIA            EFIERR(18)
#define EFI_MEDIA_CHANGED       EFIERR(19)
#define EFI_INVALID_LANGUAGE    EFIERR(32)
#define EFI_SECURITY_VIOLATION  EFIERR(26)

#define EFI_ERROR(s)    (((INTN)(s)) < 0)

typedef struct {
    UINT32  Data1;
    UINT16  Data2;
    UINT16  Data3;
    UINT8   Data4[8];
} EFI_GUID;

#define EFI_GUID_INIT(a,b,c,d0,d1,d2,d3,d4,d5,d6,d7) \
    { (a), (b), (c), { (d0),(d1),(d2),(d3),(d4),(d5),(d6),(d7) } }

typedef struct {
    UINT64  Signature;
    UINT32  Revision;
    UINT32  HeaderSize;
    UINT32  CRC32;
    UINT32  Reserved;
} EFI_TABLE_HEADER;

typedef struct {
    UINT16  Year;
    UINT8   Month;
    UINT8   Day;
    UINT8   Hour;
    UINT8   Minute;
    UINT8   Second;
    UINT8   Pad1;
    UINT32  Nanosecond;
    INT16   TimeZone;
    UINT8   Daylight;
    UINT8   Pad2;
} EFI_TIME;

typedef struct {
    UINT32  Resolution;
    UINT32  Accuracy;
    UINT8   SetsToZero;
} EFI_TIME_CAPABILITIES;

typedef enum {
    TimerCancel,
    TimerPeriodic,
    TimerRelative
} EFI_TIMER_DELAY;

typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

typedef enum {
    EfiResetCold,
    EfiResetWarm,
    EfiResetShutdown,
    EfiResetPlatformSpecific
} EFI_RESET_TYPE;

#define EFI_PAGE_SIZE           0x1000u
#define EFI_PAGE_SHIFT          12
#define EFI_SIZE_TO_PAGES(a)    (((a) >> EFI_PAGE_SHIFT) + \
                                 (((a) & (EFI_PAGE_SIZE - 1)) ? 1 : 0))

#define EFI_MEMORY_UC           0x0000000000000001ULL
#define EFI_MEMORY_WC           0x0000000000000002ULL
#define EFI_MEMORY_WT           0x0000000000000004ULL
#define EFI_MEMORY_WB           0x0000000000000008ULL
#define EFI_MEMORY_UCE          0x0000000000000010ULL
#define EFI_MEMORY_WP           0x0000000000001000ULL
#define EFI_MEMORY_RP           0x0000000000002000ULL
#define EFI_MEMORY_XP           0x0000000000004000ULL
#define EFI_MEMORY_RO           0x0000000000020000ULL
#define EFI_MEMORY_NV           0x0000000000008000ULL
#define EFI_MEMORY_MORE_RELIABLE 0x0000000000010000ULL
#define EFI_MEMORY_RUNTIME      0x8000000000000000ULL

typedef struct {
    UINT32  Type;               /* EFI_MEMORY_TYPE */
    UINT32  Pad;
    UINT64  PhysicalStart;
    UINT64  VirtualStart;
    UINT64  NumberOfPages;
    UINT64  Attribute;
} EFI_MEMORY_DESCRIPTOR;

typedef struct {
    EFI_GUID    VendorGuid;
    VOID       *VendorTable;
} EFI_CONFIGURATION_TABLE;

/* ------------------------------------------------------------- event flags */

#define EVT_TIMER                           0x80000000u
#define EVT_RUNTIME                         0x40000000u
#define EVT_NOTIFY_WAIT                     0x00000100u
#define EVT_NOTIFY_SIGNAL                   0x00000200u

typedef VOID (*EFI_EVENT_NOTIFY)(EFI_EVENT Event, VOID *Context);

typedef struct {
    UINT64  Event;
    EFI_STATUS Status;
    UINTN   BufferSize;
    VOID   *Buffer;
} EFI_FILE_IO_TOKEN;

typedef struct {
    UINT32  Type;
    UINT32  Length;
    UINT64  Data[1];
} EFI_CAPSULE_HEADER; /* minimal; full layout not needed by Stage 01 */

/* --------------------------------------------------- service typedefs */

typedef EFI_STATUS (*EFI_TEXT_RESET)(VOID *This, BOOLEAN ExtendedVerification);
typedef EFI_STATUS (*EFI_TEXT_STRING)(VOID *This, CHAR16 *String);
typedef EFI_STATUS (*EFI_TEXT_TEST_STRING)(VOID *This, CHAR16 *String);
typedef EFI_STATUS (*EFI_TEXT_QUERY_MODE)(VOID *This, UINTN *ModeNumber);
typedef EFI_STATUS (*EFI_TEXT_SET_MODE)(VOID *This, UINTN ModeNumber);
typedef EFI_STATUS (*EFI_TEXT_ENABLE_CURSOR)(VOID *This, BOOLEAN Enable);

typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
    UINT32  RedMask;
    UINT32  GreenMask;
    UINT32  BlueMask;
    UINT32  ReservedMask;
} EFI_PIXEL_BITMASK;

typedef struct {
    UINT32                      Version;
    UINT32                      HorizontalResolution;
    UINT32                      VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT   PixelFormat;
    EFI_PIXEL_BITMASK           PixelInformation;
    UINT32                      PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT8   Blue;
    UINT8   Green;
    UINT8   Red;
    UINT8   Reserved;
} EFI_GRAPHICS_OUTPUT_BLT_PIXEL;

typedef EFI_STATUS (*EFI_GOP_QUERY_MODE)(VOID *This, UINT32 ModeNumber,
                                         UINTN *SizeOfInfo,
                                         EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info);
typedef EFI_STATUS (*EFI_GOP_SET_MODE)(VOID *This, UINT32 ModeNumber);
typedef EFI_STATUS (*EFI_GOP_BLT)(VOID *This,
                                  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer,
                                  UINT32 BltOperation, UINTN SourceX,
                                  UINTN SourceY, UINTN DestinationX,
                                  UINTN DestinationY, UINTN Width, UINTN Height,
                                  UINTN Delta);

typedef enum {
    EfiBltVideoFill,
    EfiBltVideoToBltBuffer,
    EfiBltBufferToVideo,
    EfiBltVideoToVideo,
    EfiBltOperationMax
} EFI_GRAPHICS_OUTPUT_BLT_OPERATION;

typedef EFI_STATUS (*EFI_FILE_OPEN)(VOID *This, VOID **NewHandle,
                                    CHAR16 *FileName, UINT64 OpenMode,
                                    UINT64 Attributes);
typedef EFI_STATUS (*EFI_FILE_CLOSE)(VOID *This);
typedef EFI_STATUS (*EFI_FILE_DELETE)(VOID *This);
typedef EFI_STATUS (*EFI_FILE_READ)(VOID *This, UINTN *BufferSize, VOID *Buffer);
typedef EFI_STATUS (*EFI_FILE_WRITE)(VOID *This, UINTN *BufferSize, VOID *Buffer);
typedef EFI_STATUS (*EFI_FILE_GET_POSITION)(VOID *This, UINT64 *Position);
typedef EFI_STATUS (*EFI_FILE_SET_POSITION)(VOID *This, UINT64 Position);
typedef EFI_STATUS (*EFI_FILE_GET_INFO)(VOID *This, EFI_GUID *InformationType,
                                        UINTN *BufferSize, VOID *Buffer);
typedef EFI_STATUS (*EFI_FILE_SET_INFO)(VOID *This, EFI_GUID *InformationType,
                                        UINTN BufferSize, VOID *Buffer);
typedef EFI_STATUS (*EFI_FILE_FLUSH)(VOID *This);
typedef EFI_STATUS (*EFI_FILE_OPEN_EX)(VOID *This, VOID **NewHandle,
                                       CHAR16 *FileName, UINT64 OpenMode,
                                       UINT64 Attributes, VOID *Token);
typedef EFI_STATUS (*EFI_FILE_READ_EX)(VOID *This, EFI_FILE_IO_TOKEN *Token);
typedef EFI_STATUS (*EFI_FILE_WRITE_EX)(VOID *This, EFI_FILE_IO_TOKEN *Token);
typedef EFI_STATUS (*EFI_FILE_FLUSH_EX)(VOID *This, EFI_FILE_IO_TOKEN *Token);

typedef EFI_STATUS (*EFI_VOLUME_OPEN_VOLUME)(VOID *This, VOID **Root);

typedef EFI_STATUS (*EFI_GET_TIME)(EFI_TIME *Time,
                                   EFI_TIME_CAPABILITIES *Capabilities);
typedef EFI_STATUS (*EFI_SET_TIME)(EFI_TIME *Time);
typedef EFI_STATUS (*EFI_GET_WAKEUP_TIME)(BOOLEAN *Enabled, BOOLEAN *Pending,
                                          EFI_TIME *Time);
typedef EFI_STATUS (*EFI_SET_WAKEUP_TIME)(BOOLEAN Enable, EFI_TIME *Time);
typedef EFI_STATUS (*EFI_SET_VIRTUAL_ADDRESS_MAP)(UINTN DescriptorSize,
                                                  UINT32 DescriptorVersion,
                                                  EFI_MEMORY_DESCRIPTOR *VirtualMap);
typedef EFI_STATUS (*EFI_CONVERT_POINTER)(UINTN DebugDisposition, VOID **Address);
typedef EFI_STATUS (*EFI_GET_VARIABLE)(CHAR16 *VariableName, EFI_GUID *VendorGuid,
                                       UINT32 *Attributes, UINTN *DataSize,
                                       VOID *Data);
typedef EFI_STATUS (*EFI_GET_NEXT_VARIABLE_NAME)(UINTN *VariableNameSize,
                                                 CHAR16 *VariableName,
                                                 EFI_GUID *VendorGuid);
typedef EFI_STATUS (*EFI_SET_VARIABLE)(CHAR16 *VariableName, EFI_GUID *VendorGuid,
                                       UINT32 Attributes, UINTN DataSize,
                                       VOID *Data);
typedef EFI_STATUS (*EFI_GET_NEXT_HIGH_MONOTONIC_COUNT)(UINT32 *HighCount);
typedef VOID (*EFI_RESET_SYSTEM)(EFI_RESET_TYPE ResetType, EFI_STATUS ResetStatus,
                                 UINTN DataSize, CHAR16 *ResetData);
typedef EFI_STATUS (*EFI_UPDATE_CAPSULE)(EFI_CAPSULE_HEADER **CapsuleHeaderArray,
                                         UINTN CapsuleCount,
                                         UINT64 ScatterGatherList);
typedef EFI_STATUS (*EFI_QUERY_CAPSULE_CAPABILITIES)(
    EFI_CAPSULE_HEADER **CapsuleHeaderArray, UINTN CapsuleCount,
    UINT64 *MaximumCapsuleSize, EFI_RESET_TYPE *ResetType);
typedef EFI_STATUS (*EFI_QUERY_VARIABLE_INFO)(UINT32 Attributes,
                                              UINT64 *MaximumVariableStorageSize,
                                              UINT64 *RemainingVariableStorageSize,
                                              UINT64 *MaximumVariableSize);

typedef VOID (*EFI_RAISE_TPL)(UINTN NewTpl);
typedef VOID (*EFI_RESTORE_TPL)(UINTN OldTpl);
typedef EFI_STATUS (*EFI_ALLOCATE_PAGES)(EFI_ALLOCATE_TYPE Type,
                                         EFI_MEMORY_TYPE MemoryType,
                                         UINTN Pages, UINT64 *Memory);
typedef EFI_STATUS (*EFI_FREE_PAGES)(UINT64 Memory, UINTN Pages);
typedef EFI_STATUS (*EFI_GET_MEMORY_MAP)(UINTN *MemoryMapSize,
                                         EFI_MEMORY_DESCRIPTOR *MemoryMap,
                                         UINTN *MapKey, UINTN *DescriptorSize,
                                         UINT32 *DescriptorVersion);
typedef EFI_STATUS (*EFI_ALLOCATE_POOL)(EFI_MEMORY_TYPE PoolType, UINTN Size,
                                        VOID **Buffer);
typedef EFI_STATUS (*EFI_FREE_POOL)(VOID *Buffer);
typedef EFI_STATUS (*EFI_CREATE_EVENT)(UINT32 Type, UINTN NotifyTpl,
                                       EFI_EVENT_NOTIFY NotifyFunction,
                                       VOID *NotifyContext, EFI_EVENT *Event);
typedef EFI_STATUS (*EFI_SET_TIMER)(EFI_EVENT Event, EFI_TIMER_DELAY Type,
                                    UINT64 TriggerTime);
typedef EFI_STATUS (*EFI_WAIT_FOR_EVENT)(UINTN NumberOfEvents,
                                         EFI_EVENT *Event, UINTN *Index);
typedef EFI_STATUS (*EFI_SIGNAL_EVENT)(EFI_EVENT Event);
typedef EFI_STATUS (*EFI_CLOSE_EVENT)(EFI_EVENT Event);
typedef EFI_STATUS (*EFI_CHECK_EVENT)(EFI_EVENT Event);
typedef EFI_STATUS (*EFI_INSTALL_PROTOCOL_INTERFACE)(EFI_HANDLE *Handle,
                                                     EFI_GUID *Protocol,
                                                     VOID *Interface);
typedef EFI_STATUS (*EFI_REINSTALL_PROTOCOL_INTERFACE)(EFI_HANDLE Handle,
                                                       EFI_GUID *Protocol,
                                                       VOID *OldInterface,
                                                       VOID *NewInterface);
typedef EFI_STATUS (*EFI_UNINSTALL_PROTOCOL_INTERFACE)(EFI_HANDLE Handle,
                                                       EFI_GUID *Protocol,
                                                       VOID *Interface);
typedef EFI_STATUS (*EFI_HANDLE_PROTOCOL)(EFI_HANDLE Handle,
                                          EFI_GUID *Protocol, VOID **Interface);
typedef VOID (*EFI_RESERVED_FN)(VOID);
typedef EFI_STATUS (*EFI_REGISTER_PROTOCOL_NOTIFY)(EFI_GUID *Protocol,
                                                   EFI_EVENT Event,
                                                   EFI_HANDLE *Registration);
typedef EFI_STATUS (*EFI_LOCATE_HANDLE)(UINT32 SearchType, EFI_GUID *Protocol,
                                        VOID *SearchKey, UINTN *BufferSize,
                                        EFI_HANDLE *Buffer);
typedef EFI_STATUS (*EFI_LOCATE_DEVICE_PATH)(EFI_GUID *Protocol,
                                             VOID **DevicePath,
                                             EFI_HANDLE *Device);
typedef EFI_STATUS (*EFI_INSTALL_CONFIGURATION_TABLE_FN)(EFI_GUID *Guid,
                                                         VOID *Table);
typedef EFI_STATUS (*EFI_IMAGE_LOAD)(BOOLEAN BootPolicy,
                                     EFI_HANDLE ParentImageHandle,
                                     VOID *DevicePath, VOID *SourceBuffer,
                                     UINTN SourceSize, EFI_HANDLE *ImageHandle);
typedef EFI_STATUS (*EFI_IMAGE_START)(EFI_HANDLE ImageHandle,
                                      UINTN *ExitDataSize, CHAR16 **ExitData);
typedef EFI_STATUS (*EFI_EXIT)(EFI_HANDLE ImageHandle, EFI_STATUS ExitStatus,
                               UINTN ExitDataSize, CHAR16 *ExitData);
typedef EFI_STATUS (*EFI_UNLOAD_IMAGE)(EFI_HANDLE ImageHandle);
typedef EFI_STATUS (*EFI_EXIT_BOOT_SERVICES)(EFI_HANDLE ImageHandle, UINTN MapKey);
typedef EFI_STATUS (*EFI_GET_NEXT_MONOTONIC_COUNT)(UINT64 *Count);
typedef EFI_STATUS (*EFI_STALL)(UINTN Microseconds);
typedef EFI_STATUS (*EFI_SET_WATCHDOG_TIMER)(UINTN Timeout, UINT64 WatchdogCode,
                                             UINTN DataSize, CHAR16 *WatchdogData);
typedef EFI_STATUS (*EFI_CONNECT_CONTROLLER)(EFI_HANDLE ControllerHandle,
                                             EFI_HANDLE *DriverImageHandle,
                                             VOID *RemainingDevicePath,
                                             BOOLEAN Recursive);
typedef EFI_STATUS (*EFI_DISCONNECT_CONTROLLER)(EFI_HANDLE ControllerHandle,
                                                EFI_HANDLE DriverImageHandle,
                                                EFI_HANDLE ChildHandle);
typedef EFI_STATUS (*EFI_OPEN_PROTOCOL)(EFI_HANDLE Handle, EFI_GUID *Protocol,
                                        VOID **Interface, EFI_HANDLE AgentHandle,
                                        EFI_HANDLE ControllerHandle,
                                        UINT32 Attributes);
typedef EFI_STATUS (*EFI_CLOSE_PROTOCOL)(EFI_HANDLE Handle, EFI_GUID *Protocol,
                                         EFI_HANDLE AgentHandle,
                                         EFI_HANDLE ControllerHandle);
typedef EFI_STATUS (*EFI_OPEN_PROTOCOL_INFORMATION)(EFI_HANDLE Handle,
                                                    EFI_GUID *Protocol,
                                                    VOID **EntryBuffer,
                                                    UINTN *EntryCount);
typedef EFI_STATUS (*EFI_PROTOCOLS_PER_HANDLE)(EFI_HANDLE Handle,
                                               EFI_GUID ***ProtocolBuffer,
                                               UINTN *ProtocolBufferCount);
typedef EFI_STATUS (*EFI_LOCATE_HANDLE_BUFFER)(UINT32 SearchType,
                                               EFI_GUID *Protocol, VOID *SearchKey,
                                               UINTN *NoHandles,
                                               EFI_HANDLE **Buffer);
typedef EFI_STATUS (*EFI_LOCATE_PROTOCOL)(EFI_GUID *Protocol, VOID *Registration,
                                          VOID **Interface);
typedef EFI_STATUS (*EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES)(EFI_HANDLE *Handle, ...);
typedef EFI_STATUS (*EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES)(EFI_HANDLE Handle, ...);
typedef EFI_STATUS (*EFI_CALCULATE_CRC32)(VOID *Data, UINTN DataSize, UINT32 *Crc32);
typedef VOID (*EFI_COPY_MEM)(VOID *Destination, VOID *Source, UINTN Length);
typedef VOID (*EFI_SET_MEM)(VOID *Buffer, UINTN Size, UINT8 Value);
typedef EFI_STATUS (*EFI_CREATE_EVENT_EX)(UINT32 Type, UINTN NotifyTpl,
                                          EFI_EVENT_NOTIFY NotifyFunction,
                                          VOID *NotifyContext,
                                          EFI_GUID *EventGroup, EFI_EVENT *Event);

/* ------------------------------------------------------------- protocols */

typedef struct {
    EFI_TEXT_RESET              Reset;
    EFI_TEXT_STRING             OutputString;
    EFI_TEXT_TEST_STRING        TestString;
    EFI_TEXT_QUERY_MODE         QueryMode;
    EFI_TEXT_SET_MODE           SetMode;
    EFI_TEXT_ENABLE_CURSOR      EnableCursor;
    VOID                       *Mode;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
    UINT32          MaxMode;
    UINT32          Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN           SizeOfInfo;
    UINT64          FrameBufferBase;
    UINTN           FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct {
    EFI_GOP_QUERY_MODE  QueryMode;
    EFI_GOP_SET_MODE    SetMode;
    EFI_GOP_BLT         Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

#define EFI_FILE_MODE_READ      0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE     0x0000000000000002ULL

/* --------------------------- EFI_SIMPLE_TEXT_INPUT_PROTOCOL (EFI 1.10) --- */

/* EFI_INPUT_KEY: ScanCode 0 for printable keys; UnicodeChar 0 otherwise. */
typedef struct {
    UINT16  ScanCode;
    CHAR16  UnicodeChar;
} EFI_INPUT_KEY;

typedef EFI_STATUS (*EFI_INPUT_RESET)(VOID *This, BOOLEAN ExtendedVerification);
typedef EFI_STATUS (*EFI_INPUT_READ_KEY_STROKE)(VOID *This, EFI_INPUT_KEY *Key);

typedef struct {
    EFI_INPUT_RESET              Reset;
    EFI_INPUT_READ_KEY_STROKE    ReadKeyStroke;
    EFI_EVENT                    WaitForKey;
} EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

/* UEFI scan codes (subset used by the boot picker). */
#define SCAN_NULL     0x0000
#define SCAN_UP       0x0001
#define SCAN_DOWN     0x0002
#define SCAN_RIGHT    0x0004
#define SCAN_LEFT     0x0005
#define SCAN_HOME     0x0006
#define SCAN_END      0x0007
#define SCAN_DELETE   0x0008
#define SCAN_ESC      0x0017

/* EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL (UEFI 2.x): exposes modifier state so
 * a held Alt at power-on can be detected (plain ConIn cannot). */
#define EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID \
    EFI_GUID_INIT(0xdd9e7534, 0x7762, 0x4698, 0x8c, 0x14, 0xf5, 0x85, 0x17, 0xa6, 0x25, 0xaa)

#define EFI_SHIFT_STATE_VALID        0x80000000u
#define EFI_RIGHT_SHIFT_PRESSED      0x00000001u
#define EFI_LEFT_SHIFT_PRESSED       0x00000002u
#define EFI_RIGHT_CONTROL_PRESSED    0x00000004u
#define EFI_LEFT_CONTROL_PRESSED     0x00000008u
#define EFI_RIGHT_ALT_PRESSED        0x00000010u
#define EFI_LEFT_ALT_PRESSED         0x00000020u
#define EFI_RIGHT_LOGO_PRESSED       0x00000040u
#define EFI_LEFT_LOGO_PRESSED        0x00000080u

typedef UINT8 EFI_KEY_TOGGLE_STATE;

typedef struct {
    UINT32                  KeyShiftState;
    EFI_KEY_TOGGLE_STATE    KeyToggleState;
} EFI_KEY_STATE;

typedef struct {
    EFI_INPUT_KEY   Key;
    EFI_KEY_STATE   KeyState;
} EFI_KEY_DATA;

typedef EFI_STATUS (*EFI_INPUT_RESET_EX)(VOID *This, BOOLEAN ExtendedVerification);
typedef EFI_STATUS (*EFI_INPUT_READ_KEY_EX)(VOID *This, EFI_KEY_DATA *KeyData);
typedef EFI_STATUS (*EFI_SET_STATE_EX)(VOID *This, EFI_KEY_TOGGLE_STATE *KeyToggleState);
typedef EFI_STATUS (*EFI_REGISTER_KEYSTROKE_NOTIFY)(VOID *This, EFI_KEY_DATA *KeyData,
                                                    VOID *NotifyFunction, VOID **NotifyHandle);
typedef EFI_STATUS (*EFI_UNREGISTER_KEYSTROKE_NOTIFY)(VOID *This, VOID *NotifyHandle);

typedef struct {
    EFI_INPUT_RESET_EX                  Reset;
    EFI_INPUT_READ_KEY_EX               ReadKeyStrokeEx;
    EFI_EVENT                           WaitForKeyEx;
    EFI_SET_STATE_EX                    SetState;
    EFI_REGISTER_KEYSTROKE_NOTIFY       RegisterKeyNotify;
    EFI_UNREGISTER_KEYSTROKE_NOTIFY     UnregisterKeyNotify;
} EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL;

/* EFI_KEY_NOTIFY_FUNCTION is registered by firmware consumers only; we never
 * register notifications, so the function pointer stays opaque. */

#define EFI_FILE_READ_ONLY      0x0000000000000001ULL
#define EFI_FILE_HIDDEN         0x0000000000000002ULL
#define EFI_FILE_SYSTEM         0x0000000000000004ULL
#define EFI_FILE_DIRECTORY      0x0000000000000010ULL
#define EFI_FILE_ARCHIVE        0x0000000000000020ULL

typedef struct {
    UINT64  Size;
    UINT64  FileSize;
    UINT64  PhysicalSize;
    EFI_TIME CreateTime;
    EFI_TIME LastAccessTime;
    EFI_TIME ModificationTime;
    UINT64  Attribute;
    CHAR16  FileName[1];
} EFI_FILE_INFO;

typedef struct {
    UINT64                  Revision;
    EFI_FILE_OPEN           Open;
    EFI_FILE_CLOSE          Close;
    EFI_FILE_DELETE         Delete;
    EFI_FILE_READ           Read;
    EFI_FILE_WRITE          Write;
    EFI_FILE_GET_POSITION   GetPosition;
    EFI_FILE_SET_POSITION   SetPosition;
    EFI_FILE_GET_INFO       GetInfo;
    EFI_FILE_SET_INFO       SetInfo;
    EFI_FILE_FLUSH          Flush;
    EFI_FILE_OPEN_EX        OpenEx;
    EFI_FILE_READ_EX        ReadEx;
    EFI_FILE_WRITE_EX       WriteEx;
    EFI_FILE_FLUSH_EX       FlushEx;
} EFI_FILE_PROTOCOL;

typedef struct {
    UINT64                  Revision;
    EFI_VOLUME_OPEN_VOLUME  OpenVolume;
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

/* EFI_FILE_SYSTEM_INFO (for the FAT volume label shown in the boot menu).
 * Spec layout (UEFI 2.10): Size, ReadOnly, VolumeSize, FreeSpace, BlockSize,
 * VolumeLabel - there is NO MaxSpace field; adding one shifts VolumeLabel
 * 8 bytes and truncates the label. */
typedef struct {
    UINT64  Size;
    BOOLEAN ReadOnly;
    UINTN   VolumeSize;
    UINTN   FreeSpace;
    UINT32  BlockSize;
    CHAR16  VolumeLabel[1];
} EFI_FILE_SYSTEM_INFO;

#define EFI_FILE_SYSTEM_INFO_ID \
    EFI_GUID_INIT(0x09576e93, 0x6d3f, 0x11d2, 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b)

typedef struct {
    UINT32          Revision;
    EFI_HANDLE      ParentHandle;
    VOID            *SystemTable;
    EFI_HANDLE      DeviceHandle;
    VOID            *FilePath;
    VOID            *Reserved;
    UINT32          LoadOptionsSize;
    VOID            *LoadOptions;
    VOID            *ImageBase;
    UINT64          ImageSize;
    EFI_MEMORY_TYPE ImageCodeType;
    EFI_MEMORY_TYPE ImageDataType;
    EFI_STATUS (*Unload)(EFI_HANDLE ImageHandle);
} EFI_LOADED_IMAGE_PROTOCOL;

/* -------------------------------------------------------- runtime services */

typedef struct {
    EFI_TABLE_HEADER                        Hdr;

    EFI_GET_TIME                            GetTime;
    EFI_SET_TIME                            SetTime;
    EFI_GET_WAKEUP_TIME                     GetWakeupTime;
    EFI_SET_WAKEUP_TIME                     SetWakeupTime;
    EFI_SET_VIRTUAL_ADDRESS_MAP             SetVirtualAddressMap;
    EFI_CONVERT_POINTER                     ConvertPointer;
    EFI_GET_VARIABLE                        GetVariable;
    EFI_GET_NEXT_VARIABLE_NAME              GetNextVariableName;
    EFI_SET_VARIABLE                        SetVariable;
    EFI_GET_NEXT_HIGH_MONOTONIC_COUNT       GetNextHighMonotonicCount;
    EFI_RESET_SYSTEM                        ResetSystem;
    EFI_UPDATE_CAPSULE                      UpdateCapsule;
    EFI_QUERY_CAPSULE_CAPABILITIES          QueryCapsuleCapabilities;
    EFI_QUERY_VARIABLE_INFO                 QueryVariableInfo;
} EFI_RUNTIME_SERVICES;

/* ----------------------------------------------------------- boot services */

typedef struct {
    EFI_TABLE_HEADER                                Hdr;

    EFI_RAISE_TPL                                   RaiseTPL;
    EFI_RESTORE_TPL                                 RestoreTPL;
    EFI_ALLOCATE_PAGES                              AllocatePages;
    EFI_FREE_PAGES                                  FreePages;
    EFI_GET_MEMORY_MAP                              GetMemoryMap;
    EFI_ALLOCATE_POOL                               AllocatePool;
    EFI_FREE_POOL                                   FreePool;
    EFI_CREATE_EVENT                                CreateEvent;
    EFI_SET_TIMER                                   SetTimer;
    EFI_WAIT_FOR_EVENT                              WaitForEvent;
    EFI_SIGNAL_EVENT                                SignalEvent;
    EFI_CLOSE_EVENT                                 CloseEvent;
    EFI_CHECK_EVENT                                 CheckEvent;
    EFI_INSTALL_PROTOCOL_INTERFACE                  InstallProtocolInterface;
    EFI_REINSTALL_PROTOCOL_INTERFACE                ReinstallProtocolInterface;
    EFI_UNINSTALL_PROTOCOL_INTERFACE                UninstallProtocolInterface;
    EFI_HANDLE_PROTOCOL                             HandleProtocol;
    EFI_RESERVED_FN                                 Reserved;
    EFI_REGISTER_PROTOCOL_NOTIFY                    RegisterProtocolNotify;
    EFI_LOCATE_HANDLE                               LocateHandle;
    EFI_LOCATE_DEVICE_PATH                          LocateDevicePath;
    EFI_INSTALL_CONFIGURATION_TABLE_FN              InstallConfigurationTable;
    EFI_IMAGE_LOAD                                  LoadImage;
    EFI_IMAGE_START                                 StartImage;
    EFI_EXIT                                        Exit;
    EFI_UNLOAD_IMAGE                                UnloadImage;
    EFI_EXIT_BOOT_SERVICES                          ExitBootServices;
    EFI_GET_NEXT_MONOTONIC_COUNT                    GetNextMonotonicCount;
    EFI_STALL                                       Stall;
    EFI_SET_WATCHDOG_TIMER                          SetWatchdogTimer;
    EFI_CONNECT_CONTROLLER                          ConnectController;
    EFI_DISCONNECT_CONTROLLER                       DisconnectController;
    EFI_OPEN_PROTOCOL                               OpenProtocol;
    EFI_CLOSE_PROTOCOL                              CloseProtocol;
    EFI_OPEN_PROTOCOL_INFORMATION                   OpenProtocolInformation;
    EFI_PROTOCOLS_PER_HANDLE                        ProtocolsPerHandle;
    EFI_LOCATE_HANDLE_BUFFER                        LocateHandleBuffer;
    EFI_LOCATE_PROTOCOL                             LocateProtocol;
    EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES        InstallMultipleProtocolInterfaces;
    EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES      UninstallMultipleProtocolInterfaces;
    EFI_CALCULATE_CRC32                             CalculateCrc32;
    EFI_COPY_MEM                                    CopyMem;
    EFI_SET_MEM                                     SetMem;
    EFI_CREATE_EVENT_EX                             CreateEventEx;
} EFI_BOOT_SERVICES;

/* ------------------------------------------------------------ system table */

typedef struct {
    EFI_TABLE_HEADER                        Hdr;
    CHAR16                                 *FirmwareVendor;
    UINT32                                  FirmwareRevision;
    UINT32                                  __pad;
    EFI_HANDLE                              ConsoleInHandle;
    VOID                                   *ConIn;
    EFI_HANDLE                              ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL        *ConOut;
    EFI_HANDLE                              StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL        *StdErr;
    EFI_RUNTIME_SERVICES                   *RuntimeServices;
    EFI_BOOT_SERVICES                      *BootServices;
    UINTN                                   NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE                *ConfigurationTable;
} EFI_SYSTEM_TABLE;

/* ------------------------------------------------------------- GUID table */

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
    EFI_GUID_INIT(0x964e5b22, 0x6459, 0x11d2, 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b)

#define EFI_FILE_INFO_ID \
    EFI_GUID_INIT(0x09576e92, 0x6d3f, 0x11d2, 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b)

#define EFI_LOADED_IMAGE_PROTOCOL_GUID \
    EFI_GUID_INIT(0x5b1b31a1, 0x9562, 0x11d2, 0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b)

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    EFI_GUID_INIT(0x9042a9de, 0x23dc, 0x4a38, 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a)

#define EFI_ACPI_20_TABLE_GUID \
    EFI_GUID_INIT(0x8868e871, 0xe4f1, 0x11d3, 0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81)

#define EFI_ACPI_10_TABLE_GUID \
    EFI_GUID_INIT(0xeb9d2d30, 0x2d88, 0x11d3, 0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d)

#define EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID \
    EFI_GUID_INIT(0x387477c1, 0x69c7, 0x11d2, 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b)

#define EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID_EX \
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID

#define EFI_BLOCK_IO_PROTOCOL_GUID \
    EFI_GUID_INIT(0x964e5b21, 0x6459, 0x11d2, 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b)

#define EFI_LOADED_IMAGE_PROTOCOL_REVISION  0x1000

/* EFI_DEVICE_PATH_PROTOCOL (EFI 1.10): minimal node header. We only need to
 * walk/append nodes for LoadImage device paths. */
typedef struct {
    UINT8   Type;
    UINT8   SubType;
    UINT8   Length[2];          /* little-endian, includes this header */
} EFI_DEVICE_PATH_PROTOCOL;

#define EFI_DEVICE_PATH_PROTOCOL_GUID \
    EFI_GUID_INIT(0x09576e91, 0x6d3f, 0x11d2, 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b)

#define EFI_DP_TYPE_END              0x7F
#define EFI_DP_SUBTYPE_END_ALL       0xFF
#define EFI_DP_TYPE_MEDIA            0x04
#define EFI_DP_SUBTYPE_MEDIA_FILE    0x04

/* locate search types */
#define EFI_ALL_HANDLES             0u
#define EFI_BY_REGISTER_NOTIFY      1u
#define EFI_BY_PROTOCOL             2u

/* --------------------------------------------------------------
 * robobootloader helpers built on top of the raw UEFI definitions
 * (implemented in uefi.c)
 * -------------------------------------------------------------- */

/* Open the boot volume (ESP we booted from, or any ESP as fallback). */
EFI_FILE_PROTOCOL *file_open_volume(void);

/* Open a file by ASCII path (NULL on failure). Close with file_close(). */
EFI_FILE_PROTOCOL *file_open(EFI_FILE_PROTOCOL *volume, const char *path);

/* Read an entire file into freshly allocated memory (+1 NUL byte).
 * Returns false on any failure. Free with mem_free(). */
bool file_read_whole(EFI_FILE_PROTOCOL *volume, const char *path,
                     uint8_t **out_data, uint64_t *out_size);

/* Chunked read with a progress callback: after every chunk the callback gets
 * (bytes_done, bytes_total, ctx). The callback must be fast and non-blocking
 * (update UI state only); NULL disables reporting. Same guarantees and
 * allocation behaviour as file_read_all. */
typedef void (*file_progress_fn)(uint64_t done, uint64_t total, void *ctx);
bool file_read_all_progress(EFI_FILE_PROTOCOL *f, uint8_t **out_data,
                            uint64_t *out_size, file_progress_fn progress,
                            void *ctx);

void file_close(EFI_FILE_PROTOCOL *f);

/* Find the ACPI RSDP (ACPI 2.0 preferred, 1.0 fallback). 0 if absent. */
uint64_t find_acpi_rsdp(void);

/* Enumerate every volume exposing EFI_SIMPLE_FILE_SYSTEM_PROTOCOL (i.e. every
 * ESP the firmware knows). Fills handles[] (up to max) with the opened root
 * EFI_FILE_PROTOCOL of each volume; returns the count (0 on none/error).
 * Caller closes each with file_close(). */
uint64_t file_open_all_volumes(EFI_FILE_PROTOCOL **handles, uint64_t max);

/* Size of a file in bytes (0 on failure or if too large). Does not move the
 * read position permanently (rewinds to 0). */
uint64_t file_size(EFI_FILE_PROTOCOL *f);

/* ASCII-ified FAT volume label of an opened volume root (empty on failure).
 * Non-ASCII characters are dropped. */
void file_volume_label(EFI_FILE_PROTOCOL *root, char *out, uint64_t cap);

/* Locate the Simple Text Input EX protocol on the active ConsoleIn handle.
 * NULL if the firmware does not provide it. */
EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *console_input_ex(void);

/* Firmware image loading: LoadImage from a device path + StartImage.
 * Used by the boot picker to chainload \EFI\BOOT\BOOTX64.EFI on another
 * volume. On success this never returns (the new image runs). */
EFI_STATUS file_load_image(EFI_HANDLE volume_handle, const CHAR16 *path,
                           EFI_HANDLE *out_image);
EFI_STATUS file_start_image(EFI_HANDLE image_handle);

/* The device handle that a SimpleFileSystem volume handle belongs to (for
 * LoadImage device paths). Pass the handle returned by LocateHandleBuffer. */
EFI_HANDLE volume_device_handle(uint64_t index); /* implemented via statics */

#endif /* ROBO_UEFI_H */
