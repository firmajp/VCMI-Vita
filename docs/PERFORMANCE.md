# v037 measurements and next steps

Internal device-session analysis, not a benchmark guarantee. Hardware: Vita 2000, 3.65 Ensō. No user logs or save files are distributed.

| Scene | Windows / frames | Median FPS | Draw | Upload |
| --- | --- | --- | --- | --- |
| Adventure map | 8 / 2231 | 29.26 | 14.74 ms | 10.62 ms |
| Town | 4 / 745 | 18.575 | 42.53 ms | 10.73 ms |

Town painting accounted for 38,831.5 ms across 926 calls (about 41.9 ms/call, maximum 43.34 ms). Input dispatch was small: about 0.07 ms in town and 0.27 ms on the map. Town composition is the clearest next target. Instrument static background/buildings versus animated layers, palette conversion and blits, then cache unchanged layers while preserving animations.

Map steady-state performance is close to 30 FPS. Next measure movement/path-selection spikes and worst frame times, rather than optimizing only average FPS. Battle work can follow after town/map improvements; full-screen uploads still cost about 11 ms and cold asset/audio spikes deserve separate measurements.

Startup wall time: 17.758 seconds; splash visible about 458 ms after main entry, first frame 324 ms. Legacy text load remains about 5.579 seconds, palette preload about 1.995 seconds for 1536 frames. Earlier v036 startup was 19.848 seconds, but legacy text changed little (about 5.620 seconds previously); this sample does not prove that line scanning explains the total improvement. Optimize measured costs without simply moving them to first map or battle interaction.

No optimization is introduced by this publication package. Compare future versions on the same save, town and movement sequence, including large battles/sieges and cold/warm startup.
