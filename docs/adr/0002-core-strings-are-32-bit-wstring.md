# Core strings are 32-bit `std::wstring`

When wxString leaves the core, `String` becomes `std::wstring` with free helper functions, not UTF-8 `std::string` or `std::u16string`. `wchar_t` is 32-bit on both Linux and Android, so a code unit is a code point, exactly as in today's wxGTK build: template scripts that index, slice or measure card text (`substr`, positions, lengths) keep their current results over non-ASCII text. UTF-8 would silently shift those positions, and UTF-16 (which would match Kotlin/Java) changes them for non-BMP characters and fights the `L""` literals behind `_()`. Conversion to UTF-8 happens only at boundaries: file I/O, JNI/FFI, and font/text APIs.

## Consequences

- Every JNI/FFI crossing converts; the core↔UI API should pass strings coarsely, not per character.
- Number parsing/formatting moves to locale-independent helpers (wx's `ToDouble`/`Format` depended on the C locale).
- A free-function API was chosen over a wxString-lookalike class, accepting more call-site churn to avoid re-growing wx-isms.
