"""Battery cutoff state machine — no ROS, no I2C, fully unit-testable."""
from dataclasses import dataclass
from dataclasses import field


@dataclass
class BatteryDecision:
    """Result of a single BatteryGuard.update() call."""

    in_cutoff: bool
    entered_cutoff: bool = field(default=False)
    recovered: bool = field(default=False)


class BatteryGuard:
    """Battery cutoff state machine with hysteresis; ignores stale readings."""

    def __init__(
        self,
        cutoff_voltage: float = 9.9,
        recovery_voltage: float = 10.2,
    ):
        """Initialise guard with cutoff and recovery voltage thresholds."""
        if recovery_voltage <= cutoff_voltage:
            raise ValueError(
                f'recovery_voltage ({recovery_voltage}) must be greater than '
                f'cutoff_voltage ({cutoff_voltage})'
            )
        self.cutoff_voltage = cutoff_voltage
        self.recovery_voltage = recovery_voltage
        self.in_cutoff = False

    def update(self, voltage: float, fresh: bool) -> BatteryDecision:
        """Update cutoff state; returns a BatteryDecision with any transition flags."""
        if not fresh or voltage <= 0.0:
            return BatteryDecision(in_cutoff=self.in_cutoff)

        if not self.in_cutoff and voltage < self.cutoff_voltage:
            self.in_cutoff = True
            return BatteryDecision(in_cutoff=True, entered_cutoff=True)

        if self.in_cutoff and voltage >= self.recovery_voltage:
            self.in_cutoff = False
            return BatteryDecision(in_cutoff=False, recovered=True)

        return BatteryDecision(in_cutoff=self.in_cutoff)
