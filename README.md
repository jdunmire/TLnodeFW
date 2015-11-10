TLnodeFW - Firmware for the RockingDLabs TLnode
=====================================================
An application to run on the TLnode hardware and report sensor
values to an MQTT server.


Prerequisits for execution
--------------------------
In order for the system_get_vdd33() function to work,
the byte 107 in the init section of flash must be set to 0xff.
The esp_init_data_vccRead.bin is a copy of the esp_init_data_default.bin
file from v1.3.0 of the ESP SDK with byte 107 changed from 0x00 to 0xff.
Write the file to flash with a command like this:

    $ esptool.py --port /dev/ttyUSB0 \
      write_flash 0x7c000 esp_init_data_vccRead.bin

The above command has been added to the Makefile.


Prerequisits for build
----------------------
  * https://github.com/pfalcon/esp-open-sdk
  * ESP SDK v1.3.0 (automatically installed by `esp-open-sdk`
  * A copy of [esp_mqtt](https://github.com/tuanpmt/esp_mqtt) in the
    `modules/esp_mqtt/` directory. (Working code was built with
    commit #7247e3f of that project.)


Build and Install
----------------------
  * Copy `paths.tmpl` to `paths.inc`.
  * Adjust the paths in `paths.inc` for your setup.
  * Copy `include/mqtt_config.tmpl` to `include/mqtt_config.h`.
  * Adjust the settings in `include/mqtt_config.h` to match your network.
  * Build:

        $ make

  * Install (put your target system in bootloader mode first):

        $ make flash


Key files
------------------------
  * LICENSE.txt - permissions for copying this project.

  * .gitignore - This is a list of files that should not be committed.

  * paths.tmpl - Environment variables that point the build tools.

      __DO NOT MODIFY paths.tmpl__ Copy it to `paths.inc` and make your
      changes there. `paths.inc` is listed in `.gitignore` so that it
      will not get committed. This both protects you from inadvertentely
      sharing information about your environment when you share your git
      archive and prevents _dualing commits_.

  * driver/ - auxillary code
      - the uart driver is included here.

  * include/ - application header files

  * include/mqtt_config.tmpl - Configuration of the MQTT network

      __DO NOT MODIFY include/mqtt_config.tmpl__ Copy it to
      `include/mqtt_config.h` and make your
      changes there. `include/mqtt_config.h` is listed in
      `include/.gitignore` so that it will not get committed. This both
      protects you from inadvertentely sharing information about your
      network when you share your git archive and prevents _dualing
      commits_.

  * include/drivers - header files for drivers
      - header files for the uart driver are here

  * licenses/ - licenses for code that is used by, but not part of the
      project. GPL-v3 is included here to cover the uart driver.

  * user/ - The top level code

License
-------
Copyright 2015 Jerry Dunmire
This file is part of TLnodeFW

TLnodeFW is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

TLnodeFW is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with TLnodeFW.  If not, see <http://www.gnu.org/licenses/>.

Files that are not part of TLnodeFW are clearly identified at the top
of each of the files. These files are distributed under terms noted in each
of the files.

