# ume
VTE based terminal emulator forked from sakura 

A couple of key differences are:
  - Colors are changeable from the config file
  - More rebindable keys
  - Config colors are compatible with termite



### Prerequisites
ume requires the following libraries:
```
vte >= 2.91
vte-devel >= 0.50
glib >= 2.40
gtk >= 3.20
x11-devel
```
Also requires a C++17 compliant compiler.
  
### Installing

  First ensure you have all the prequisites installed.
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
  
  
  For further instructions view [INSTALL](INSTALL) to see how to install ume.
 
### User Guide
###### Configuration Settings
| Key | Default | Description |
| --- | --- | --- |
|`last_colorset`|`1`| The last color set used by ume |
|`scroll_lines`|`4096`| How many lines of scrollback to store |
|`scroll_amount`|`10`| Amount to scroll up when you scroll up |
|`font`|`Ubuntu Mono,monospace 12`| Default font |
|`show_always_first_tab`|`No`| Always show the first tab |
|`scrollbar`|`0`| Should the terminal show the scroll bar |
|`closebutton`|`true`| Should ume show the close button |
|`tabs_on_bottom`|`0`| Show tabs on the bottom of the screen |
|`less_questions`|`0`| Show less pop ups |
|`disable_numbered_tabswitch`|`0`| Allows you to switch to tabs by pressing numbers |
|`use_fading`|`0`| Fade between colorset swaps |
|`scrollable_tabs`|`true`| Use the scrollwheel to scroll over tabs |
|`urgent_bell`|`Yes`| Enable urgent bell when something pops up in the terminal |
|`audible_bell`|`Yes`| Should the bell make a sound |
|`blinking_cursor`|`No`| Should the cursor blink |
|`stop_tab_cycling_at_end_tabs`|`No`| Stop at the end when tabbing through tabs |
|`allow_bold`|`Yes`| Allow displaying bolded characters |
|`cursor_type`|`block`| Cursor type when inputting numbers in the gtk client |
|`word_chars`|`-,./?%&#_~:`| Characters that define breaks between words |
|`add_tab_accelerator`|`5`| Accelerator for creating tabs |
|`del_tab_accelerator`|`5`| Accelerator for deleting tabs |
|`switch_tab_accelerator`|`4`| Accelerator for switching tabs |
|`move_tab_accelerator`|`5`| Accelerator for moving tabs |
|`copy_accelerator`|`5`| Accelerator for copying |
|`scrollbar_accelerator`|`5`| Accelerator for toggling the scrollbar |
|`open_url_accelerator`|`5`| Accelerator for opening a url |
|`font_size_accelerator`|`5`| Accelerator for adjusting font size |
|`set_tab_name_accelerator`|`5`| Accelerator for setting the tab name |
|`search_accelerator`|`5`| Accelerator for opening the search menu |
|`add_tab_key`|`T`| Key to create a new tab |
|`del_tab_key`|`W`| Key to close a tab |
|`prev_tab_key`|`Left`| Key to switch to the previous tab |
|`next_tab_key`|`Right`| Key to switch to the next tab |
|`copy_key`|`C`| Key to copy selection |
|`paste_key`|`V`| Key to paste |
|`scrollbar_key`|`S`| Key to toggle the scroll bar |
|`scroll_up_key`|`K`| Key to scroll up |
|`scroll_down_key`|`J`| Key to scroll down |
|`page_up_key`|`U`| Key to page down |
|`page_down_key`|`D`| Key to page up |
|`set_tab_name_key`|`N`| Key to set the current tab name |
|`search_key`|`F`| Key to open search menu |
|`increase_font_size_key`|`plus`| Key to increase font size |
|`decrease_font_size_key`|`minus`| Key to decrease font size |
|`fullscreen_key`|`F11`| Key to make the terminal fullscreen |
|`colors1_key`|`F1`| Key to switch to the 1st colorset |
|`colors2_key`|`F2`| Key to switch to the 2nd colorset |
|`colors3_key`|`F3`| Key to switch to the 3rd colorset |
|`colors4_key`|`F4`| Key to switch to the 4th colorset |
|`colors5_key`|`F5`| Key to switch to the 5th colorset |
|`colors6_key`|`F6`| Key to switch to the 6th colorset |
|`set_colorset_accelerator`|`5`| Accelerator for changing to a colorset |
|`icon_file`|`terminal-tango.svg`| Path to icon file |
|`reload_config_on_modify`|`false`| If the config file gets modified while running, reload |
|`ignore_overwrite`|`false`| Ignore the overwrite prompt when closing ume |

(UNDER CONSTRUCTION)

### TODO
  - [ ] Clean up code base
  - [ ] Remove wall of warnings when building
  - [x] Add shell colors to color menu
  - [x]　Programmatically manipulate ume while it is running
  - [ ] Better way to configure modifier keys 
  - [ ] Change booleans in config file to true false values
  - [ ] Change yes/no in config file to true false values

## License

This project is licensed under the GPL2.0 License - see the [LICENSE](LICENSE) file for details
