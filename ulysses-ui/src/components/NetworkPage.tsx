import { useState, useEffect, useCallback } from "react";
import { api, type NetworkStatus } from "../lib/api";
import { Globe, Users, Plus, LogOut, Copy, Check } from "lucide-react";
import { QRCodeSVG } from "qrcode.react";

export function NetworkPage() {
  const [status, setStatus] = useState<NetworkStatus | null>(null);
  const [joinUuid, setJoinUuid] = useState("");
  const [joinSecret, setJoinSecret] = useState("");
  const [joinSalt, setJoinSalt] = useState("");
  const [copied, setCopied] = useState(false);
  const [groupInfo, setGroupInfo] = useState<{ group_uuid: string; group_secret: string; salt: string; device_id: string } | null>(null);

  const refresh = useCallback(async () => {
    try { setStatus(await api.getNetworkStatus()); } catch {}
  }, []);
  useEffect(() => { refresh(); const id = setInterval(refresh, 5000); return () => clearInterval(id); }, [refresh]);

  const handleCreate = async () => {
    try {
      const info = await api.createNetworkGroup();
      setGroupInfo(info);
      refresh();
    } catch (e) { alert("Error: " + e); }
  };

  const handleJoin = async () => {
    if (!joinUuid || !joinSecret || !joinSalt) return;
    try {
      await api.joinNetworkGroup(joinUuid, joinSecret, joinSalt);
      setJoinUuid(""); setJoinSecret(""); setJoinSalt("");
      refresh();
    } catch (e) { alert("Error: " + e); }
  };

  const handleLeave = async () => {
    if (!confirm("Leave network group?")) return;
    try { await api.leaveNetworkGroup(); setGroupInfo(null); refresh(); } catch (e) { alert("Error: " + e); }
  };

  const copyGroupInfo = () => {
    if (!groupInfo) return;
    navigator.clipboard.writeText(JSON.stringify(groupInfo, null, 2));
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <div className="space-y-6">
      <div>
        <h2 className="text-xl font-bold">Network</h2>
        <p className="text-sm text-text-secondary mt-0.5">Cross-device sync — fire triggers and unlock sessions across all paired devices</p>
      </div>

      {/* Status */}
      <div className={`p-6 rounded-xl border ${status?.has_group ? "border-accent/30 bg-accent-soft" : "border-border bg-surface-raised"}`}>
        <div className="flex items-center gap-4">
          <div className={`p-3 rounded-xl ${status?.has_group ? "bg-accent/20" : "bg-surface-alt"}`}>
            <Globe size={24} className={status?.has_group ? "text-accent" : "text-text-muted"} />
          </div>
          <div className="flex-1">
            <h3 className="font-semibold">{status?.has_group ? "Connected to Group" : "No Group"}</h3>
            {status?.has_group && (
              <p className="text-xs text-text-muted mt-0.5 font-mono">{status.group_uuid}</p>
            )}
          </div>
          {status?.has_group && (
            <button onClick={handleLeave} className="flex items-center gap-2 px-4 py-2 rounded-lg bg-danger-soft text-danger hover:bg-danger/20 text-sm font-medium transition-colors">
              <LogOut size={14} /> Leave
            </button>
          )}
        </div>
      </div>

      {/* Created group info */}
      {groupInfo && (
        <div className="p-5 rounded-xl border border-accent/30 bg-surface-raised space-y-4">
          <div className="flex items-center justify-between">
            <h3 className="font-semibold text-sm">Group Created — Share These Credentials</h3>
            <button onClick={copyGroupInfo} className="flex items-center gap-1.5 text-xs font-medium text-accent hover:text-accent-hover">
              {copied ? <Check size={14} /> : <Copy size={14} />} {copied ? "Copied" : "Copy"}
            </button>
          </div>
          <div className="flex flex-col md:flex-row gap-6 items-center">
            <div className="bg-white p-2 rounded-lg shrink-0">
              <QRCodeSVG 
                value={JSON.stringify({
                  group_uuid: groupInfo.group_uuid,
                  group_secret: groupInfo.group_secret,
                  salt: groupInfo.salt
                })} 
                size={160} 
              />
            </div>
            <pre className="flex-1 w-full text-xs text-text-secondary bg-surface-alt p-4 rounded-lg overflow-x-auto font-mono">
              {JSON.stringify(groupInfo, null, 2)}
            </pre>
          </div>
        </div>
      )}

      {!status?.has_group && (
        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
          <button onClick={handleCreate} className="p-6 rounded-xl border border-border bg-surface-raised hover:border-accent/40 hover:bg-surface-hover transition-all text-left">
            <div className="p-2.5 rounded-lg bg-accent-soft inline-block mb-3"><Plus size={20} className="text-accent" /></div>
            <h3 className="font-semibold">Create New Group</h3>
            <p className="text-xs text-text-muted mt-1">Generate credentials and share with other devices</p>
          </button>
          <div className="p-6 rounded-xl border border-border bg-surface-raised space-y-3">
            <h3 className="font-semibold">Join Existing Group</h3>
            <input value={joinUuid} onChange={e => setJoinUuid(e.target.value)} placeholder="Group UUID" className="w-full px-3 py-2 rounded-lg bg-surface-alt border border-border text-sm text-text-primary placeholder:text-text-muted focus:outline-none focus:border-accent" />
            <input value={joinSecret} onChange={e => setJoinSecret(e.target.value)} placeholder="Secret (hex)" className="w-full px-3 py-2 rounded-lg bg-surface-alt border border-border text-sm text-text-primary placeholder:text-text-muted focus:outline-none focus:border-accent" />
            <input value={joinSalt} onChange={e => setJoinSalt(e.target.value)} placeholder="Salt (hex)" className="w-full px-3 py-2 rounded-lg bg-surface-alt border border-border text-sm text-text-primary placeholder:text-text-muted focus:outline-none focus:border-accent" />
            <button onClick={handleJoin} className="w-full px-4 py-2.5 rounded-xl bg-accent hover:bg-accent-hover text-white text-sm font-medium transition-colors">Join Group</button>
          </div>
        </div>
      )}

      {/* Peers */}
      {status?.has_group && (
        <div>
          <h3 className="text-sm font-medium text-text-muted uppercase tracking-wider mb-3">
            <Users size={14} className="inline mr-1.5" />Peers ({status.peers.length})
          </h3>
          <div className="space-y-2">
            {status.peers.map((p, i) => (
              <div key={i} className="flex items-center gap-3 p-4 rounded-xl bg-surface-raised border border-border-subtle">
                <span className="w-2 h-2 rounded-full bg-success" />
                <span className="text-sm font-mono text-text-secondary truncate">{p.device_id}</span>
                <span className="text-xs text-text-muted ml-auto">{p.last_seen}</span>
              </div>
            ))}
            {status.peers.length === 0 && <p className="text-sm text-text-muted text-center py-8">No peers connected</p>}
          </div>
        </div>
      )}
    </div>
  );
}
