# vita-shellbat
A remake of taiHEN plugin that shows battery percent in statusbar and new controller battery indicators:
- Player 1 only
- Shows when PS controller is bluetooth connected - tested with DS4
- This mod should be compatible on all versions - tested on 3.70

### Installation
Add this plugin under `*main` section in `ur0:tai/config.txt` or `ux0:tai/config.txt`:
```
*main
ur0:tai/shellbat.suprx
```

### Controller Battery Indicators
![image](https://user-images.githubusercontent.com/78706679/125861506-779f7496-1fef-419f-ae00-30ac1b872b63.png)
- Battery is fully charged [|||]
- Battery is charging [| | |] (stepped pulse)
- Battery level is 5 [|| |] (last indicator pulse)
- Battery level is 4 [|| ]
- Battery level is 3 [| | ] (last indicator pulse)
- Battery level is 2 [|  ] (full pulse with last indicator)
- Battery level is 1 [   ] (full pulse without last indicator)
- Battery level is 0 - dead and disconnected

# Credits
Princess-of-Sleeping - for forked [shellbat](<https://github.com/Princess-of-Sleeping/vita-shellbat>)

nowrep - for original [shellbat](<https://github.com/nowrep/vita-shellbat>)
