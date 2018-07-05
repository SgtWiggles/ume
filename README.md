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
Also requires a C++11 compliant compiler.
  
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
  | `last_color_set` | `1` | The last color set used by ume |
(UNDER CONSTRUCTION)

### TODO
  - [ ] Clean up code base
  - [ ] Remove wall of warnings when building
  - [ ] Add shell colors to color menu
  - [ ] Programmatically manipulate ume while it is running
  - [ ] Better way to configure modifier keys 

## License

This project is licensed under the GPL2.0 License - see the [LICENSE](LICENSE) file for details
