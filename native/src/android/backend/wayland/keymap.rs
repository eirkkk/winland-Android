use xkbcommon::xkb;

/// Reverse-lookup a Unicode character in the active xkb keymap to find the
/// evdev scancode and shift state that produces it. Returns `(scancode, with_shift)`
/// or `None` if the character cannot be typed on this keymap.
///
/// This avoids hardcoded character→keycode tables and works with any keyboard
/// layout (Arabic, Cyrillic, etc.) loaded into the xkb keymap.
pub fn unicode_to_keycode(
    keymap: &xkb::Keymap,
    ch: char,
) -> Option<(u32, bool)> {
    let keysym = xkb::utf32_to_keysym(ch as u32);
    if keysym == xkb::Keysym::NoSymbol {
        return None;
    }

    let min = keymap.min_keycode();
    let max = keymap.max_keycode();

    for raw in min.raw()..=max.raw() {
        let keycode = xkb::Keycode::new(raw);

        // Check level 0 (unshifted)
        let syms0 = keymap.key_get_syms_by_level(keycode, 0, 0);
        if syms0.contains(&keysym) {
            return Some((raw, false));
        }

        // Check level 1 (shifted)
        let syms1 = keymap.key_get_syms_by_level(keycode, 0, 1);
        if syms1.contains(&keysym) {
            return Some((raw, true));
        }
    }

    None
}

// Maps Android KeyEvent keycodes to Linux evdev key codes, then converts to xkb keycode (+8).
pub fn android_keycode_to_xkb_scancode(android_keycode: i32) -> u32 {
	let evdev = match android_keycode {
		// Digits 0-9
		7 => 11,
		8 => 2,
		9 => 3,
		10 => 4,
		11 => 5,
		12 => 6,
		13 => 7,
		14 => 8,
		15 => 9,
		16 => 10,

		// Letters A-Z
		29 => 30,
		30 => 48,
		31 => 46,
		32 => 32,
		33 => 18,
		34 => 33,
		35 => 34,
		36 => 35,
		37 => 23,
		38 => 36,
		39 => 37,
		40 => 38,
		41 => 50,
		42 => 49,
		43 => 24,
		44 => 25,
		45 => 16,
		46 => 19,
		47 => 31,
		48 => 20,
		49 => 22,
		50 => 47,
		51 => 17,
		52 => 45,
		53 => 21,
		54 => 44,

		// Common controls
		19 => 103, // DPAD_UP
		20 => 108, // DPAD_DOWN
		21 => 105, // DPAD_LEFT
		22 => 106, // DPAD_RIGHT
		92 => 104, // PAGE_UP
		93 => 109, // PAGE_DOWN
		122 => 110, // MOVE_HOME
		123 => 115, // MOVE_END
		112 => 111, // FORWARD_DEL
		124 => 210, // INSERT
		61 => 15,  // TAB
		62 => 57,  // SPACE
		66 => 28,  // ENTER
		67 => 14,  // DEL/BACKSPACE
		111 => 1,  // ESCAPE
		120 => 119, // SYSRQ/PRINTSCREEN
		116 => 70,  // SCROLL_LOCK
		121 => 119, // BREAK (best-effort mapping)
		82 => 127,  // MENU
		131 => 59,  // F1
		132 => 60,  // F2
		133 => 61,  // F3
		134 => 62,  // F4
		135 => 63,  // F5
		136 => 64,  // F6
		137 => 65,  // F7
		138 => 66,  // F8
		139 => 67,  // F9
		140 => 68,  // F10
		141 => 87,  // F11
		142 => 88,  // F12

		// Punctuation
		55 => 52, // COMMA
		56 => 53, // PERIOD
		68 => 12, // GRAVE
		69 => 13, // MINUS
		70 => 26, // EQUALS
		71 => 27, // LEFT_BRACKET
		72 => 43, // RIGHT_BRACKET
		73 => 39, // BACKSLASH
		74 => 40, // SEMICOLON
		75 => 51, // APOSTROPHE
		76 => 41, // SLASH

		// Modifiers
		57 => 56,  // ALT_LEFT
		58 => 100, // ALT_RIGHT
		59 => 42,  // SHIFT_LEFT
		60 => 54,  // SHIFT_RIGHT
		113 => 29, // CTRL_LEFT
		114 => 97, // CTRL_RIGHT
		117 => 125, // META_LEFT
		118 => 126, // META_RIGHT

		// Numpad
		144 => 82,  // NUMPAD_0
		145 => 79,  // NUMPAD_1
		146 => 80,  // NUMPAD_2
		147 => 81,  // NUMPAD_3
		148 => 75,  // NUMPAD_4
		149 => 76,  // NUMPAD_5
		150 => 77,  // NUMPAD_6
		151 => 71,  // NUMPAD_7
		152 => 72,  // NUMPAD_8
		153 => 73,  // NUMPAD_9
		154 => 55,  // NUMPAD_DIVIDE
		155 => 98,  // NUMPAD_MULTIPLY
		156 => 74,  // NUMPAD_SUBTRACT
		157 => 78,  // NUMPAD_ADD
		158 => 96,  // NUMPAD_DOT
		160 => 28,  // NUMPAD_ENTER
		143 => 69,  // NUM_LOCK

		// Fallback: if keycode looks already like evdev, pass through.
		code if code > 0 && code < 256 => code as u32,
		_ => 0,
	};

	if evdev == 0 {
		0
	} else {
		evdev + 8
	}
}
