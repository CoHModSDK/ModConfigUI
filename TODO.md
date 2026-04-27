# CoHModConfigUI TODO

## Dropdown Hairline On 4-Item Enums

Status: Deferred for later polish.

Context:
- In the 4-item enum case, the dropdown should behave like a no-scroll variant.
- The major scrollbar behavior is already correct: no active scrolling is needed until there are 5 or more choices.
- A very thin black vertical line can still appear at the right edge of the opened 4-item dropdown.

What was verified on 2026-03-30:
- Cheat Engine inspection showed the scrollbar subtree is effectively collapsed in the 4-item case.
- `btnDec` and `btnInc` were collapsed.
- `btnPgUp` and `btnPgDn` were not acting like a visible scrollbar lane.
- The cloned dropdown item widgets were already using the narrow content width.
- The remaining artifact appears to come from native dropdown/listbox root behavior, not from the populated item text path.

Current code assumptions:
- Scrollbar visibility is controlled by `choiceCount > 4` in `src/UiInterop.cpp`.
- For 4 or fewer choices, the code hides the scrollbar and narrows the no-scroll listbox root/content geometry.
- This fixed the major width mismatch, but not the last hairline seam.

Likely follow-up when revisiting:
- Compare against a native in-game dropdown that opens with exactly 4 visible rows, if one can be found.
- Inspect whether the line is a border/separator from the listbox root Presentation rather than a true scrollbar child.
- If needed, adjust the no-scroll listbox root styling/state separately from the scrollbar subtree.
