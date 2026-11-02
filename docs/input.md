# Input preferences, version 1

`$XDG_CONFIG_HOME/holonight/input.toml` (otherwise `$HOME/.config/holonight/input.toml`)
is independent of appearance and Shell. The complete minimal document is:

```toml
version = 1
profile = "standard"
```

The other supported profile is `vim-navigation`. A missing file selects standard.
Existing documents must specify both fields. Unknown keys are reserved for extensions.
Readers retain their last valid profile after malformed TOML, unsupported versions,
invalid values or read failures. Valid replacement and deletion apply live.
This contract does not enable modal text editing or arbitrary remapping.
