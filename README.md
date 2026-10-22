# QTBUG-58059: Qt WebEngine 6.x eMudhra G3 Certificate Distrust Reproducer & Workaround

Minimal CMake + Qt WebEngine Quick reproducer showing that **Qt WebEngine 6.11.0** (`Chromium 140.0.7339.225`) still trusts TLS certificates chaining to **`emSign ECC Root CA - G3`** (the root above the 8 compromised eMudhra G3 intermediate CAs disclosed in [October 2026](https://emudhra.we-have-a-problem.com/)), even on an OS with updated system certificates.

It also demonstrates a runtime workaround for existing Qt 6.x binaries without rebuilding Qt WebEngine, using Chromium's built-in `--test-crs-constraints` switch.

## Why Qt WebEngine 6.11 is affected on all desktop platforms

1. **Compiled-in `CRLSet` is an empty stub when `component_updater` is disabled:**
   In `chromium/net/cert/crl_set.cc` (`CRLSet::BuiltinCRLSet()`), the built-in CRLSet is:
   ```text
   "\x31\x00{\"ContentType\":\"CRLSet\",\"Sequence\":0,\"Version\":0}"
   ```
   And the static SPKI blocklist update in `net/cert/cert_verify_proc_blocklist.inc` (plus the removal of `emSign ECC Root CA - G3` from `net/data/ssl/chrome_root_store/root_store.textproto`) has not yet been backported to `qtwebengine-chromium`.

2. **Updating OS system certificates does not remove trust in Qt WebEngine on desktop:**
   In `chromium/net/features.gni`, desktop builds use `chrome_root_store_only = is_win || is_mac || is_linux || is_chromeos`.
   `SystemTrustStoreChrome` combines `trust_store_system` and `trust_store_chrome_` via `bssl::TrustStoreCollection`. Removing a root from the OS store only makes `trust_store_system` return `UNSPECIFIED`, so verification falls through to `trust_store_chrome_`, where `emSign ECC Root CA - G3` (`86a1ecba089c4a8d3bbe2734c612ba341d813e043cf9e8a862cd5c57a36bbe6b`) is compiled in as a `TRUSTED_ANCHOR`.

## Build

```bash
cmake -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.0/<platform> -B build -S .
cmake --build build
```

## Run

### 1. Default Qt 6.11 behavior (vulnerable — page loads without certificate error)

```bash
./build/qtbug58059_repro
```

Output:
```text
qml: RESULT=VULNERABLE_LOAD_SUCCEEDED certErrorSeen=false url=https://testovg3.emsign.com/RootOVG3.html
```

### 2. With Chrome Root Store constraint workaround (`--fix`)

Passes `--test-crs-constraints=86a1ecba089c4a8d3bbe2734c612ba341d813e043cf9e8a862cd5c57a36bbe6b:maxversionexclusive=0` via `QTWEBENGINE_CHROMIUM_FLAGS` before `QtWebEngineQuick::initialize()`:

```bash
./build/qtbug58059_repro --fix
```

Output:
```text
qml: RESULT=BLOCKED_BY_CERT_ERROR type=-202 description="Server's certificate is not trusted" url=https://testovg3.emsign.com/
qml: RESULT=LOAD_FAILED certErrorSeen=true errorCode=-202 errorString=net::ERR_CERT_AUTHORITY_INVALID url=https://testovg3.emsign.com/
```
