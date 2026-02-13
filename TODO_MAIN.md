# TODO (Main)

## Shutdown Subsystem Follow-ups
- [ ] Register a QFileSystem shutdown listener (using `registerSubsystem`) that walks open handles and flushes dirty buffers before acknowledging a shutdown request.
- [ ] Register future app/runtime subsystems (editors, desktop apps, etc.) so they can veto shutdown until their own unsaved state is resolved.
