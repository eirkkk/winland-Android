//! ext-workspace protocol implementation
//!
//! Allows clients to list and control workspaces (virtual desktops).
//!
//! ## Usage
//!
//! ```no_run
//! use smithay::wayland::ext_workspace::{ExtWorkspaceManagerState, ExtWorkspaceHandler};
//!
//! pub struct State {
//!     workspace_state: ExtWorkspaceManagerState,
//! }
//!
//! smithay::delegate_dispatch2!(State);
//!
//! impl ExtWorkspaceHandler for State {
//!     fn ext_workspace_state(&mut self) -> &mut ExtWorkspaceManagerState {
//!         &mut self.workspace_state
//!     }
//! }
//!
//! # let mut display = wayland_server::Display::<State>::new().unwrap();
//! # let display_handle = display.handle();
//! let state = ExtWorkspaceManagerState::new::<State>(&display_handle);
//! ```

use std::sync::{Arc, Mutex};

use wayland_protocols::ext::workspace::v1::server::{
    ext_workspace_group_handle_v1::{
        self, ExtWorkspaceGroupHandleV1, GroupCapabilities,
    },
    ext_workspace_handle_v1::{
        self, ExtWorkspaceHandleV1, State, WorkspaceCapabilities,
    },
    ext_workspace_manager_v1::{
        self, ExtWorkspaceManagerV1,
    },
};
use wayland_server::{
    Client, DataInit, Dispatch, DisplayHandle, GlobalDispatch, New, Resource,
    backend::{ClientId, GlobalId},
};

use crate::{
    utils::user_data::UserDataMap,
    wayland::{Dispatch2, GlobalData, GlobalDispatch2},
};

// ── Handler trait ───────────────────────────────────────────────────────────

/// Handler for the ext-workspace protocol
pub trait ExtWorkspaceHandler: 'static {
    fn ext_workspace_state(&mut self) -> &mut ExtWorkspaceManagerState;
}

// ── Workspace group handle ──────────────────────────────────────────────────

#[derive(Debug)]
struct WorkspaceGroupInner {
    instances: Vec<ExtWorkspaceGroupHandleV1>,
    removed: bool,
}

/// Strong handle to a workspace group
#[derive(Debug, Clone)]
pub struct ExtWorkspaceGroupHandle {
    inner: Arc<(Mutex<WorkspaceGroupInner>, UserDataMap)>,
}

impl ExtWorkspaceGroupHandle {
    fn new(instances: Vec<ExtWorkspaceGroupHandleV1>) -> Self {
        Self {
            inner: Arc::new((
                Mutex::new(WorkspaceGroupInner {
                    instances,
                    removed: false,
                }),
                UserDataMap::new(),
            )),
        }
    }

    pub fn send_capabilities(&self, capabilities: GroupCapabilities) {
        let inner = self.inner.0.lock().unwrap();
        for group in &inner.instances {
            group.capabilities(capabilities);
        }
    }

    pub fn send_output_enter(&self, output: &wayland_server::protocol::wl_output::WlOutput) {
        let inner = self.inner.0.lock().unwrap();
        for group in &inner.instances {
            group.output_enter(output);
        }
    }

    pub fn send_output_leave(&self, output: &wayland_server::protocol::wl_output::WlOutput) {
        let inner = self.inner.0.lock().unwrap();
        for group in &inner.instances {
            group.output_leave(output);
        }
    }

    pub fn send_workspace_enter(&self, workspace: &ExtWorkspaceHandleV1) {
        let inner = self.inner.0.lock().unwrap();
        for group in &inner.instances {
            group.workspace_enter(workspace);
        }
    }

    pub fn send_workspace_leave(&self, workspace: &ExtWorkspaceHandleV1) {
        let inner = self.inner.0.lock().unwrap();
        for group in &inner.instances {
            group.workspace_leave(workspace);
        }
    }

    pub fn send_removed(&self) {
        let mut inner = self.inner.0.lock().unwrap();
        if inner.removed {
            return;
        }
        inner.removed = true;
        for group in inner.instances.drain(..) {
            group.removed();
        }
    }

    pub fn user_data(&self) -> &UserDataMap {
        &self.inner.1
    }

