# Emulator Control Stdio

The emulator supports a persistent machine-readable control channel with `--control-stdio`.

## Start

```powershell
.\build\Release\crosspoint_emulator.exe --control-stdio
```

If you need to let delayed UI work complete during automation, you can raise the delay cap:

```powershell
.\build\Release\crosspoint_emulator.exe --control-stdio --delay-cap-ms 60000
```

For deterministic test runs, start from a clean emulator session state:

```powershell
.\build\Release\crosspoint_emulator.exe --control-stdio --reset-session
```

`--reset-session` clears runtime-generated session data under `sdcard/.crosspoint/` while preserving explicitly
installed extensions and other manually managed files.

After the first interactive screen is ready, the emulator emits a JSON event prefixed with `[SIMCTL]`:

```text
[SIMCTL] {"ok":true,"event":"ready","state":{...}}
```

## Commands

Send one command per line through stdin:

```text
GET_STATE
GET_SCREEN
TYPE_TEXT settings
WAIT_MS 900
PRESS DOWN
RELEASE DOWN
RELEASE_ALL
HOLD LEFT 900
TAP CONFIRM 180
GO_BACK 10000
WAIT_ACTIVITY Settings 30000
WAIT_HEADER Settings 10000
WAIT_BODY Wi-Fi 10000
WAIT_LIST_ITEM Settings 15000
WAIT_MENU_ITEM File Transfer
WAIT_SELECTED_LIST_ITEM Settings 5000
WAIT_SELECTED_MENU_ITEM Settings 5000
WAIT_POPUP Saved 10000
ACTIVATE_VISIBLE_LIST_ITEM Settings
ACTIVATE_VISIBLE_MENU_ITEM File Transfer
SCREENSHOT D:\temp\emu.bmp
QUIT
```

## Events

The emulator writes structured responses back to stdout, prefixed with `[SIMCTL]`:

```text
[SIMCTL] {"ok":true,"event":"state","state":{...}}
[SIMCTL] {"ok":true,"event":"screen","screen":{...}}
[SIMCTL] {"ok":true,"event":"text_typed","text":"settings"}
[SIMCTL] {"ok":true,"event":"wait_started","waitType":"delay","value":"900ms","timeoutMs":900}
[SIMCTL] {"ok":true,"event":"wait_progress","waitType":"menu_item","value":"File Transfer","elapsedMs":1000,"timeoutMs":30000,"state":{...}}
[SIMCTL] {"ok":true,"event":"wait_satisfied","waitType":"delay","value":"900","matched":"900ms","state":{...}}
[SIMCTL] {"ok":true,"event":"action_started","action":"hold","button":"LEFT","durationMs":900}
[SIMCTL] {"ok":true,"event":"action_completed","action":"hold","value":"","matched":"LEFT","state":{...}}
[SIMCTL] {"ok":true,"event":"tapped","button":"CONFIRM","durationMs":180}
[SIMCTL] {"ok":true,"event":"released_all"}
[SIMCTL] {"ok":true,"event":"action_started","action":"go_back","timeoutMs":10000}
[SIMCTL] {"ok":true,"event":"action_completed","action":"go_back","matched":"Home","state":{...}}
[SIMCTL] {"ok":true,"event":"wait_started","waitType":"menu_item","value":"File Transfer","timeoutMs":30000}
[SIMCTL] {"ok":true,"event":"wait_satisfied","waitType":"menu_item","value":"File Transfer","matched":"File Transfer","state":{...}}
[SIMCTL] {"ok":true,"event":"action_started","action":"activate_visible_list_item","value":"Settings"}
[SIMCTL] {"ok":true,"event":"action_completed","action":"activate_visible_list_item","value":"Settings","matched":"Settings","state":{...}}
[SIMCTL] {"ok":false,"event":"wait_timeout","waitType":"popup","value":"Saved","state":{...}}
```

## State Shape

The returned `state` object contains:

