use std::collections::HashMap;

#[derive(Default)]
pub struct ScrollState {
    pub points: HashMap<i32, (f32, f32)>,
}

impl ScrollState {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn process_touch(&mut self, id: i32, x: f32, y: f32, action: i32) {
        match action {
            0 | 5 => { // DOWN or POINTER_DOWN
                self.points.insert(id, (x, y));
            }
            1 | 6 => { // UP or POINTER_UP
                self.points.remove(&id);
            }
            2 => { // MOVE
                if let Some(p) = self.points.get_mut(&id) {
                    *p = (x, y);
                }
            }
            _ => {}
        }
    }

    pub fn get_centroid(&self) -> Option<(f32, f32)> {
        if self.points.is_empty() { return None; }
        let mut sum_x = 0.0;
        let mut sum_y = 0.0;
        for (x, y) in self.points.values() {
            sum_x += x;
            sum_y += y;
        }
        let count = self.points.len() as f32;
        Some((sum_x / count, sum_y / count))
    }
}

