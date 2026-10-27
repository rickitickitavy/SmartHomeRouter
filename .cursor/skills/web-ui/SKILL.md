---
name: web-ui
description: >-
  Web UI conventions for HTML/CSS/JS consoles and apps. Use always when
  implementing or changing a web application UI, layout, styling, tables,
  dialogs, or entity delete/edit flows.
---

# Web UI

Applies when writing or changing web UI.

## Hard rules

- ALWAYS use a recycle-bin icon button (e.g. 🗑 / `.console-widget-delete-btn`) as the default control for remove/delete actions when there are many such controls — not a text button like "Remove" or "Remove row". Keep a confirm dialog before deleting entities.
- NEVER gray enabled inputs, selects, textareas, or buttons; gray/muted is only for disabled or readonly controls. Style enabled textareas like enabled text inputs.
- Checkbox captions MUST NOT toggle the box; only a click on the checkbox square toggles it (do not wrap caption text in `<label for>` / wrapping `<label>` around the input + text).
- Checkboxes MUST align to the **left** unless a screen explicitly specifies another alignment. Prefer `.field-check` (row: square then caption). Never leave a lone checkbox as a stretched child of a column `.field` (that centers the square).
- before deleting any entity make request to user if he shure to delete
- NEVER make a button width the full page or stretch it across its container. ALWAYS use a mindful width (content plus comfortable padding).
- ALWAYS close a dialog when the user clicks outside the dialog (on the overlay).
- ALWAYS check whether the user changed data in a dialog. If they did, ask before closing without saving.
