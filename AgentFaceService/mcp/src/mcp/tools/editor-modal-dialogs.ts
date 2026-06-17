import { execFile } from 'node:child_process';
import { promisify } from 'node:util';

const execFileAsync = promisify(execFile);

export interface EditorModalDialogDismissFallbackResult {
  source: 'windows_os_window_fallback';
  available: boolean;
  attempted: boolean;
  candidate_modal_windows?: number;
  dismissed_modal_windows: number;
  dismissed_modal_titles: string[];
  remaining_modal_titles?: string[];
  has_remaining_modal: boolean;
  remaining_modal_title?: string;
  remaining_modal_window?: {
    handle?: number;
    title?: string;
    class_name?: string;
    has_owner?: boolean;
  };
  error?: string;
}

const WINDOWS_MODAL_DISMISS_SCRIPT = String.raw`
$ErrorActionPreference = 'Stop'
$utf8NoBom = New-Object System.Text.UTF8Encoding -ArgumentList $false
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class BlueprintHelperModalWindowApi {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll")]
    public static extern IntPtr GetWindow(IntPtr hWnd, uint uCmd);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
}
"@

$script:dismissed = @()
$script:remaining = @()
$GW_OWNER = 4
$WM_CLOSE = 0x0010
$modalTitlePattern = '^(Message|Warning|Error|Confirm|Question|\u6d88\u606f|\u8b66\u544a|\u9519\u8bef|\u63d0\u793a|\u786e\u8ba4|\u95ee\u9898)$'
$mainWindowTitlePattern = '(Unreal Editor|UnrealEditor| - Unreal Editor| - UnrealEditor)'
$slateWindowClassPattern = '^(UnrealWindow|#32770)$'

function Get-WindowTextValue([IntPtr]$Handle) {
    $builder = New-Object System.Text.StringBuilder 512
    [void][BlueprintHelperModalWindowApi]::GetWindowText($Handle, $builder, $builder.Capacity)
    return $builder.ToString()
}

function Get-WindowClassValue([IntPtr]$Handle) {
    $builder = New-Object System.Text.StringBuilder 256
    [void][BlueprintHelperModalWindowApi]::GetClassName($Handle, $builder, $builder.Capacity)
    return $builder.ToString()
}

function Get-UnrealEditorModalWindows {
    $found = New-Object System.Collections.Generic.List[object]
    [BlueprintHelperModalWindowApi]::EnumWindows({
        param([IntPtr]$hWnd, [IntPtr]$lParam)
        if (-not [BlueprintHelperModalWindowApi]::IsWindowVisible($hWnd)) {
            return $true
        }

        $processId = 0
        [void][BlueprintHelperModalWindowApi]::GetWindowThreadProcessId($hWnd, [ref]$processId)
        try {
            $process = Get-Process -Id $processId -ErrorAction Stop
        } catch {
            return $true
        }

        if ($process.ProcessName -notlike 'UnrealEditor*') {
            return $true
        }

        $title = Get-WindowTextValue $hWnd
        $owner = [BlueprintHelperModalWindowApi]::GetWindow($hWnd, $GW_OWNER)
        $className = Get-WindowClassValue $hWnd
        $hasTitle = -not [string]::IsNullOrWhiteSpace($title)
        $titleLooksLikeModal = $hasTitle -and ($title -match $modalTitlePattern)
        $looksLikeOwnedSlateDialog = ($owner -ne [IntPtr]::Zero) -and (($className -match $slateWindowClassPattern) -or (-not $hasTitle))
        $looksLikeModal = $looksLikeOwnedSlateDialog -or $titleLooksLikeModal
        $looksLikeMainEditorWindow = ($owner -eq [IntPtr]::Zero) -and $hasTitle -and ($title -match $mainWindowTitlePattern)
        if (-not $looksLikeModal) {
            return $true
        }
        if ($looksLikeMainEditorWindow) {
            return $true
        }

        $found.Add([pscustomobject]@{
            handle = $hWnd.ToInt64()
            process_id = $processId
            title = $title
            class_name = $className
            has_owner = ($owner -ne [IntPtr]::Zero)
        })
        return $true
    }, [IntPtr]::Zero) | Out-Null
    return $found
}

$targets = Get-UnrealEditorModalWindows
foreach ($target in $targets) {
    [void][BlueprintHelperModalWindowApi]::SendMessage([IntPtr]$target.handle, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
    $script:dismissed += $target
}

Start-Sleep -Milliseconds 150
$script:remaining = @(Get-UnrealEditorModalWindows)

[pscustomobject]@{
    source = 'windows_os_window_fallback'
    available = $true
    attempted = $true
    candidate_modal_windows = $targets.Count
    dismissed_modal_windows = $script:dismissed.Count
    dismissed_modal_titles = @($script:dismissed | ForEach-Object { if ([string]::IsNullOrWhiteSpace($_.title)) { "$($_.class_name)#$($_.handle)" } else { $_.title } })
    has_remaining_modal = ($script:remaining.Count -gt 0)
    remaining_modal_titles = @($script:remaining | ForEach-Object { if ([string]::IsNullOrWhiteSpace($_.title)) { "$($_.class_name)#$($_.handle)" } else { $_.title } })
    remaining_modal_title = if ($script:remaining.Count -gt 0) { if ([string]::IsNullOrWhiteSpace($script:remaining[0].title)) { "$($script:remaining[0].class_name)#$($script:remaining[0].handle)" } else { $script:remaining[0].title } } else { $null }
    remaining_modal_window = if ($script:remaining.Count -gt 0) { $script:remaining[0] } else { $null }
} | ConvertTo-Json -Compress -Depth 4
`;

export async function dismissUnrealEditorModalDialogsByOsWindow(): Promise<EditorModalDialogDismissFallbackResult> {
  if (process.platform !== 'win32') {
    return {
      source: 'windows_os_window_fallback',
      available: false,
      attempted: false,
      dismissed_modal_windows: 0,
      dismissed_modal_titles: [],
      has_remaining_modal: false,
      error: `OS-window modal dismissal is only available on Windows; current platform is ${process.platform}.`,
    };
  }

  try {
    const { stdout } = await execFileAsync(
      'powershell.exe',
      ['-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-Command', WINDOWS_MODAL_DISMISS_SCRIPT],
      {
        timeout: 5000,
        windowsHide: true,
        maxBuffer: 1024 * 1024,
      },
    );
    const parsed = JSON.parse(stdout.trim()) as Partial<EditorModalDialogDismissFallbackResult>;
    return {
      source: 'windows_os_window_fallback',
      available: parsed.available ?? true,
      attempted: parsed.attempted ?? true,
      candidate_modal_windows: parsed.candidate_modal_windows,
      dismissed_modal_windows: parsed.dismissed_modal_windows ?? 0,
      dismissed_modal_titles: parsed.dismissed_modal_titles ?? [],
      remaining_modal_titles: parsed.remaining_modal_titles,
      has_remaining_modal: parsed.has_remaining_modal ?? false,
      remaining_modal_title: parsed.remaining_modal_title,
      remaining_modal_window: parsed.remaining_modal_window,
    };
  } catch (err) {
    return {
      source: 'windows_os_window_fallback',
      available: true,
      attempted: true,
      dismissed_modal_windows: 0,
      dismissed_modal_titles: [],
      has_remaining_modal: false,
      error: err instanceof Error ? err.message : String(err),
    };
  }
}