    fn init_new_instance(&self, group: ExtWorkspaceGroupHandleV1) {
        debug_assert!(!self.is_removed(), "no handles for removed groups");
        self.inner.0.lock().unwrap().instances.push(group);
    }

    fn remove_instance(&self, instance: &ExtWorkspaceGroupHandleV1) {
        let mut inner = self.inner.0.lock().unwrap();
        inner.instances.retain(|i| i != instance);
    }

    fn is_removed(&self) -> bool {
        self.inner.0.lock().unwrap().removed
    }
}

// ── Workspace handle ────────────────────────────────────────────────────────

pub use ext_workspace_handle_v1::State as WsState;
pub use ext_workspace_handle_v1::WorkspaceCapabilities as WsCapabilities;
pub use ext_workspace_group_handle_v1::GroupCapabilities as WsGroupCapabilities;

#[derive(Debug)]
struct WorkspaceInner {
    name: String,
    state: State,
    capabilities: WorkspaceCapabilities,
    coordinates: Vec<u8>,
    instances: Vec<ExtWorkspaceHandleV1>,
    removed: bool,
}

/// Strong handle to a workspace
#[derive(Debug, Clone)]
pub struct ExtWorkspaceHandle {
    inner: Arc<(Mutex<WorkspaceInner>, UserDataMap)>,
}

impl ExtWorkspaceHandle {
    fn new(
        name: String,
        state: State,
        capabilities: WorkspaceCapabilities,
        coordinates: Vec<u8>,
        instances: Vec<ExtWorkspaceHandleV1>,
    ) -> Self {
        Self {
            inner: Arc::new((
                Mutex::new(WorkspaceInner {
                    name,
                    state,
                    capabilities,
                    coordinates,
                    instances,
                    removed: false,
                }),
                UserDataMap::new(),
            )),
        }
    }

    pub fn send_id(&self, id: &str) {
        let inner = self.inner.0.lock().unwrap();
        for ws in &inner.instances {
            ws.id(id.to_string());
        }
    }

    pub fn send_name(&self, name: &str) {
        let mut inner = self.inner.0.lock().unwrap();
        if inner.name == name {
            return;
        }
        inner.name = name.to_string();
        for ws in &inner.instances {
            ws.name(name.to_string());
        }
    }

    pub fn send_state(&self, state: State) {
        let mut inner = self.inner.0.lock().unwrap();
        if inner.state == state {
            return;
        }
        inner.state = state;
        for ws in &inner.instances {
            ws.state(state);
        }
    }

    pub fn send_coordinates(&self, coordinates: Vec<u8>) {
        let mut inner = self.inner.0.lock().unwrap();
        if inner.coordinates == coordinates {
            return;
        }
        inner.coordinates = coordinates.clone();
        for ws in &inner.instances {
            ws.coordinates(coordinates.clone());
        }
    }

    pub fn send_capabilities(&self, capabilities: WorkspaceCapabilities) {
        let mut inner = self.inner.0.lock().unwrap();
        inner.capabilities = capabilities;
        for ws in &inner.instances {
            ws.capabilities(capabilities);
        }
    }

    pub fn send_removed(&self) {
        let mut inner = self.inner.0.lock().unwrap();
        if inner.removed {
            return;
        }
        inner.removed = true;
        for ws in inner.instances.drain(..) {
            ws.removed();
        }
    }

    pub fn user_data(&self) -> &UserDataMap {
        &self.inner.1
    }

    fn init_new_instance(&self, w: ExtWorkspaceHandleV1) {
        debug_assert!(!self.is_removed(), "no handles for removed workspaces");
        let mut inner = self.inner.0.lock().unwrap();
        w.name(inner.name.clone());
        w.state(inner.state);
        if !inner.coordinates.is_empty() {
            w.coordinates(inner.coordinates.clone());
        }
        w.capabilities(inner.capabilities);
        inner.instances.push(w);
    }

    fn remove_instance(&self, instance: &ExtWorkspaceHandleV1) {
        let mut inner = self.inner.0.lock().unwrap();
        inner.instances.retain(|i| i != instance);
    }

    fn is_removed(&self) -> bool {
        self.inner.0.lock().unwrap().removed
    }
}

// ── Manager state ───────────────────────────────────────────────────────────

