# Mouse hand assignment (0.6.2)

Open **F2 → Keyboard → Swap mouse hands → On → Apply** to equip items with **left mouse click → left hand** and **right mouse click → right hand**. Off retains the original LMB right / RMB left behavior. Manual configuration uses `[Controls] SwapMouseHands=1`, followed by restarting the game.

The setting changes ordinary item wheel clicks, including weapons and spells where a hand can be selected. Skyrim's constraints still apply: two-handed gear occupies both hands; shields and torches remain left-hand items; armor and consumables retain their existing neutral actions. It does not remap gameplay attack buttons.

Combat/outfit presets continue using LMB to apply and RMB to manage. Settings, naming and confirmation dialogs keep their regular mouse clicks. Controller hand buttons and keyboard Enter retain their existing behavior. Footer hints reflect the selected assignment, and all eight bundled languages include the new label, help and hint.

The option defaults off when missing; startup does not rewrite an existing INI. Apply serializes the new key with the existing direct-byte writer. Cancel restores the previous preference, and Defaults restores off.

## Verification

WheelLogicTests cover both ordinary mouse hand mappings and unchanged preset apply/manage semantics. SettingsTests save the enabled option and verify it in a new process, exercise old INIs, cancel/default behavior and the six-row keyboard layout. All language files are checked against the English catalog.

Native Draw/WARP preview: `build/hands-skyrim-controls-swapped-1920.jpg` (1280×720 compressed preview). The sixth setting row and explanatory text fit without overlapping the controls, binding hints or Apply button. A separate ordinary wheel preview checks the reversed footer text. The user confirmed successful game testing of mouse hand switching on October 13; included in 0.7.0. The detailed runtime/case matrix was not supplied.
