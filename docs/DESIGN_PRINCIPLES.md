# Design principles

Five rules, derived from the one rule.

## 1. One way to do each thing

If there are two ways to move a window, one of them is wrong. Pick the better
one and delete the other. Familiarity alone is not a reason to keep both.

## 2. Nothing is hidden that matters

Linux's transparency is the trait worth stealing. Every piece of state the
system acts on should be inspectable and, where sane, editable as plain text.
No binary registry. No setting that exists only in a GUI without an underlying
model.

## 3. Restraint over expressiveness

A coherent visual system comes from saying no. Use a fixed palette, spacing
scale, typography scale and motion language. Shared design tokens should enforce
that discipline rather than relying on memory.

## 4. Never fake capability

A toggle that does nothing, a menu item that opens an empty dialog, or a
progress bar that is not measuring anything is worse than an absence because it
teaches the user that the system lies.

## 5. The system explains itself

Errors say what happened and what to do. The terminal is a first-class citizen,
not merely an escape hatch. Important state and actions should be traceable.
