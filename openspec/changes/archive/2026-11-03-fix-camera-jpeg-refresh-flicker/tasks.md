# Tasks

## 1. JPEG refresh without blanking

- [x] 1.1 In `data/index.html` `buildConsoleCameraView`, change the JPEG interval path to preload a cache-busted snapshot into an off-DOM `Image` and assign to the visible `img` only on successful `onload`; leave the visible frame unchanged on `onerror`; verify Place JPEG cells no longer blank between refreshes
- [x] 1.2 Apply the same preload-then-assign logic in `data/user.html` `buildConsoleCameraView`; verify the simple-user page matches Place (no blank between refreshes, failed load keeps last frame)
- [x] 1.3 Confirm Live mode and non-HTTP live URL messaging are unchanged on both pages after the JPEG change

## Workflow follow-up

- Archive the change after implementation and review.
- Sync delta specs into main `web-console` when archiving (or via `/opsx-sync`).
