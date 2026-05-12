#include <efi.h>
#include <efilib.h>

#define SHELL_PLUGIN_PROTOCOL_GUID \
    { 0xf1e2d3c4, 0xa5b6, 0x47c8, { 0x90, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67 } }

typedef struct _SHELL_PLUGIN_PROTOCOL
{
    CHAR16     *Name;
    EFI_STATUS (*Execute)(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST, CHAR16 *args);
} SHELL_PLUGIN_PROTOCOL;


EFI_STATUS plugin_execute(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST, CHAR16 *args)
{
    Print(L"hello from plugin! args: '%s'\r\n", args);
    return EFI_SUCCESS;
}

static SHELL_PLUGIN_PROTOCOL g_plugin =
{
    L"hello",
    plugin_execute
};

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST)
{
    InitializeLib(ImageHandle, ST);

    EFI_GUID plugin_guid = SHELL_PLUGIN_PROTOCOL_GUID;
    EFI_HANDLE handle    = NULL;

    uefi_call_wrapper(
        BS->InstallProtocolInterface, 4,
        &handle, &plugin_guid, EFI_NATIVE_INTERFACE, &g_plugin);

    return EFI_SUCCESS;
}