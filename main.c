#include <efi.h>
#include <efilib.h>

#define INPUT_MAX 256
#define MAX_TOKENS 16
#define MAX_HISTORY 50

// Command history buffer
CHAR16 history[MAX_HISTORY][INPUT_MAX];
UINTN history_count = 0;
UINTN history_index = 0;


// Add command to history
VOID add_to_history(CHAR16 *cmd) {
  if (history_count < MAX_HISTORY) {
    StrCpy(history[history_count], cmd);
    history_count++;
  } else {
    // Shift history down and add new command at end
    for (UINTN i = 0; i < MAX_HISTORY - 1; i++) {
      StrCpy(history[i], history[i + 1]);
    }
    StrCpy(history[MAX_HISTORY - 1], cmd);
  }
  history_index = history_count;
}

// Custom input function with history support
VOID input_with_history(EFI_SYSTEM_TABLE *SystemTable, CHAR16 *prompt, CHAR16 *line, UINTN max_length) {
  UINTN pos = 0;
  EFI_INPUT_KEY key;
  EFI_STATUS status;

  // Initialize line
  for (UINTN i = 0; i < max_length; i++) {
    line[i] = L'\0';
  }

  Print(prompt);

  while (1) {
    status = uefi_call_wrapper(SystemTable->ConIn->ReadKeyStroke, 2, SystemTable->ConIn, &key);
    
    if (status == EFI_SUCCESS) {
      // Handle arrow keys for history navigation
      if (key.ScanCode == SCAN_UP) {
        if (history_index > 0) {
          history_index--;
        }
        
        // Clear current line
        for (UINTN i = 0; i < pos; i++) {
          Print(L"\b \b");
        }
        
        // Load history line
        StrCpy(line, history[history_index]);
        pos = StrLen(line);
        Print(L"%s", line);
        
      } else if (key.ScanCode == SCAN_DOWN) {
        if (history_index < history_count - 1) {
          history_index++;
          
          // Clear current line
          for (UINTN i = 0; i < pos; i++) {
            Print(L"\b \b");
          }
          
          // Load history line
          StrCpy(line, history[history_index]);
          pos = StrLen(line);
          Print(L"%s", line);
          
        } else if (history_index == history_count - 1) {
          history_index = history_count;
          
          // Clear current line
          for (UINTN i = 0; i < pos; i++) {
            Print(L"\b \b");
          }
          
          // Clear line buffer
          for (UINTN i = 0; i < max_length; i++) {
            line[i] = L'\0';
          }
          pos = 0;
        }
      } else if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
        // Enter key pressed
        break;
      } else if (key.UnicodeChar == CHAR_BACKSPACE) {
        // Backspace key pressed
        if (pos > 0) {
          pos--;
          line[pos] = L'\0';
          Print(L"\b \b");
        }
      } else if (key.UnicodeChar >= 0x20 && key.UnicodeChar < 0x7F && pos < max_length - 1) {
        // Regular character
        line[pos] = key.UnicodeChar;
        pos++;
        line[pos] = L'\0';
        Print(L"%c", key.UnicodeChar);
      }
    }
  }
}


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

    input_with_history(SystemTable, L"uefi> ", line, INPUT_MAX);

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
    } else if (StrLen(line) > 0) {
      Print(L"Unknown command: %s\r\n", line);
    }

    // Add command to history if it's not empty
    if (StrLen(line) > 0) {
      add_to_history(line);
    }
  }

  return EFI_SUCCESS;
}