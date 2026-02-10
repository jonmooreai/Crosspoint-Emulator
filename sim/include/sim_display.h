#pragma once

// Call before setup() so HalDisplay::begin() can use the window.
bool sim_display_init(void);
void sim_display_shutdown(void);
// Process SDL events (keyboard, etc.). Returns false if user requested quit.
bool sim_display_pump_events(void);
