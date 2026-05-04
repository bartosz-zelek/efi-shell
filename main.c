#include <efi.h>
#include <efilib.h>

#define MAX_INPUT   256
#define MAX_HISTORY 50
#define MAX_ENVVARS 32

CHAR16 g_history[MAX_HISTORY][MAX_INPUT];
int    g_hist_count = 0;
int    g_hist_pos   = 0;

CHAR16 g_env_names [MAX_ENVVARS][64];
CHAR16 g_env_values[MAX_ENVVARS][128];
int    g_env_count = 0;


void history_add(CHAR16 *cmd)
{
    if (g_hist_count < MAX_HISTORY)
    {
        StrCpy(g_history[g_hist_count], cmd);
        g_hist_count++;
    }
    else
    {
        for (int i = 0; i < MAX_HISTORY - 1; i++)
        {
            StrCpy(g_history[i], g_history[i + 1]);
        }
        StrCpy(g_history[MAX_HISTORY - 1], cmd);
    }
    g_hist_pos = g_hist_count;
}


CHAR16 *env_get(CHAR16 *name)
{
    for (int i = 0; i < g_env_count; i++)
    {
        if (StrCmp(g_env_names[i], name) == 0)
        {
            return g_env_values[i];
        }
    }
    return NULL;
}

void env_set(CHAR16 *name, CHAR16 *value)
{
    for (int i = 0; i < g_env_count; i++)
    {
        if (StrCmp(g_env_names[i], name) == 0)
        {
            StrCpy(g_env_values[i], value);
            return;
        }
    }
    if (g_env_count < MAX_ENVVARS)
    {
        StrCpy(g_env_names[g_env_count], name);
        StrCpy(g_env_values[g_env_count], value);
        g_env_count++;
    }
    else
    {
        Print(L"env: table full\r\n");
    }
}

void env_unset(CHAR16 *name)
{
    for (int i = 0; i < g_env_count; i++)
    {
        if (StrCmp(g_env_names[i], name) == 0)
        {
            g_env_count--;
            StrCpy(g_env_names[i],  g_env_names[g_env_count]);
            StrCpy(g_env_values[i], g_env_values[g_env_count]);
            return;
        }
    }
    Print(L"unset: '%s' not found\r\n", name);
}

void env_expand(CHAR16 *src, CHAR16 *dst)
{
    int d = 0;
    for (int s = 0; src[s] != L'\0' && d < MAX_INPUT - 1; s++)
    {
        if (src[s] != L'$')
        {
            dst[d++] = src[s];
            continue;
        }
        s++;
        CHAR16 varname[64];
        int n = 0;
        while (src[s] != L'\0' && n < 63)
        {
            CHAR16 c = src[s];
            int is_letter = (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z');
            int is_digit  = (c >= L'0' && c <= L'9');
            if (!is_letter && !is_digit && c != L'_')
            {
                break;
            }
            varname[n++] = c;
            s++;
        }
        varname[n] = L'\0';
        s--;
        CHAR16 *val = env_get(varname);
        if (val != NULL)
        {
            for (int v = 0; val[v] != L'\0' && d < MAX_INPUT - 1; v++)
            {
                dst[d++] = val[v];
            }
        }
    }
    dst[d] = L'\0';
}

void cmd_set(CHAR16 *arg)
{
    if (arg == NULL || *arg == L'\0')
    {
        if (g_env_count == 0)
        {
            Print(L"(no variables set)\r\n");
        }
        for (int i = 0; i < g_env_count; i++)
        {
            Print(L"%s=%s\r\n", g_env_names[i], g_env_values[i]);
        }
        return;
    }
    int eq = 0;
    while (arg[eq] != L'\0' && arg[eq] != L'=')
    {
        eq++;
    }
    if (arg[eq] != L'=')
    {
        Print(L"Usage: set name=value\r\n");
        return;
    }
    CHAR16 name[64]   = {0};
    CHAR16 value[128] = {0};
    int name_len = eq < 63 ? eq : 63;
    for (int i = 0; i < name_len; i++)
    {
        name[i] = arg[i];
    }
    CHAR16 raw_value[128] = {0};
    int vlen = StrLen(arg + eq + 1);
    if (vlen > 127)
    {
        vlen = 127;
    }
    for (int i = 0; i < vlen; i++)
    {
        raw_value[i] = arg[eq + 1 + i];
    }
    env_expand(raw_value, value);
    env_set(name, value);
}


void input_read(EFI_SYSTEM_TABLE *ST, CHAR16 *line)
{
    int pos = 0;
    EFI_INPUT_KEY key;
    for (int i = 0; i < MAX_INPUT; i++)
    {
        line[i] = L'\0';
    }
    Print(L"uefi> ");
    for (;;)
    {
        if (uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key) != EFI_SUCCESS)
        {
            continue;
        }
        if (key.ScanCode == SCAN_UP)
        {
            if (g_hist_pos > 0)
            {
                g_hist_pos--;
            }
            while (pos-- > 0)
            {
                Print(L"\b \b");
            }
            pos = 0;
            line[0] = L'\0';
            if (g_hist_pos < g_hist_count)
            {
                StrCpy(line, g_history[g_hist_pos]);
                pos = StrLen(line);
                Print(L"%s", line);
            }
        }
        else if (key.ScanCode == SCAN_DOWN)
        {
            if (g_hist_pos < g_hist_count)
            {
                g_hist_pos++;
            }
            while (pos-- > 0)
            {
                Print(L"\b \b");
            }
            pos = 0;
            line[0] = L'\0';
            if (g_hist_pos < g_hist_count)
            {
                StrCpy(line, g_history[g_hist_pos]);
                pos = StrLen(line);
                Print(L"%s", line);
            }
        }
        else if (key.UnicodeChar == CHAR_CARRIAGE_RETURN)
        {
            break;
        }
        else if (key.UnicodeChar == CHAR_BACKSPACE)
        {
            if (pos > 0)
            {
                line[--pos] = L'\0';
                Print(L"\b \b");
            }
        }
        else if (key.UnicodeChar >= 0x20 && key.UnicodeChar < 0x7F)
        {
            if (pos < MAX_INPUT - 1)
            {
                line[pos++] = key.UnicodeChar;
                line[pos]   = L'\0';
                Print(L"%c", key.UnicodeChar);
            }
        }
    }
}


EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST)
{
    InitializeLib(ImageHandle, ST);
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);

    CHAR16 raw[MAX_INPUT];
    CHAR16 line[MAX_INPUT];

    while (1)
    {
        input_read(ST, raw);
        Print(L"\r\n");

        if (StrLen(raw) == 0)
        {
            continue;
        }

        history_add(raw);
        env_expand(raw, line);

        if (StrCmp(line, L"help") == 0)
        {
            Print(L"Commands: help  cls  ver  time  echo <text>\r\n"
                  L"          set [name=value]  unset <name>\r\n"
                  L"          reboot  poweroff  exit\r\n");
        }
        else if (StrCmp(line, L"cls") == 0)
        {
            uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
        }
        else if (StrCmp(line, L"ver") == 0)
        {
            Print(L"UEFI %u.%u  |  %s\r\n",
                (ST->Hdr.Revision >> 16) & 0xFFFF,
                 ST->Hdr.Revision        & 0xFFFF,
                 ST->FirmwareVendor);
        }
        else if (StrCmp(line, L"time") == 0)
        {
            EFI_TIME t;
            uefi_call_wrapper(ST->RuntimeServices->GetTime, 2, &t, NULL);
            Print(L"%04u-%02u-%02u  %02u:%02u:%02u\r\n",
                t.Year, t.Month, t.Day, t.Hour, t.Minute, t.Second);
        }
        else if (StrnCmp(line, L"echo ", 5) == 0)
        {
            Print(L"%s\r\n", line + 5);
        }
        else if (StrCmp(raw, L"set") == 0)
        {
            cmd_set(NULL);
        }
        else if (StrnCmp(raw, L"set ", 4) == 0)
        {
            cmd_set(raw + 4);
        }
        else if (StrnCmp(raw, L"unset ", 6) == 0)
        {
            env_unset(raw + 6);
        }
        else if (StrCmp(line, L"reboot") == 0)
        {
            uefi_call_wrapper(ST->RuntimeServices->ResetSystem, 4,
                EfiResetCold, EFI_SUCCESS, 0, NULL);
        }
        else if (StrCmp(line, L"poweroff") == 0)
        {
            uefi_call_wrapper(ST->RuntimeServices->ResetSystem, 4,
                EfiResetShutdown, EFI_SUCCESS, 0, NULL);
        }
        else if (StrCmp(line, L"exit") == 0)
        {
            break;
        }
        else
        {
            Print(L"unknown command: %s\r\n", line);
        }
    }

    return EFI_SUCCESS;
}