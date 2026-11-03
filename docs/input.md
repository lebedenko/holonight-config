# Shared input preferences

## Navigation style

Choose **Settings → Keyboard → Navigation style** to use Standard (the default) or
Vim navigation in Files, the standalone file chooser, and the Bluetooth popup. The preference
applies live, independently of Shell; text editing remains conventional. Standard
keeps arrow, Home/End, Page Up/Down and Tab navigation. Vim adds h/j/k/l, gg/G,
counts, Ctrl+U/D half pages and Ctrl+H/J/K/L panel focus where supported. In file
lists h/l go to parent/open; in grids they move horizontally. `.` toggles hidden
files; Alt+D edits the location. Text fields and modal dialogs isolate navigation.

The shared preference is in `$XDG_CONFIG_HOME/holonight/config.toml`, or
`~/.config/holonight/config.toml` when XDG_CONFIG_HOME is unset:

```toml
[input]
navigation = "vim-navigation"
```

Use `navigation = "standard"` or remove the key to restore Standard. Missing files
are never created by readers. Valid replacement/deletion applies live; invalid
updates retain the last valid setting and expose reader diagnostics. Settings Save,
Discard, reset and conflict resolution apply to this preference. Edits preserve
Shell sections, comments and unknown keys; appearance stays in its own document.

For manual migration from the unreleased `input.toml` contract, copy its `profile`
value into `[input].navigation` in `config.toml`. The old `version` and `profile`
keys are no longer read. Old files are never deleted automatically. There are no
application-specific navigation overrides.
