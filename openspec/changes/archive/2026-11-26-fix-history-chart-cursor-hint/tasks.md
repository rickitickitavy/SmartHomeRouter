## 1. Tooltip CSS

- [x] 1.1 On `.history-chart-tooltip`, set an explicit normal `line-height` (override wrap `line-height: 0`) so multiline tip text does not collapse
- [x] 1.2 Keep tip readable (padding, `pre-line` or block lines); do not rely on canvas wrap line-height

## 2. Placement + content (operator + simple-user)

- [x] 2.1 In `onHistoryChartPointer` (`data/index.html` and `data/user.html`), keep time and value/state on separate lines (2–3 lines max if date/time split helps)
- [x] 2.2 After content is set, measure tip size and place relative to cursor: prefer right/above; if that side would overflow the canvas wrap, flip to the opposite side; clamp inside the wrap
- [x] 2.3 Verify both pages behave the same for numeric and discrete charts

## 3. Version

- [x] 3.1 Confirm host firmware build is **4.6.39** (apply bump; propose was 4.6.38)
