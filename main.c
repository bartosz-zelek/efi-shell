#include <efi.h>
#include <efilib.h>

#define INPUT_MAX    256
#define MAX_HISTORY  50
#define MAX_ENV      32
#define ENV_NAME_MAX 32
#define ENV_VAL_MAX  128

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

static CHAR16 env_names [MAX_ENV][ENV_NAME_MAX];
static CHAR16 env_values[MAX_ENV][ENV_VAL_MAX];
static UINTN  env_count = 0;

static const CHAR16 *env_get(const CHAR16 *name) {
    for (UINTN i = 0; i < env_count; i++)
        if (StrCmp(env_names[i], name) == 0)
            return env_values[i];
    return NULL;
}

static VOID env_set(const CHAR16 *name, const CHAR16 *value) {
    for (UINTN i = 0; i < env_count; i++) {
        if (StrCmp(env_names[i], name) == 0) {
            StrCpy(env_values[i], value);
            return;
        }
    }
    if (env_count < MAX_ENV) {
        StrCpy(env_names [env_count], name);
        StrCpy(env_values[env_count], value);
        env_count++;
    } else {
        Print(L"set: environment full\r\n");
    }
}

static BOOLEAN env_unset(const CHAR16 *name) {
    for (UINTN i = 0; i < env_count; i++) {
        if (StrCmp(env_names[i], name) == 0) {
            env_count--;
            StrCpy(env_names [i], env_names [env_count]);
            StrCpy(env_values[i], env_values[env_count]);
            return TRUE;
        }
    }
    return FALSE;
}

static VOID env_expand(const CHAR16 *src, CHAR16 *dst) {
    UINTN d = 0;
    for (UINTN s = 0; src[s] && d < INPUT_MAX - 1; ) {
        if (src[s] != L'$') { dst[d++] = src[s++]; continue; }
        s++; /* skip '$' */
        CHAR16 vname[ENV_NAME_MAX]; UINTN vn = 0;
        while (src[s] && vn < ENV_NAME_MAX - 1) {
            CHAR16 c = src[s];
            if (!((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') ||
                  (c >= L'0' && c <= L'9') || c == L'_')) break;
            vname[vn++] = c; s++;
        }
        vname[vn] = L'\0';
        const CHAR16 *val = (vn > 0) ? env_get(vname) : NULL;
        if (val)
            for (UINTN vi = 0; val[vi] && d < INPUT_MAX - 1; vi++)
                dst[d++] = val[vi];
    }
    dst[d] = L'\0';
}

static VOID line_zero(CHAR16 *line) {
    for (UINTN i = 0; i < INPUT_MAX; i++) line[i] = L'\0';
}
static VOID line_clear(UINTN pos) { while (pos--) Print(L"\b \b"); }
static VOID line_load(CHAR16 *line, UINTN *pos, const CHAR16 *src) {
    line_clear(*pos);
    StrCpy(line, src);
    *pos = StrLen(line);
    if (*pos >= INPUT_MAX) *pos = INPUT_MAX - 1;
    Print(L"%s", line);
}

static VOID input_read(EFI_SYSTEM_TABLE *ST, CHAR16 *line) {
    UINTN pos = 0;
    EFI_INPUT_KEY key;
    line_zero(line);
    Print(L"uefi> ");
    for (;;) {
        if (uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key) != EFI_SUCCESS)
            continue;
        if (key.ScanCode == SCAN_UP) {
            if (history_index > 0) line_load(line, &pos, history[--history_index]);
        } else if (key.ScanCode == SCAN_DOWN) {
            if (history_index < history_count - 1)
                line_load(line, &pos, history[++history_index]);
            else if (history_index == history_count - 1) {
                history_index = history_count;
                line_clear(pos); line_zero(line); pos = 0;
            }
        } else if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            break;
        } else if (key.UnicodeChar == CHAR_BACKSPACE) {
            if (pos > 0) { line[--pos] = L'\0'; Print(L"\b \b"); }
        } else if (key.UnicodeChar >= 0x20 && key.UnicodeChar < 0x7F && pos < INPUT_MAX - 1) {
            line[pos++] = key.UnicodeChar;
            line[pos]   = L'\0';
            Print(L"%c", key.UnicodeChar);
        }
    }
}

static VOID cmd_help(void) {
    Print(L"Commands:\r\n"
          L"  help             show this message\r\n"
          L"  cls              clear screen\r\n"
          L"  echo <text>      print text ($VAR expansion supported)\r\n"
          L"  ver              UEFI firmware version\r\n"
          L"  time             current date and time\r\n"
          L"  stall <ms>       sleep for <ms> milliseconds\r\n"
          L"  set              list all variables\r\n"
          L"  set <name>=<val> set a variable\r\n"
          L"  unset <name>     delete a variable\r\n"
          L"  reboot           cold reset\r\n"
          L"  poweroff         shut down\r\n"
          L"  exit             quit the shell\r\n");
}

