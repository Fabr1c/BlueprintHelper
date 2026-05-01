# Logic Reading Guide

Use `blueprint_get_logic` first when the user asks what a Blueprint does. It returns LogicMD, which is low-token and intended for review.

Use `blueprint_get_logic_json` when planning an edit, comparing structured call flow, or locating nodes. Enable node IDs only when needed for mutation planning. Enable positions only when debugging layout.

Use `blueprint_export_to_json` only when raw replay, exact pin/link debugging, or import compatibility is required.

LogicMD and LogicJson are read formats. They are not raw import formats.
