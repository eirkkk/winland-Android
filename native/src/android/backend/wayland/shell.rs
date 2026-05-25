use smithay::{
    desktop::{
        Window, WindowSurfaceType,
        space::{RenderZindex, SpaceElement},
    },
    output::Output,
    utils::{IsAlive, Logical, Point, Rectangle},
};

#[derive(Debug, Clone)]
pub(crate) struct WindowElement(pub Window);

impl PartialEq for WindowElement {
    fn eq(&self, other: &Self) -> bool {
        self.0 == other.0
    }
}

impl Eq for WindowElement {}

impl IsAlive for WindowElement {
    fn alive(&self) -> bool {
        self.0.alive()
    }
}

impl SpaceElement for WindowElement {
    fn geometry(&self) -> Rectangle<i32, Logical> {
        self.0.geometry()
    }

    fn bbox(&self) -> Rectangle<i32, Logical> {
        self.0.bbox_with_popups()
    }

    fn is_in_input_region(&self, point: &Point<f64, Logical>) -> bool {
        self.0.surface_under(*point, WindowSurfaceType::ALL).is_some()
    }

    fn z_index(&self) -> u8 {
        RenderZindex::Shell as u8
    }

    fn set_activate(&self, activated: bool) {
        self.0.set_activated(activated);
    }

    fn output_enter(&self, output: &Output, overlap: Rectangle<i32, Logical>) {
        log::debug!(
            "WindowElement: output_enter output={} overlap={:?}",
            output.name(),
            overlap,
        );
    }

    fn output_leave(&self, output: &Output) {
        log::debug!(
            "WindowElement: output_leave output={}",
            output.name(),
        );
    }
}
