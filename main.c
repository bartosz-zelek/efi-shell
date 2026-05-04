#include <efi.h>
#include <efilib.h>

#define INPUT_MAX 256
#define MAX_TOKENS 16

EFI_STATUS
EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  InitializeLib(ImageHandle, SystemTable);

  CHAR16 line[INPUT_MAX];
  CHAR16 *tokens[MAX_TOKENS];
  BOOLEAN running = TRUE;

  uefi_call_wrapper(SystemTable->ConOut->ClearScreen, 1, SystemTable->ConOut);

  while (running) {
    for (UINTN i = 0; i < INPUT_MAX; i++) {
      line[i] = L'\0';
    }

    Input(L"uefi> ", line, INPUT_MAX);

    Print(L"\r\n");

    if (StrCmp(line, L"help") == 0) {
      Print(L"Available commands:\r\n");
      Print(L"  help - show this message\r\n");
      Print(L"  cls  - clear screen\r\n");
      Print(L"  echo - print arguments\r\n");
      Print(L"  exit - leave the shell\r\n");
    } else if (StrCmp(line, L"cls") == 0) {
      uefi_call_wrapper(SystemTable->ConOut->ClearScreen, 1, SystemTable->ConOut);
    } else if (StrnCmp(line, L"echo", 4) == 0) {
      Print(L"%s\r\n", line + 5);
    } else if (StrCmp(line, L"exit") == 0) {
      running = FALSE;
    } else {
      Print(L"Unknown command: %s\r\n", line);
    }

  }

  return EFI_SUCCESS;
}