# Mini Green House

Copyright (c) 2022, Erik C. Nykwest
All rights reserved.

This source code is licensed under the GPL-style license found in the
LICENSE file in the root directory of this source tree.

A Code to monitor my minigreen house for seedlings.
This code was specifically written for, and tested on, an ESP32 Dev Module
It measures both the Temperature and Humidity and sends reports to both Blynk and Smartnest.
Smartnest is connected to the Smart Life App via IFTTT to turn on and off
my wireless heating pad via a smart outlet.
The ESP32 also controls a servo motor to open or close the green house windows
based on the current temperature.