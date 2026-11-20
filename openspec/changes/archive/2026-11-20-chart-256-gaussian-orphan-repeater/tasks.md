## 1. History chart maxPoints 256

- [x] 1.1 Raise `WebConsole` `/api/history` default and clamp from 128 to **256**
- [x] 1.2 Set `maxPoints=256` on history fetches in `data/index.html` and `data/user.html`

## 2. Gaussian smooth (default samples 3)

- [x] 2.1 Replace flat average smooth with Gaussian `N = 2U+1`, endpoints at `x=±2/3`, σ=1, edge renormalization — on both operator and simple-user pages
- [x] 2.2 Default samples / state / input to **3**; keep Smooth unchecked on open; remove any remaining EMA UI
- [x] 2.3 Update smooth input max if needed so large `U` stays sane relative to 256-point series (keep a practical upper bound)

## 3. Orphan ROUTER reassignment

- [x] 3.1 Detect orphan `routerId` (non-zero, not in current peers); render fixed `<select>` (local + paired) instead of readonly text; paired/local remain readonly
- [x] 3.2 On Save from editable select, POST `routerId`; omit when readonly so server keeps existing
- [x] 3.3 Confirm host `/api/devices` already persists `routerId` on edit when provided (touch only if needed)

## 4. Versions and smoke

- [x] 4.1 Confirm propose host bump **4.5.13**; leave slave **4.2.16** unless apply touches slave (it should not)
- [x] 4.2 Smoke: chart requests 256; Smooth default 3 + Gaussian for U=2 uses 5 taps; orphan device ROUTER select Save reassigns without delete
