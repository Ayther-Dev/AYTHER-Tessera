# API 1.1: superficie de observación

Spec 008, RF-3.0 (plan §1.4). La API 1.1 es aditiva: `TESSERA_ABI_MINOR` pasa de 0 a 1 y `tessera_api_query_v1` acepta pedir 1.0 o 1.1. Todas las funciones llevan el `session` primero y el `tessera_context_v1` al final, y devuelven `tessera_status`. Los ids de región (1 a 26), los flags, las versiones de layout y los límites están en `include/tessera/tessera_observation.h`, generado desde `ayther-abi-1.10-r4`.

Cada función corresponde a un miembro de `ayther_interface_v1`; la semántica de los argumentos es la del contrato de gpgx (`docs/ayther_abi_v1.md`).

| Función | Miembro de `ayther_interface_v1` | Notas |
|---|---|---|
| `tessera_observation_region_query_v1` | `query_region` | Tamaño y flags de una región. |
| `tessera_observation_region_read_v1` | `read_region` | `expected_generation` para lecturas coherentes. |
| `tessera_observation_write_control_v1` | `write_control` | Solo `LAYER_MASK`, `LAYER_DIM`, `AUDIO_MUTE` y el poke de work RAM; las regiones 6 a 9 devuelven `TESSERA_UNSUPPORTED_CAPABILITY` (PQ-1). |
| `tessera_observation_snapshot_capture_v1` | `capture_snapshot` | Instantánea del frame. |
| `tessera_observation_recompose_v1` | `recompose_frame` | Recomposición simple. |
| `tessera_observation_recompose_multilayer_v1` | `recompose_multilayer` | Capas separadas. |
| `tessera_observation_recompose_stats_v1` | `get_recompose_stats` | Estadísticas de recomposición. |
| `tessera_observation_audio_events_poll_v1` | `poll_audio_events` | Lleva `event_size` para validar el layout del evento (`ayther_audio_event_v1` contiene una unión y no se genera aún). |
| `tessera_observation_audio_transport_stats_v1` | `get_audio_transport_stats` | |
| `tessera_observation_subscriptions_get_v1` / `_set_v1` | `get_subscriptions` / `set_subscriptions` | Máscara `TESSERA_OBS_SUB_*`. |
| `tessera_observation_frame_delta_poll_v1` | `poll_frame_delta` | Delta del último frame (TP-205.4). |
| `tessera_observation_frame_delta_since_v1` | `frame_delta_since` | `AYTHER_STATUS_DELTA_HISTORY_LOST` se devuelve como `TESSERA_STALE_EPOCH`. |

## Estado de la implementación
Esta tarea (TP-006.4) declara y documenta; ninguna función tiene aún definición. Las implementa el backend Mega Drive en la fase 1 (TP-101 a TP-107) y la sesión sintética devolverá `TESSERA_UNSUPPORTED_CAPABILITY`.

## Restauración de estado sin traza (TP-007.2)

```c
tessera_status TESSERA_CALL tessera_state_restore_v1(
    tessera_session session, tessera_bytes_v1 state, tessera_context_v1 *ctx);
```

- Restaura un savestate `TSSTATE1` (`tessera.megadrive.v1`) sin exigir una traza. `tessera_checkpoint_restore_v1`, atado a `TSTRACE1`, se conserva para el CLI y FX-01.
- `TESSERA_CHECKPOINT_INCOMPATIBLE` (-20): generación o codec distintos de los de la sesión.
- `TESSERA_CHECKPOINT_INVALID` (-21): bytes mal formados o truncados; la sesión queda sin cambios.
- `TESSERA_WRONG_STATE` (-5): la sesión no admite restauración en su fase actual.
- Tamaño máximo: 2 MiB (RNF-2); `limits_v1.snapshot_bytes` lo publica.
