# Installing Refrakt

Refrakt is distributed as a plain `.zip` file — there's no installer. This
guide walks through placing the plugin file in the right folder for your
system.

## Windows

1. Unzip the download. You should see a folder named `Refrakt.vst3`.
2. Copy `Refrakt.vst3` into:
   ```
   C:\Program Files\Common Files\VST3\
   ```
   You'll need administrator rights to copy into this folder — if Windows
   asks for permission, allow it.
3. Open your DAW and rescan your plugins if it doesn't pick up Refrakt
   automatically (usually somewhere under Options/Preferences → Manage
   Plugins → Rescan or Verify).

**Windows may show a warning when you copy or run files from an unfamiliar
source.** This is expected — Refrakt isn't digitally signed yet. The plugin
itself is safe; this is just Windows being cautious about unsigned
software. If your DAW itself refuses to load it, check your antivirus
settings and allow the plugin folder.

**If the plugin window opens but stays completely blank:** your system is
missing the Microsoft Edge WebView2 Runtime, which Refrakt's interface runs
on. Most up-to-date Windows 10/11 machines already have it. If not, install
it free from Microsoft here, then reopen the plugin:
https://go.microsoft.com/fwlink/p/?LinkId=2124703

## macOS

1. Unzip the download. You should see `Refrakt.vst3` and `Refrakt.component`.
2. Copy them into:
   - VST3: `~/Library/Audio/Plug-Ins/VST3/`
   - AU (Component): `~/Library/Audio/Plug-Ins/Components/`

   (`~/Library` is hidden by default — in Finder, hold **Option** and click
   the **Go** menu to reveal it, or press **Cmd+Shift+G** and type the path.)
3. Open your DAW and rescan your plugins if needed.

**macOS will likely say the plugin "is damaged and can't be opened."**
This is not actual damage — it's macOS Gatekeeper blocking an unsigned,
internet-downloaded file, and it's a known issue with this build. To fix
it:

1. Open **Terminal** (search for it in Spotlight).
2. Run these two commands, one at a time (adjust the path if you placed
   the files somewhere other than the folders above):
   ```
   xattr -cr ~/Library/Audio/Plug-Ins/VST3/Refrakt.vst3
   xattr -cr ~/Library/Audio/Plug-Ins/Components/Refrakt.component
   ```
3. Reopen your DAW — the plugin should now load normally.

## Still stuck?

Contact SkeptikAudio and we'll help you get it running.