static VOID cmd_ver(EFI_SYSTEM_TABLE *ST) {
    Print(L"UEFI Specification: %u.%u\r\n",
          (ST->Hdr.Revision >> 16) & 0xFFFF, ST->Hdr.Revision & 0xFFFF);
    Print(L"Firmware Vendor:    %s\r\n",     ST->FirmwareVendor);
    Print(L"Firmware Revision:  0x%08x\r\n", ST->FirmwareRevision);
}

static VOID cmd_time(EFI_SYSTEM_TABLE *ST) {
    EFI_TIME t;
    if (EFI_ERROR(uefi_call_wrapper(ST->RuntimeServices->GetTime, 2, &t, NULL))) {
        Print(L"GetTime failed\r\n"); return;
    }
    Print(L"%04u-%02u-%02u  %02u:%02u:%02u\r\n",
          t.Year, t.Month, t.Day, t.Hour, t.Minute, t.Second);
}

static VOID cmd_stall(EFI_SYSTEM_TABLE *ST, const CHAR16 *arg) {
    if (!arg || !*arg) { Print(L"Usage: stall <ms>\r\n"); return; }
    UINTN ms = 0;
    for (UINTN i = 0; arg[i] >= L'0' && arg[i] <= L'9'; i++)
        ms = ms * 10 + (arg[i] - L'0');
    uefi_call_wrapper(ST->BootServices->Stall, 1, ms * 1000);
    Print(L"Stalled %lu ms\r\n", ms);
}

static VOID cmd_set(const CHAR16 *args) {
    if (!args || !*args) {
        if (env_count == 0) { Print(L"(no variables)\r\n"); return; }
        for (UINTN i = 0; i < env_count; i++)
            Print(L"%s=%s\r\n", env_names[i], env_values[i]);
        return;
    }
    /* Find '=' */
    UINTN eq = 0;
    while (args[eq] && args[eq] != L'=') eq++;
    if (!args[eq]) { Print(L"Usage: set <name>=<value>\r\n"); return; }

    CHAR16 name[ENV_NAME_MAX] = {0};
    UINTN nlen = (eq < ENV_NAME_MAX - 1) ? eq : ENV_NAME_MAX - 1;
    for (UINTN k = 0; k < nlen; k++) name[k] = args[k];

    CHAR16 value[ENV_VAL_MAX] = {0};
    env_expand(args + eq + 1, value);

    env_set(name, value);
}

static BOOLEAN cmd_dispatch(EFI_SYSTEM_TABLE *ST, const CHAR16 *line) {
    CHAR16 exp[INPUT_MAX];
    env_expand(line, exp);

    if      (StrCmp (exp, L"help")     == 0) cmd_help();
    else if (StrCmp (exp, L"cls")      == 0) uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    else if (StrnCmp(exp, L"echo", 4)  == 0) Print(L"%s\r\n", StrLen(exp) > 5 ? exp + 5 : L"");
    else if (StrCmp (exp, L"ver")      == 0) cmd_ver(ST);
    else if (StrCmp (exp, L"time")     == 0) cmd_time(ST);
    else if (StrnCmp(exp, L"stall", 5) == 0) cmd_stall(ST, StrLen(exp) > 6 ? exp + 6 : L"");
    else if (StrCmp (line, L"set")     == 0) cmd_set(NULL);
    else if (StrnCmp(line, L"set ", 4) == 0) cmd_set(line + 4);
    else if (StrnCmp(line, L"unset ",6)== 0) {
        if (!env_unset(line + 6)) Print(L"unset: '%s' not found\r\n", line + 6);
    }
    else if (StrCmp (exp, L"reboot")   == 0) uefi_call_wrapper(ST->RuntimeServices->ResetSystem, 4, EfiResetCold,     EFI_SUCCESS, 0, NULL);
    else if (StrCmp (exp, L"poweroff") == 0) uefi_call_wrapper(ST->RuntimeServices->ResetSystem, 4, EfiResetShutdown, EFI_SUCCESS, 0, NULL);
    else if (StrCmp (exp, L"exit")     == 0) return FALSE;
    else if (StrLen (exp) > 0)               Print(L"Unknown command: %s\r\n", exp);

    return TRUE;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST) {
    InitializeLib(ImageHandle, ST);
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);

    CHAR16 line[INPUT_MAX];
    while (TRUE) {
        input_read(ST, line);
        Print(L"\r\n");
        if (StrLen(line) > 0) history_add(line);
        if (!cmd_dispatch(ST, line)) break;
    }
    return EFI_SUCCESS;
}