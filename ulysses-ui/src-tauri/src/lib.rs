use zbus::{Connection, proxy};

#[proxy(
    interface = "net.ulysses.Daemon",
    default_service = "net.ulysses.Daemon",
    default_path = "/net/ulysses/Daemon"
)]
trait Daemon {
    // Lists
    fn get_lists(&self) -> zbus::Result<String>;
    fn create_list(&self, json: &str) -> zbus::Result<String>;
    fn update_list(&self, json: &str) -> zbus::Result<bool>;
    fn delete_list(&self, id: &str) -> zbus::Result<bool>;

    // Blocks
    fn get_blocks(&self) -> zbus::Result<String>;
    fn create_block(&self, json: &str) -> zbus::Result<String>;
    fn update_block(&self, json: &str) -> zbus::Result<bool>;
    fn delete_block(&self, id: &str) -> zbus::Result<bool>;

    // Sessions
    fn start_session(&self, block_id: &str, lock_type: &str, lock_param: &str) -> zbus::Result<String>;
    fn end_session(&self, session_id: &str, auth_token: &str) -> zbus::Result<bool>;
    fn get_active_sessions(&self) -> zbus::Result<String>;
    fn get_status(&self) -> zbus::Result<String>;

    // Triggers
    fn create_trigger(&self, json: &str) -> zbus::Result<String>;
    fn delete_trigger(&self, uuid: &str) -> zbus::Result<bool>;
    fn fire_trigger(&self, uuid: &str) -> zbus::Result<String>;
    fn get_triggers(&self) -> zbus::Result<String>;

    // Network
    fn create_network_group(&self) -> zbus::Result<String>;
    fn join_network_group(&self, group_uuid: &str, secret_hex: &str, salt_hex: &str) -> zbus::Result<bool>;
    fn leave_network_group(&self) -> zbus::Result<()>;
    fn get_network_status(&self) -> zbus::Result<String>;
    fn get_peers(&self) -> zbus::Result<String>;
}

// Helper to get a proxy
async fn get_proxy() -> Result<DaemonProxy<'static>, String> {
    let conn = Connection::session().await.map_err(|e| format!("D-Bus connection failed: {}", e))?;
    DaemonProxy::new(&conn).await.map_err(|e| format!("Proxy creation failed: {}", e))
}

// ─── Lists ───────────────────────────────────────────────────────────────────

#[tauri::command]
async fn get_lists() -> Result<String, String> {
    get_proxy().await?.get_lists().await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn create_list(json: String) -> Result<String, String> {
    get_proxy().await?.create_list(&json).await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn update_list(json: String) -> Result<bool, String> {
    get_proxy().await?.update_list(&json).await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn delete_list(id: String) -> Result<bool, String> {
    get_proxy().await?.delete_list(&id).await.map_err(|e| e.to_string())
}

// ─── Blocks ──────────────────────────────────────────────────────────────────

#[tauri::command]
async fn get_blocks() -> Result<String, String> {
    get_proxy().await?.get_blocks().await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn create_block(json: String) -> Result<String, String> {
    get_proxy().await?.create_block(&json).await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn update_block(json: String) -> Result<bool, String> {
    get_proxy().await?.update_block(&json).await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn delete_block(id: String) -> Result<bool, String> {
    get_proxy().await?.delete_block(&id).await.map_err(|e| e.to_string())
}

// ─── Sessions ────────────────────────────────────────────────────────────────

#[tauri::command]
async fn get_active_sessions() -> Result<String, String> {
    get_proxy().await?.get_active_sessions().await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn start_session(block_id: String, lock_type: String, lock_param: String) -> Result<String, String> {
    get_proxy().await?.start_session(&block_id, &lock_type, &lock_param).await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn end_session(session_id: String, auth_token: String) -> Result<bool, String> {
    get_proxy().await?.end_session(&session_id, &auth_token).await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn get_status() -> Result<String, String> {
    get_proxy().await?.get_status().await.map_err(|e| e.to_string())
}

// ─── Triggers ────────────────────────────────────────────────────────────────

#[tauri::command]
async fn get_triggers() -> Result<String, String> {
    get_proxy().await?.get_triggers().await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn create_trigger(json: String) -> Result<String, String> {
    get_proxy().await?.create_trigger(&json).await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn delete_trigger(uuid: String) -> Result<bool, String> {
    get_proxy().await?.delete_trigger(&uuid).await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn fire_trigger(uuid: String) -> Result<String, String> {
    get_proxy().await?.fire_trigger(&uuid).await.map_err(|e| e.to_string())
}

// ─── Network ─────────────────────────────────────────────────────────────────

#[tauri::command]
async fn get_network_status() -> Result<String, String> {
    get_proxy().await?.get_network_status().await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn create_network_group() -> Result<String, String> {
    get_proxy().await?.create_network_group().await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn join_network_group(group_uuid: String, secret_hex: String, salt_hex: String) -> Result<bool, String> {
    get_proxy().await?.join_network_group(&group_uuid, &secret_hex, &salt_hex).await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn leave_network_group() -> Result<(), String> {
    get_proxy().await?.leave_network_group().await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn get_peers() -> Result<String, String> {
    get_proxy().await?.get_peers().await.map_err(|e| e.to_string())
}

// ─── App Entry ───────────────────────────────────────────────────────────────

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![
            // Lists
            get_lists, create_list, update_list, delete_list,
            // Blocks
            get_blocks, create_block, update_block, delete_block,
            // Sessions
            get_active_sessions, start_session, end_session, get_status,
            // Triggers
            get_triggers, create_trigger, delete_trigger, fire_trigger,
            // Network
            get_network_status, create_network_group, join_network_group,
            leave_network_group, get_peers
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
