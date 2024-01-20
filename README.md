# tunetest
ATU-10 Antenne tuning testing framework for Mac, Linux or Windows, gcc or clang only

This is a test framework for the ATU-10 tuner. It simulates the effect of the tuner on a variety of simulated antenna impedances and then lets you view the results. You can also zoom into an individual tune and see where the algorithm looked in the inductor/capacitor search space.

The _getch() section needs to be commented out for Windows and commented in for Mac or Linux.

This program uses ANSI console commands to draw the text. For Windows this requires the following to be added to the Windows Registry: HKEY_CURRENT_USER\Console\ add the DWORD VirtualTerminalLevel to equal 1.

When run, the console window needs to be increased in height to about 35 rows.

The initial view is the Resistance/Reactance view. It shows the SWR for all the test resistances and reactances. The values are shown with a single digit or a + or - sign. The legend to the right helps decode the values.

For reference the keyboard commands available are in the lower right.

A number of graph values are shown:
* Best - the best possibly tuned value, created by trying every L or C combination
* Tune1 - the original tuner algoirthm (in tune1.c) for use as reference
* Tune2 - the modified tuner algorithm (in tune2.c)
* Count1 - how many relay switches required to tune1
* Count2 - how many relay switches required to tune2
* Combinations of these are also avaialable for examble Tune1-Best

* To change the view to another frequency, use the f key(to increase) or F key (to decrease)
* A variety of graphs types can be selected, use the g key (to select next) or G key (to select previous) graph. The comparison graph "Tuned1 - Tuned2" has the varient "Tuned2 - Tuned1". This allows viewing negative numbers (show up simply as - with no magnitude shown) in one view as positive in the other.
* To change the scale, use the s key (to increase) or S key (to decrease) the scale. This is reflected in the legend. Changing the scale allows viewing small values near 1 or 0 or to view the whole SWR range of 1 to 10.
* Pressing 1 (for tune1) or 2 (for tune2) will query for the row and column to view in LC space. Enter the two numbers and a new screen is shown.

This shows the SWR map for all LC combinations. Because 256x256x2 is too big to view on the screen, it is shown zoomed in by a factor of 4 with the lowest SWR value shown. Only the top half is shown without scrolling. The scrolling keys are shown to the right. The shifted scrolling keys move in half-screen size steps. The c key shows the other capacitor swith SWR map. Reference the x and y values at the top of the screen to see where you have scrolled to, it shows the coordinates of the upper left corner. Everywhere the algorithm 'looked' in the map is highlighted. The final tuned value is marked with a # sign. Press q to return to the resistance/reactance screen.