```json
{
  "millis": 1229,
  "currentActivity": "Home",
  "pendingAction": "none",
  "pendingActivity": "",
  "stackActivities": [],
  "screen": {
    "activityName": "Home",
    "headerTitle": "",
    "headerSubtitle": "",
    "subHeaderLabel": "",
    "subHeaderRightLabel": "",
    "bodyPrimaryText": "",
    "bodySecondaryText": "",
    "bodyTertiaryText": "",
    "popupMessage": "",
    "buttonHints": {
      "btn1": "",
      "btn2": "Select",
      "btn3": "Up",
      "btn4": "Down"
    },
    "buttonMenu": {
      "selectedIndex": 0,
      "selectedLabel": "Browse Files",
      "labels": ["Browse Files", "Recent Books", "File Transfer", "Settings"]
    },
    "list": {
      "itemCount": 0,
      "selectedIndex": -1,
      "selectedVisibleIndex": -1,
      "selectedTitle": "",
      "selectedSubtitle": "",
      "selectedValue": "",
      "visibleItems": []
    }
  }
}
```

`GET_SCREEN` returns only the `screen` object when automation does not need the full process state.

Wait commands use case-insensitive substring matching:

- `WAIT_HEADER` checks `headerTitle` and `headerSubtitle`
- `WAIT_BODY` checks `bodyPrimaryText`, `bodySecondaryText`, and `bodyTertiaryText`
- `WAIT_LIST_ITEM` checks visible list item `title`, `subtitle`, and `value`
- `WAIT_MENU_ITEM` checks `buttonMenu.labels`
- `WAIT_SELECTED_LIST_ITEM` checks the currently selected visible list item
- `WAIT_SELECTED_MENU_ITEM` checks `buttonMenu.selectedLabel`
- `WAIT_POPUP` checks `popupMessage`

`WAIT_*` commands are barriers. If you pipe a full script into stdin, the emulator pauses later queued commands until
the active wait succeeds or times out. While a wait is still pending, the emulator emits `wait_progress` roughly once
per second with the latest compact `state`, so CLI harnesses can show what the emulator is currently waiting on instead
of appearing idle.

Before `ready` is emitted, mutating commands such as `ACTIVATE_VISIBLE_*`, `PRESS`, `TAP`, and `TYPE_TEXT` stay queued
instead of failing against the transient boot screen. Read-only inspection commands and `WAIT_*` commands still run
immediately, so a harness can safely begin with waits while startup settles.

Read-only inspection commands still pass through barriers:

- `GET_STATE`
- `GET_SCREEN`
- `SCREENSHOT`

`WAIT_MS` is a pure timing barrier for automation flows that need real elapsed time on the emulator, such as testing
long-press behavior, delayed popups, or background work transitions.

`HOLD <button> <ms>` is a stateful long-press helper. It presses the button immediately, keeps it held for the
requested duration, then releases it and completes as a barrier event.

`ACTIVATE_VISIBLE_LIST_ITEM` and `ACTIVATE_VISIBLE_MENU_ITEM` are state-driven convenience commands. They navigate step
by step using the current selected item, wait for selection changes to settle, and only then press confirm. They also
behave as barriers, so later queued commands will wait until activation completes or fails. For actions that stay on
the same screen, completion is detected from stable UI state changes such as menu, list, body, or popup updates.

`GO_BACK` is a state-driven back-navigation helper. It taps the back button, waits for the activity or header to
change, and retries until the target screen changes or the timeout is reached.

`TYPE_TEXT` forwards text semantically to the current activity. Right now this is implemented for
`KeyboardEntryActivity`, which means automation can fill search, password, or URL prompts without simulating each key
press.

`RELEASE_ALL` is a safety command for automation harnesses. It clears every held button and cancels queued tap steps,
which is useful after interrupted long-press or manual `PRESS` testing.

## Notes

- `[SIMCTL]` lines are intended for automation; other log lines may still be present.
- `--state-json` can still be used in parallel for file-based snapshots.
- Script-based input remains supported, but new automation should prefer `--control-stdio`.
