Klatchian Mist |Build Status|
=============================

ESP32 Zigbee dehumidifier controller.

Supports Midea dehumidifiers.

Requires an ESP32-H2 or ESP32-C6.

    The shaking finger stopped suddenly. The man spun around.
    "How did you manage to walk through the wall?"
    "I'm sorry?" said Susan, backing away. "I didn't know there was one."
    "What d'you call this, then, Klatchian Mist?" The man slapped the air.

    -- `Terry Pratchett <https://en.wikipedia.org/wiki/Terry_Pratchett>`_
    (`Soul Music, 1994 <https://en.wikipedia.org/wiki/Soul_Music_(novel)>`_)


Usage
-----

Use the WiFi button on the dehumidifier (or use the UART command) to join/leave
the Zigbee network. Joining a new network is not performed automatically.
Leaving the network currently requires a restart.

The following Zigbee clusters are provided:

* Power Switch
* Mode (Analog Output)
* Fan Speed (Analog Output)
* Humidity Reading (Analog Input)
* Humidity Setpoint (Analog Output)
* Ioniser (Binary Output)
* Auto Defrost (Binary Input)
* Bucket Full (Binary Input)

Status
~~~~~~

The following Zigbee analog input clusters report the status of the device:

* Uptime (days)
* Connected time (days)
* Uplink address
* Uplink RSSI (dB)

When a core dump is present the **Uptime (days)** cluster will report a fault
in the status flag with a reliability attribute value of "unreliable other".

LED Events
~~~~~~~~~~

.. list-table::
   :widths: 50 50
   :header-rows: 1

   * - Colours
     - Description
   * - White
     - Disconnected (network not configured)
   * - White (blinking)
     - Connecting (network not configured)
   * - Yellow
     - Disconnected (network configured)
   * - Yellow (blinking)
     - Connecting (network configured)
   * - Green (constant then blinking every 3 seconds)
     - Network connected
   * - Red (blinking 2 times for 1 second)
     - Network error
   * - Red
     - Network failed (network configured)
   * - Red (blinking)
     - Network failed (network not configured)
   * - Orange (for 2 seconds)
     - Status updated
   * - Blue (for 2 seconds)
     - Remote control activity
   * - Magenta
     - Identify request received
   * - Cyan
     - OTA update in progress
   * - Red (blinking 8 times in 3 seconds)
     - OTA update error
   * - Rainbow (cycling)
     - Core dump present

UART Commands
~~~~~~~~~~~~~

.. list-table::
   :widths: 15 85
   :header-rows: 1

   * - Keys
     - Description
   * - ``0``
     - Disable logging (persistent)
   * - ``1``\ ..\ ``5``
     - Set application log level to ERROR..VERBOSE (persistent)
   * - ``6``\ ..\ ``9``
     - Set system log level to ERROR..DEBUG (persistent)
   * - ``b``
     - Print cluster binding table
   * - ``j``
     - Join Zigbee network (no effect if already joined/joining)
   * - ``l``
     - Leave Zigbee network (no effect if already left)
   * - ``m``
     - Print memory information
   * - ``n``
     - Print Zigbee neighbours
   * - ``t``
     - Print task list and stats
   * - ``R``
     - Restart
   * - ``A``
     - ZBOSS assert (used for testing to generate a core dump)
   * - ``C``
     - Crash (used for testing to generate a core dump)
   * - ``d``
     - Print brief core dump summary
   * - ``D``
     - Print whole core dump
   * - ``E``
     - Erase saved core dump

Build
-----

This project can be built with the `ESP-IDF build system
<https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html>`_.

Configure::

    idf.py set-target esp32c6
    idf.py menuconfig

Under "Component config" you'll find "Klatchian Mist" where you can configure
the number of lights supported and whether switches/relays are active low or not.

The GPIO configuration assumes you're using an `ESP32-C6-DevKitC-1
<https://docs.espressif.com/projects/espressif-esp-dev-kits/en/latest/esp32c6/esp32-c6-devkitc-1/>`_.

Build::

    idf.py build

Flash::

    idf.py flash


Help
----

What order are all the entities shown in Home Assistant?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Zigbee specifications are thousands of pages long and it supports 240
endpoints per device but there's no attribute to describe on/off clusters if
you have more than one of the same type!

Using `this version of homeassistant-entity-renamer
<https://github.com/nomis/homeassistant-entity-renamer>`_ that can update
the friendly names (so that they're not all "Analoginput", "Binaryinput" and
"Switch") and the `hass-rename-entities.sh script <hass-rename-entities.sh>`_
you can rename all of the entities automatically.

The control cluster endpoints are in the following order:

.. list-table::
   :widths: 20 10 70
   :header-rows: 1

   * - Type
     - Endpoint
     - Name
   * - Analogoutput
     - 20
     - Mode
   * - Analogoutput
     - 21
     - Fan Speed
   * - Analogoutput
     - 31
     - Humidity setpoint
   * - Analogoutput
     - 32
     - Temperature
   * - Switch
     - 10
     - Power
   * - Switch
     - 40
     - Ioniser

The sensor cluster endpoints are in the following order:

.. list-table::
   :widths: 20 10 70
   :header-rows: 1

   * - Type
     - Endpoint
     - Name
   * - Analoginput
     - 1
     - Uptime (days)
   * - Analoginput
     - 30
     - Humidity Reading
   * - Analoginput
     - 210
     - Connected time (days)
   * - Analoginput
     - 211
     - Uplink address
   * - Analoginput
     - 212
     - Uplink RSSI (dB)
   * - Binaryinput
     - 11
     - Auto Defrost
   * - Binaryinput
     - 12
     - Bucket Full

References
----------

Additional detail on the serial protocol can be found in these projects:

* `Hypfer/esp8266-midea-dehumidifier <https://github.com/Hypfer/esp8266-midea-dehumidifier>`_
* `Chreece/ESPHome-Dehumidifier <https://github.com/Chreece/ESPHome-Dehumidifier>`_

.. |Build Status| image:: https://jenkins.uuid.uk/buildStatus/icon?job=klatchian-mist%2Fmain
