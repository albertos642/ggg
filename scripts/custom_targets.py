"""
@file custom_targets.py
@brief Custom PlatformIO targets for interactive menuconfig execution.

@author Alberto Soncini <alberto@synergon-lab.xyz>
@copyright Copyright (c) 2012-2026 Alberto Soncini.
@license GPL-3.0-or-later

This file is part of GGG.

GGG is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GGG is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GGG. If not, see <https://www.gnu.org/licenses/>.
"""

# GGG PlatformIO Custom Targets
try:
    Import("env")
    if "__PIO_TARGETS" not in env or "menuconfig" not in env["__PIO_TARGETS"]:
        env.AddCustomTarget(
            name="menuconfig",
            dependencies=None,
            actions=["menuconfig"],
            title="Run Kconfig Menu",
            description="Launch the interactive GGG configuration menu"
        )
except Exception:
    pass
