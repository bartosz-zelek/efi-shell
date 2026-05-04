#include <efi.h>
#include <efilib.h>

#define INPUT_MAX   256
#define MAX_TOKENS  16
#define MAX_HISTORY 50

static CHAR16 history[MAX_HISTORY][INPUT_MAX];
static UINTN  history_count = 0;
static UINTN  history_index = 0;

static VOID history_add(const CHAR16 *cmd) {
    if (history_count < MAX_HISTORY) {
        StrCpy(history[history_count++], cmd);
    } else {
        for (UINTN i = 0; i < MAX_HISTORY - 1; i++)
            StrCpy(history[i], history[i + 1]);
        StrCpy(history[MAX_HISTORY - 1], cmd);
    }
    history_index = history_count;
}

static VOID line_zero(CHAR16 *line, UINTN max) {
    for (UINTN i = 0; i < max; i++) line[i] = L'\0';
}

static VOID line_clear(UINTN pos) {
    while (pos--) Print(L"\b \b");
}

static VOID line_load(CHAR16 *line, UINTN *pos, UINTN max, const CHAR16 *src) {
    line_clear(*pos);
    StrCpy(line, src);
    *pos = StrLen(line);
    if (*pos >= max) *pos = max - 1;
    Print(L"%s", line);
}

static VOID input_read(EFI_SYSTEM_TABLE *ST, const CHAR16 *prompt, CHAR16 *line, UINTN max) {
    UINTN pos = 0;
    EFI_INPUT_KEY key;

    line_zero(line, max);
    Print(prompt);

    for (;;) {
        if (uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key) != EFI_SUCCESS)
            continue;

        if (key.ScanCode == SCAN_UP) {
            if (history_index > 0)
                line_load(line, &pos, max, history[--history_index]);

        } else if (key.ScanCode == SCAN_DOWN) {
            if (history_index < history_count - 1) {
                line_load(line, &pos, max, history[++history_index]);
            } else if (history_index == history_count - 1) {
                history_index = history_count;
                line_clear(pos);
                line_zero(line, max);
                pos = 0;
            }

        } else if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            break;

        } else if (key.UnicodeChar == CHAR_BACKSPACE) {
            if (pos > 0) {
                line[--pos] = L'\0';
                Print(L"\b \b");
            }

        } else if (key.UnicodeChar >= 0x20 && key.UnicodeChar < 0x7F && pos < max - 1) {
            line[pos++] = key.UnicodeChar;
            line[pos]   = L'\0';
            Print(L"%c", key.UnicodeChar);
        }
    }
}

static VOID cmd_help(void) {
    Print(L"Available commands:\r\n"
          L"  help              show this message\r\n"
          L"  cls               clear the screen\r\n"
          L"  echo <text>       print arguments\r\n"
          L"  ver               show UEFI firmware version\r\n"
          L"  time              show current date and time\r\n"
          L"  stall <ms>        sleep for <ms> milliseconds\r\n"
          L"  reboot            cold reset the system\r\n"
          L"  poweroff          shut down the system\r\n"
          L"  exit              quit the shell\r\n");
}

static VOID cmd_ver(EFI_SYSTEM_TABLE *ST) {
    Print(L"UEFI Specification: %u.%u\r\n", (ST->Hdr.Revision >> 16) & 0xFFFF, ST->Hdr.Revision & 0xFFFF);
    Print(L"Firmware Vendor:    %s\r\n",     ST->FirmwareVendor);
    Print(L"Firmware Revision:  0x%08x\r\n", ST->FirmwareRevision);
}

static VOID cmd_time(EFI_SYSTEM_TABLE *ST) {
    EFI_TIME Time;
    EFI_STATUS Status = uefi_call_wrapper(ST->RuntimeServices->GetTime, 2, &Time, NULL);
    if (EFI_ERROR(Status)) { Print(L"GetTime failed\r\n"); return; }
    Print(L"%04u-%02u-%02u  %02u:%02u:%02u\r\n", Time.Year, Time.Month, Time.Day, Time.Hour, Time.Minute, Time.Second);
}

static VOID cmd_stall(EFI_SYSTEM_TABLE *ST, const CHAR16 *args) {
    if (!args || StrLen(args) == 0) {
        Print(L"Usage: stall <ms>\r\n");
        return;
    }
    UINTN ms = 0;
    for (UINTN i = 0; args[i] >= L'0' && args[i] <= L'9'; i++)
        ms = ms * 10 + (args[i] - L'0');
    uefi_call_wrapper(ST->BootServices->Stall, 1, ms * 1000);
    Print(L"Stalled %lu ms\r\n", ms);
}

static VOID cmd_reboot(EFI_SYSTEM_TABLE *ST) {
    uefi_call_wrapper(ST->RuntimeServices->ResetSystem, 4, EfiResetCold, EFI_SUCCESS, 0, NULL);
}

static VOID cmd_poweroff(EFI_SYSTEM_TABLE *ST) {
    uefi_call_wrapper(ST->RuntimeServices->ResetSystem, 4, EfiResetShutdown, EFI_SUCCESS, 0, NULL);
}

static BOOLEAN cmd_dispatch(EFI_SYSTEM_TABLE *ST, const CHAR16 *line) {
    if      (StrCmp (line, L"help")      == 0) { cmd_help();             }
    else if (StrCmp (line, L"cls")       == 0) { uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut); }
    else if (StrnCmp(line, L"echo",  4)  == 0) { Print(L"%s\r\n", StrLen(line) > 5 ? line + 5 : L""); }
    else if (StrCmp (line, L"ver")       == 0) { cmd_ver(ST);            }
    else if (StrCmp (line, L"time")      == 0) { cmd_time(ST);           }
    else if (StrnCmp(line, L"stall", 5)  == 0) { cmd_stall(ST, StrLen(line) > 6 ? line + 6 : L""); }
    else if (StrCmp (line, L"reboot")    == 0) { cmd_reboot(ST);         }
    else if (StrCmp (line, L"poweroff")  == 0) { cmd_poweroff(ST);       }
    else if (StrCmp (line, L"exit")      == 0) { return FALSE;           }
    else if (StrLen (line) > 0)                { Print(L"Unknown command: %s\r\n", line); }
    return TRUE;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST) {
    InitializeLib(ImageHandle, ST);
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);

    CHAR16 line[INPUT_MAX];

    while (TRUE) {
        input_read(ST, L"uefi> ", line, INPUT_MAX);
        Print(L"\r\n");

        if (StrLen(line) > 0)
            history_add(line);

        if (!cmd_dispatch(ST, line))
            break;
    }

    return EFI_SUCCESS;
}