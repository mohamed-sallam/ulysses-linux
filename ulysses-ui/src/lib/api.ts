import { invoke } from "@tauri-apps/api/core";

// ─── Types ───────────────────────────────────────────────────────────────────

export interface ListEntry {
  type: "allow" | "deny";
  value: string;
  rule_type: "domain" | "url" | "wildcard" | "app_name" | "window_title";
}

export interface UList {
  id: string;
  name: string;
  entries: ListEntry[];
}

export interface Block {
  id: string;
  name: string;
  lock_type: "timer" | "password";
  lock_param: string;
  list_ids: string[];
}

export interface Session {
  id: string;
  block_id: string;
  trigger_uuid: string;
  lock_type: string;
  started_at: string;
  expires_at: string;
}

export interface Trigger {
  uuid: string;
  name: string;
  block_id: string;
  propagate_to_network: boolean;
  lamport_ts: number;
}

export interface NetworkStatus {
  has_group: boolean;
  group_uuid?: string;
  device_id?: string;
  peers: { device_id: string; last_seen: string }[];
}

export interface DaemonStatus {
  version: string;
  running: boolean;
  active_sessions: Session[];
  has_group: boolean;
  group_uuid?: string;
  peers: number;
}

// ─── API Functions ───────────────────────────────────────────────────────────

async function call<T>(cmd: string, args?: Record<string, unknown>): Promise<T> {
  return invoke<T>(cmd, args);
}

async function callJson<T>(cmd: string, args?: Record<string, unknown>): Promise<T> {
  const raw = await invoke<string>(cmd, args);
  return JSON.parse(raw) as T;
}

// Lists
export const api = {
  getLists: () => callJson<UList[]>("get_lists"),
  createList: (data: { name: string; entries: ListEntry[] }) =>
    call<string>("create_list", { json: JSON.stringify(data) }),
  updateList: (data: { id: string; name: string; entries: ListEntry[] }) =>
    call<boolean>("update_list", { json: JSON.stringify(data) }),
  deleteList: (id: string) => call<boolean>("delete_list", { id }),

  // Blocks
  getBlocks: () => callJson<Block[]>("get_blocks"),
  createBlock: (data: { name: string; lock_type: string; lock_param: string; list_ids: string[] }) =>
    call<string>("create_block", { json: JSON.stringify(data) }),
  updateBlock: (data: { id: string; name: string; lock_type: string; lock_param: string; list_ids: string[] }) =>
    call<boolean>("update_block", { json: JSON.stringify(data) }),
  deleteBlock: (id: string) => call<boolean>("delete_block", { id }),

  // Sessions
  getActiveSessions: () => callJson<Session[]>("get_active_sessions"),
  startSession: (blockId: string, lockType: string, lockParam: string) =>
    call<string>("start_session", { blockId, lockType, lockParam }),
  endSession: (sessionId: string, authToken: string) =>
    call<boolean>("end_session", { sessionId, authToken }),
  getStatus: () => callJson<DaemonStatus>("get_status"),

  // Triggers
  getTriggers: () => callJson<Trigger[]>("get_triggers"),
  createTrigger: (data: { name: string; block_id: string; propagate_to_network: boolean }) =>
    call<string>("create_trigger", { json: JSON.stringify(data) }),
  deleteTrigger: (uuid: string) => call<boolean>("delete_trigger", { uuid }),
  fireTrigger: (uuid: string) => call<string>("fire_trigger", { uuid }),

  // Network
  getNetworkStatus: () => callJson<NetworkStatus>("get_network_status"),
  createNetworkGroup: () => callJson<{ group_uuid: string; group_secret: string; salt: string; device_id: string }>("create_network_group"),
  joinNetworkGroup: (groupUuid: string, secretHex: string, saltHex: string) =>
    call<boolean>("join_network_group", { groupUuid, secretHex, saltHex }),
  leaveNetworkGroup: () => call<void>("leave_network_group"),
  getPeers: () => callJson<{ device_id: string; last_seen: string }[]>("get_peers"),
};
