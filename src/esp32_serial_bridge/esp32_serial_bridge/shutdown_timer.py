"""One-shot shutdown timer — no ROS, no I/O, fully unit-testable."""


class ShutdownTimer:
    """One-shot deadline: returns True once when elapsed >= deadline; resets on recovery."""

    def __init__(self, shutdown_after_s: float = 30.0):
        """Initialise timer with a positive deadline in seconds."""
        if shutdown_after_s <= 0.0:
            raise ValueError('shutdown_after_s must be positive')
        self.shutdown_after_s = shutdown_after_s
        self.shutdown_requested = False

    def update(self, in_cutoff: bool, elapsed_s: float) -> bool:
        """Return True exactly once when the deadline is reached in cutoff."""
        if elapsed_s < 0.0:
            raise ValueError('elapsed_s must be non-negative')
        if not in_cutoff:
            self.shutdown_requested = False
            return False
        if elapsed_s >= self.shutdown_after_s and not self.shutdown_requested:
            self.shutdown_requested = True
            return True
        return False
