# Josh OS engineering rules

Work on real implementation, not apparent progress.

- Inspect source before changing it. Do not treat READMEs, roadmaps, checkboxes,
  scaffolding, commit messages, or earlier AI summaries as proof that behaviour works.
- Repository ownership is strict: JoshBIOS owns firmware/BIOS/bootloader; AshFallen
  owns the native kernel and canonical OS/product; JoshOS-Stage0 is only the rapid
  Linux/browser prototype.
- Do not grow the JoshBIOS test payload into a competing OS.
- Prefer the smallest coherent implementation plus automated proof.
- Keep main/builds green. Do not disable meaningful checks just to make CI pass.
- Distinguish Builds, Tested, Integrated and Verified. Compilation alone is not a
  successful boot or hardware test.
- Keep Limine working as the reference AshFallen boot path while JoshBootloader is
  developed.
- Never flash physical firmware or modify physical disks as part of an autonomous
  task. Hardware/firmware work must remain explicitly user-controlled.
- Do not push, merge, rewrite history, install system packages, use sudo, or alter
  files outside the current Git worktree. Josh Developer Mode handles publication
  and candidate staging separately.
- When finished, run the repository's existing tests/checks where practical and
  leave a concise summary of what changed and what remains unverified.
