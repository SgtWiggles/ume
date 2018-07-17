# ume
VTE based terminal emulator forked from sakura 

A couple of key differences are:
  - Colors are changeable from the config file
  - More rebindable keys
  - Config colors are compatible with termite settings from [terminal.sexy](https://terminal.sexy/)

### Prerequisites
ume requires the following libraries:
```
vte >= 2.91
vte-devel >= 0.50
glib >= 2.40
gtk >= 3.20
x11-devel
```
Also requires a C++17 compliant compiler. So far ume has been compiled with gcc 7.3.0 on Void Linux x64. 
  
### Installing

  First ensure you have all the prerequisites installed.
  Then clone the repository using:
  ```
  git clone https://github.com/SgtWiggles/ume.git
  ```
  Then to build and install the project use:
  ```
  cd ume
  cmake .
  sudo make install
  ```
  To install ume at a different path, CMake must be given the proper environment variables.
  For example to install ume at `/usr` you would call
  ```
  cmake -DCMAKE_INSTALL_PREFIX=/usr .
  ```
  
  Use `CMAKE_BUILD_TYPE=Debug` if you need debug symbols. Default build is "Release".
  
### User Guide
###### Modifiers
  Modifiers are the modifier keys, such as control and shift, which are applied to key bindings.
  As of right now, they are represented as integers in the config file.
  The following values can be added together for different modifiers:  
  Shift(1), Cps-Lock(2), Ctrl(4), Alt(8)  
  For example, to get Ctrl+Shift, the modifier would be 5
  
Modifiers can be set to any mask value from the GdkModifierType in gdktypes.h. For reference look at: https://github.com/GNOME/gtk/blob/master/gdk/gdktypes.h

###### Configuration Settings
Key bindings can be unbound by erasing the value and leaving it blank.

| Config Key | Default | Description |
| --- | --- | --- |
|`last_colorset`|`1`| The last color set used by ume |
|`scroll_lines`|`4096`| How many lines of scrollback to store |
|`scroll_amount`|`10`| Amount to scroll up when you scroll up |
|`font`|`Ubuntu Mono,monospace 12`| Default font |
|`show_always_first_tab`|`No`| Should ume always show the first tab |
|`scrollbar`|`0`| Should the terminal show the scroll bar |
|`closebutton`|`true`| Should ume show the close button |
|`tabs_on_bottom`|`false`| Show tabs on the bottom of the screen |
|`less_questions`|`false`| Show less pop ups |
|`disable_numbered_tabswitch`|`false`| Allows you to switch to tabs by pressing numbers |
|`use_fading`|`false`| Fade text out when terminal is not focused |
|`scrollable_tabs`|`true`| Use the scrollwheel to scroll over tabs |
|`urgent_bell`|`Yes`| Enable urgent bell when something pops up in the terminal |
|`audible_bell`|`Yes`| Should the bell make a sound |
|`blinking_cursor`|`No`| Should the cursor blink |
|`stop_tab_cycling_at_end_tabs`|`No`| Stop at the end when tabbing through tabs |
|`allow_bold`|`Yes`| Allow displaying bolded characters |
|`cursor_type`|`block`| Cursor type when inputting numbers in the gtk client |
|`word_chars`|`-,./?%&#_~:`| Characters that define breaks between words |
|`add_tab_modifier`|`5`| Modifier for creating tabs |
|`del_tab_modifier`|`5`| Modifier for deleting tabs |
|`switch_tab_modifier`|`4`| Modifier for switching tabs |
|`move_tab_modifier`|`5`| Modifier for moving tabs |
|`copy_modifier`|`5`| Modifier for copying |
|`scrollbar_modifier`|`5`| Modifier for toggling the scrollbar |
|`open_url_modifier`|`5`| Modifier for opening a url |
|`font_size_modifier`|`5`| Modifier for adjusting font size |
|`set_tab_name_modifier`|`5`| Modifier for setting the tab name |
|`search_modifier`|`5`| Modifier for opening the search menu |
|`add_tab_key`|`T`| Key to create a new tab, uses `add_tab_modifier` |
|`del_tab_key`|`W`| Key to close a tab, uses `del_tab_modifier` |
|`prev_tab_key`|`Left`| Key to switch to the previous tab, uses `switch_tab_modifier` |
|`next_tab_key`|`Right`| Key to switch to the next tab, uses `switch_tab_modifier` |
|`copy_key`|`C`| Key to copy selection, uses `copy_modifier` |
|`paste_key`|`V`| Key to paste, uses `copy_modifier` |
|`scrollbar_key`|`S`| Key to toggle the scroll bar, uses `scrollbar_modifier` |
|`scroll_up_key`|`K`| Key to scroll up, uses `scrollbar_modifier` |
|`scroll_down_key`|`J`| Key to scroll down, uses `scrollbar_modifier` |
|`page_up_key`|`U`| Key to page down, uses `scrollbar_modifier` |
|`page_down_key`|`D`| Key to page up, uses `scrollbar_modifier` |
|`set_tab_name_key`|`N`| Key to set the current tab name, uses `set_tab_name_modifier` |
|`search_key`|`F`| Key to open search menu, uses `search_modifier` |
|`increase_font_size_key`|`plus`| Key to increase font size, uses `font_size_modifier` |
|`decrease_font_size_key`|`minus`| Key to decrease font size, uses `font_size_modifier` |
|`fullscreen_key`|`F11`| Key to make the terminal fullscreen, doesn't have an modifier |
|`colors1_key`|`F1`| Key to switch to the 1st colorset, uses `set_colorset_modifier` |
|`colors2_key`|`F2`| Key to switch to the 2nd colorset, uses `set_colorset_modifier` |
|`colors3_key`|`F3`| Key to switch to the 3rd colorset, uses `set_colorset_modifier` |
|`colors4_key`|`F4`| Key to switch to the 4th colorset, uses `set_colorset_modifier` |
|`colors5_key`|`F5`| Key to switch to the 5th colorset, uses `set_colorset_modifier` |
|`colors6_key`|`F6`| Key to switch to the 6th colorset, uses `set_colorset_modifier` |
|`set_colorset_modifier`|`5`| Modifier for changing to a colorset |
|`icon_file`|`terminal-tango.svg`| Path to icon file |
|`ignore_overwrite`|`false`| Ignore the overwrite prompt when closing ume. Does not overwrite the existing config file |
|`reload_modifier`|`5`| Modifier to for the reload keybind |
|`reload_key`|`R`| Key to reload config file | 

###### Signals
When ume receives the signal USR1 it reloads the config file. Thus one can reload all the config for all instances of ume using `killall -USR1 ume`.

###### Colors
Groups colors1 to colors6 correspond to the different color sets. 
Colors can be set in the following forms:
```
rgba(255,255,255,1)
rgb(255,255,255)
#FFFFFF
```
Colors  0 to 7 are the normal text colors, while 8 to 15 are bright colors.

| Config Key | Description |
| --- | --- | 
|`foreground`| Foreground color |
|`background`| Background color for the terminal |
|`cursor`| Cursor color, when the field is left blank, cursor inverts foreground and background color |
|`color0`| Text color 0 |
|`color1`| Text color 1 |
|`color2`| Text color 2 |
|`color3`| Text color 3 |
|`color4`| Text color 4 |
|`color5`| Text color 5 |
|`color6`| Text color 6 |
|`color7`| Text color 7 |
|`color8`| Bright text color 8 |
|`color9`| Bright text color 9 |
|`color10`| Bright text color 10 |
|`color11`| Bright text color 11 |
|`color12`| Bright text color 12 |
|`color13`| Bright text color 13 |
|`color14`| Bright text color 14 |
|`color15`| Bright text color 15 |


### Contact
  For submitting bugs or requesting new features, please use the [issue tracker](https://github.com/SgtWiggles/ume/issues). Before submitting make sure you are using the latest version of ume. If you are submitting a bug report be sure to include your operating system, a minimal way to reproduce the bug and anything else that may be relevent to the bug.

### TODO
- [ ] Clean up code base  
- [ ] Better way to configure modifier keys (replace modifier numbers with strings)  
- [ ] Change yes/no in config file to true false values  
- [ ] Update [INSTALL](INSTALL)  
- [x] Reload settings keybind
- [x] Replace automated reloading with signals
- [x] Change booleans in config file to true false values  
- [x] Remove wall of warnings when building 
- [x] Add shell colors to color menu  
- [x] Programmatically manipulate ume while it is running  
- [ ] ~~Fix the automated reloading system to prevent overwrites.~~

## License

This project is licensed under the GPL2.0 License - see the [LICENSE](LICENSE) file for details
