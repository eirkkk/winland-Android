use std::collections::{HashMap, HashSet};

#[derive(Debug, Clone, Copy)]
pub struct TouchPoint {
	pub x: f32,
	pub y: f32,
	pub x_norm: f32,
	pub y_norm: f32,
}

#[derive(Debug, Clone)]
pub enum RoutedInputEvent {
	TouchDown {
		id: i32,
		point: TouchPoint,
	},
	TouchMove {
		id: i32,
		point: TouchPoint,
	},
	TouchUp {
		id: i32,
	},
	TouchCancel {
		id: i32,
	},
	KeyDown {
		keycode: i32,
	},
	KeyUp {
		keycode: i32,
	},
	TextCommit {
		text: String,
	},
	GestureScroll {
		dx: f32,
		dy: f32,
	},
}

#[derive(Debug, Default)]
pub struct InputRouter {
	active_touches: HashMap<i32, TouchPoint>,
	pressed_keys: HashSet<i32>,
	last_multi_touch_centroid: Option<(f32, f32)>,
}

impl InputRouter {
	pub fn route_touch(
		&mut self,
		action: i32,
		id: i32,
		x: f32,
		y: f32,
		width: i32,
		height: i32,
	) -> Vec<RoutedInputEvent> {
		let point = normalize_point(x, y, width, height);
		let mut events = Vec::with_capacity(2);

		match action {
			0 | 5 => {
				self.active_touches.insert(id, point);
				events.push(RoutedInputEvent::TouchDown { id, point });
			}
			2 => {
				self.active_touches.insert(id, point);
				events.push(RoutedInputEvent::TouchMove { id, point });
			}
			1 | 6 => {
				self.active_touches.remove(&id);
				events.push(RoutedInputEvent::TouchUp { id });
			}
			3 => {
				self.active_touches.remove(&id);
				events.push(RoutedInputEvent::TouchCancel { id });
			}
			_ => {
				self.active_touches.insert(id, point);
				events.push(RoutedInputEvent::TouchMove { id, point });
			}
		}

		if action == 2 {
			if self.active_touches.len() >= 2 {
				if let Some((cx, cy)) = self.centroid() {
					if let Some((lx, ly)) = self.last_multi_touch_centroid {
						let dx = cx - lx;
						let dy = cy - ly;
						if dx.abs() > 0.001 || dy.abs() > 0.001 {
							events.push(RoutedInputEvent::GestureScroll { dx, dy });
						}
					}
					self.last_multi_touch_centroid = Some((cx, cy));
				}
			} else {
				self.last_multi_touch_centroid = None;
			}
		}

		if (action == 1 || action == 3 || action == 6) && self.active_touches.len() < 2 {
			self.last_multi_touch_centroid = None;
		}

		events
	}

	fn centroid(&self) -> Option<(f32, f32)> {
		if self.active_touches.is_empty() {
			return None;
		}

		let mut sx = 0.0_f32;
		let mut sy = 0.0_f32;
		for p in self.active_touches.values() {
			sx += p.x_norm;
			sy += p.y_norm;
		}

		let c = self.active_touches.len() as f32;
		Some((sx / c, sy / c))
	}

	pub fn route_key(&mut self, keycode: i32, is_down: bool) -> RoutedInputEvent {
		if is_down {
			self.pressed_keys.insert(keycode);
			RoutedInputEvent::KeyDown { keycode }
		} else {
			self.pressed_keys.remove(&keycode);
			RoutedInputEvent::KeyUp { keycode }
		}
	}

	pub fn active_touch_count(&self) -> usize {
		self.active_touches.len()
	}

	pub fn pressed_key_count(&self) -> usize {
		self.pressed_keys.len()
	}
}

fn normalize_point(x: f32, y: f32, width: i32, height: i32) -> TouchPoint {
	let width_f = if width <= 0 { 1.0 } else { width as f32 };
	let height_f = if height <= 0 { 1.0 } else { height as f32 };

	TouchPoint {
		x,
		y,
		x_norm: (x / width_f).clamp(0.0, 1.0),
		y_norm: (y / height_f).clamp(0.0, 1.0),
	}
}
