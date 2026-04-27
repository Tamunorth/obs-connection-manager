# OBS Connection Manager Roadmap

An OBS plugin that monitors network upload health during a live stream and automatically adapts the encoder bitrate to keep the stream alive when bandwidth degrades. Companion plugin to OBS Squeezeback.

## Planned

### Adaptive Bitrate Control

Watch OBS's network/upload health signal (the same indicator that turns red when the upload buffer congests) and step the streaming bitrate up or down to match available bandwidth.

- **Step down** when the network indicator goes red. Each step reduces bitrate by the configured step size. Continues stepping down until the indicator clears or the lower limit is reached.
- **Step up** when the network has been healthy for a sustained window. Restores bitrate gradually toward the upper limit so quality recovers as the network recovers.
- **Step size:** default `1000 kbps`, user-configurable.
- **Example flow:** streaming at 6000 kbps, indicator goes red, drop to 5000, still red, drop to 4000, ... until network stabilizes. Then on sustained recovery, climb back: 4000 → 5000 → 6000.
- **Health signal source:** likely OBS's existing dropped-frames / network congestion API (`obs_output_get_congestion`, dropped frame counters). To be confirmed during implementation.

### User-Configurable Limits

- **Upper limit (max bitrate):** the ceiling. The manager will never push above this, even when the network is perfectly healthy.
- **Lower limit (min bitrate):** the floor. The manager will never drop below this, even if the network is collapsing. Below this point, the stream is treated as broken and notification logic kicks in.
- **Step size:** how much to add/remove per adjustment. Default 1000 kbps.
- **Recovery window:** how long the network must be clean before stepping back up (prevents oscillation). Sensible default + user override.

### Stream-Breaking Notifications

When upload bandwidth has effectively collapsed (no usable upload, or sustained at the lower limit with continued congestion), surface a clear, immediate alert to the operator.

- **Pop-up / desktop notification** informing the user that the stream is breaking.
- Distinct from the routine adaptive-bitrate adjustments (those should be silent or logged only).
- Trigger condition to refine during implementation: e.g. at lower limit AND still red for N seconds, OR upload throughput at zero.

### Settings UI

OBS plugin settings panel exposing:
- Enable / disable adaptive control
- Upper bitrate limit
- Lower bitrate limit
- Step size (kbps per adjustment)
- Step-down sensitivity (how quickly to react to red)
- Step-up recovery window (how long clean before stepping up)
- Enable / disable stream-breaking notification
- Notification style (toast, modal, both)

## Open Questions

- Which OBS streaming API to drive bitrate changes live (encoder settings update vs. service settings update) without restarting the stream
- Best signal for "network is bad": OBS congestion value, dropped frames rate, RTMP buffer depth, or a composite
- Whether to log every adjustment to a session report so the user can review what happened during a stream

## Out of Scope (for v1)

- Multi-platform streaming (handle one output at a time)
- Recording bitrate (this is for streaming only)
- Network speedtest probing (we react to OBS's own signal, not active probes)