/// State of the [ExtWorkspaceManagerV1] global
#[derive(Debug)]
pub struct ExtWorkspaceManagerState {
    global: GlobalId,
    manager_instances: Vec<ExtWorkspaceManagerV1>,
    group_handles: Vec<ExtWorkspaceGroupHandle>,
    workspace_handles: Vec<ExtWorkspaceHandle>,
    dh: DisplayHandle,
}

impl ExtWorkspaceManagerState {
    /// Register new [ExtWorkspaceManagerV1] global
    pub fn new<D>(dh: &DisplayHandle) -> Self
    where
        D: ExtWorkspaceHandler
            + GlobalDispatch<ExtWorkspaceManagerV1, GlobalData>
            + Dispatch<ExtWorkspaceGroupHandleV1, ExtWorkspaceGroupHandle>
            + Dispatch<ExtWorkspaceHandleV1, ExtWorkspaceHandle>,
    {
        let global = dh.create_global::<D, ExtWorkspaceManagerV1, _>(1, GlobalData);

        Self {
            global,
            manager_instances: Vec::new(),
            group_handles: Vec::new(),
            workspace_handles: Vec::new(),
            dh: dh.clone(),
        }
    }

    pub fn global(&self) -> GlobalId {
        self.global.clone()
    }

    /// Create a new workspace group
    pub fn new_workspace_group<D>(&mut self) -> ExtWorkspaceGroupHandle
    where
        D: ExtWorkspaceHandler
            + Dispatch<ExtWorkspaceGroupHandleV1, ExtWorkspaceGroupHandle>
            + Dispatch<ExtWorkspaceHandleV1, ExtWorkspaceHandle>,
    {
        let group = ExtWorkspaceGroupHandle::new(
            Vec::with_capacity(self.manager_instances.len()),
        );

        for instance in &self.manager_instances {
            let Ok(client) = self.dh.get_client(instance.id()) else {
                continue;
            };
            let Ok(g) = client.create_resource::<ExtWorkspaceGroupHandleV1, _, D>(
                &self.dh,
                instance.version(),
                group.clone(),
            ) else {
                continue;
            };
            instance.workspace_group(&g);
            group.init_new_instance(g);
        }

        self.group_handles.push(group.clone());
        group
    }

    /// Create a new workspace
    pub fn new_workspace<D>(
        &mut self,
        name: impl Into<String>,
        state: State,
        capabilities: WorkspaceCapabilities,
        coordinates: Vec<u8>,
    ) -> ExtWorkspaceHandle
    where
        D: ExtWorkspaceHandler
            + Dispatch<ExtWorkspaceGroupHandleV1, ExtWorkspaceGroupHandle>
            + Dispatch<ExtWorkspaceHandleV1, ExtWorkspaceHandle>,
    {
        let ws = ExtWorkspaceHandle::new(
            name.into(),
            state,
            capabilities,
            coordinates,
            Vec::with_capacity(self.manager_instances.len()),
        );

        for instance in &self.manager_instances {
            let Ok(client) = self.dh.get_client(instance.id()) else {
                continue;
            };
            let Ok(w) = client.create_resource::<ExtWorkspaceHandleV1, _, D>(
                &self.dh,
                instance.version(),
                ws.clone(),
            ) else {
                continue;
            };
            instance.workspace(&w);
            ws.init_new_instance(w);
        }

        self.workspace_handles.push(ws.clone());
        ws
    }

    /// Send done event on all manager instances
    pub fn send_done(&self) {
        for mgr in &self.manager_instances {
            mgr.done();
        }
    }

    /// Send finished event (compositor stops advertising workspaces)
    pub fn send_finished(&self) {
        for mgr in &self.manager_instances {
            mgr.finished();
        }
    }

    pub fn remove_workspace(&mut self, handle: &ExtWorkspaceHandle) {
        handle.send_removed();
        self.workspace_handles.retain(|h| !Arc::ptr_eq(&h.inner, &handle.inner));
    }

    pub fn remove_group(&mut self, handle: &ExtWorkspaceGroupHandle) {
        handle.send_removed();
        self.group_handles.retain(|h| !Arc::ptr_eq(&h.inner, &handle.inner));
    }
}

// ── Global dispatch ─────────────────────────────────────────────────────────

