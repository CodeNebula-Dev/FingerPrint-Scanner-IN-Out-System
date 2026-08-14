// Residency-Aware Parity State Machine
if (match.is_hosteller) {
    // Hosteller: INSIDE at start of day
    is_in = (log_entry.gate_count % 2 == 0);
} else {
    // Day Scholar: OUTSIDE at start of day
    is_in = (log_entry.gate_count % 2 != 0);
}