impl<D: ExtWorkspaceHandler> GlobalDispatch2<ExtWorkspaceManagerV1, D> for GlobalData
where
    D: Dispatch<ExtWorkspaceManagerV1, GlobalData>
        + Dispatch<ExtWorkspaceGroupHandleV1, ExtWorkspaceGroupHandle>
        + Dispatch<ExtWorkspaceHandleV1, ExtWorkspaceHandle>,
{
    fn bind(
        &self,
        state: &mut D,
        dh: &DisplayHandle,
        client: &Client,
        resource: New<ExtWorkspaceManagerV1>,
        data_init: &mut DataInit<'_, D>,
    ) {
        let instance = data_init.init(resource, GlobalData);
        let state = state.ext_workspace_state();

        for group in &state.group_handles {
            if group.is_removed() {
                continue;
            }
            if let Ok(g) = client.create_resource::<ExtWorkspaceGroupHandleV1, _, D>(
                dh,
                instance.version(),
                group.clone(),
            ) {
                instance.workspace_group(&g);
                group.init_new_instance(g);
            }
        }

        for ws in &state.workspace_handles {
            if ws.is_removed() {
                continue;
            }
            if let Ok(w) = client.create_resource::<ExtWorkspaceHandleV1, _, D>(
                dh,
                instance.version(),
                ws.clone(),
            ) {
                instance.workspace(&w);
                ws.init_new_instance(w);
            }
        }

        instance.done();
        state.manager_instances.push(instance);
    }
}

// ── Manager dispatch ────────────────────────────────────────────────────────

impl<D: ExtWorkspaceHandler> Dispatch2<ExtWorkspaceManagerV1, D> for GlobalData {
    fn request(
        &self,
        state: &mut D,
        _client: &Client,
        _resource: &ExtWorkspaceManagerV1,
        request: ext_workspace_manager_v1::Request,
        _dh: &DisplayHandle,
        _data_init: &mut DataInit<'_, D>,
    ) {
        match request {
            ext_workspace_manager_v1::Request::Commit => {
                state.ext_workspace_state().send_done();
            }
            ext_workspace_manager_v1::Request::Stop => {
                state.ext_workspace_state().send_finished();
            }
            _ => {}
        }
    }

    fn destroyed(&self, state: &mut D, _client: ClientId, resource: &ExtWorkspaceManagerV1) {
        state
            .ext_workspace_state()
            .manager_instances
            .retain(|i| i != resource);
    }
}

// ── Group handle dispatch ───────────────────────────────────────────────────

impl<D: ExtWorkspaceHandler> Dispatch2<ExtWorkspaceGroupHandleV1, D> for ExtWorkspaceGroupHandle {
    fn request(
        &self,
        _state: &mut D,
        _client: &Client,
        _resource: &ExtWorkspaceGroupHandleV1,
        request: ext_workspace_group_handle_v1::Request,
        _dh: &DisplayHandle,
        _data_init: &mut DataInit<'_, D>,
    ) {
        match request {
            ext_workspace_group_handle_v1::Request::CreateWorkspace { workspace: _name } => {}
            ext_workspace_group_handle_v1::Request::Destroy => {}
            _ => {}
        }
    }

    fn destroyed(
        &self,
        _state: &mut D,
        _client: ClientId,
        resource: &ExtWorkspaceGroupHandleV1,
    ) {
        self.remove_instance(resource);
    }
}

// ── Workspace handle dispatch ───────────────────────────────────────────────

impl<D: ExtWorkspaceHandler> Dispatch2<ExtWorkspaceHandleV1, D> for ExtWorkspaceHandle {
    fn request(
        &self,
        _state: &mut D,
        _client: &Client,
        _resource: &ExtWorkspaceHandleV1,
        request: ext_workspace_handle_v1::Request,
        _dh: &DisplayHandle,
        _data_init: &mut DataInit<'_, D>,
    ) {
        match request {
            ext_workspace_handle_v1::Request::Activate => {}
            ext_workspace_handle_v1::Request::Deactivate => {}
            ext_workspace_handle_v1::Request::Remove => {}
            ext_workspace_handle_v1::Request::Assign { workspace_group: _ } => {}
            ext_workspace_handle_v1::Request::Destroy => {}
            _ => {}
        }
    }

    fn destroyed(&self, _state: &mut D, _client: ClientId, resource: &ExtWorkspaceHandleV1) {
        self.remove_instance(resource);
    }
}
