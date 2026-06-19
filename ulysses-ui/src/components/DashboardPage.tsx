import { useState, useEffect, useCallback } from "react";
import { api, type Session, type Block, type DaemonStatus } from "../lib/api";
import { Shield, ShieldAlert, Play, Square, Clock, Lock, Zap } from "lucide-react";

export function DashboardPage() {
  const [sessions, setSessions] = useState<Session[]>([]);
  const [blocks, setBlocks] = useState<Block[]>([]);
  const [status, setStatus] = useState<DaemonStatus | null>(null);
  const [stopping, setStopping] = useState<string | null>(null);
  const [showStart, setShowStart] = useState(false);

  const refresh = useCallback(async () => {
    try {
      const [s, b, st] = await Promise.all([
        api.getActiveSessions(),
        api.getBlocks(),
        api.getStatus(),
      ]);
      setSessions(s);
      setBlocks(b);
      setStatus(st);
    } catch { /* daemon not running */ }
  }, []);

  useEffect(() => {
    refresh();
    const id = setInterval(refresh, 3000);
    return () => clearInterval(id);
  }, [refresh]);

  const handleStart = async (blockId: string, lockType: string, lockParam: string) => {
    try {
      await api.startSession(blockId, lockType, lockParam);
      setShowStart(false);
      refresh();
    } catch (e) {
      alert("Failed to start session: " + e);
    }
  };

  const handleStop = async (sessionId: string) => {
    const pwd = prompt("Enter password to unlock session:");
    if (pwd === null) return;
    setStopping(sessionId);
    try {
      const ok = await api.endSession(sessionId, pwd);
      if (!ok) alert("Incorrect password or session cannot be ended yet.");
      refresh();
    } catch (e) {
      alert("Error: " + e);
    } finally {
      setStopping(null);
    }
  };

  const blockName = (id: string) => blocks.find((b) => b.id === id)?.name || "Unknown";
  const isBlocking = sessions.length > 0;

  return (
    <div className="space-y-8">
      {/* Status Banner */}
      <div className={`relative overflow-hidden rounded-2xl border p-6 ${
        isBlocking
          ? "border-danger/30 bg-gradient-to-br from-danger-soft to-surface-raised"
          : "border-success/30 bg-gradient-to-br from-success-soft to-surface-raised"
      }`}>
        <div className="flex items-center gap-4">
          <div className={`p-3 rounded-xl ${isBlocking ? "bg-danger-soft" : "bg-success-soft"}`}>
            {isBlocking ? <ShieldAlert size={28} className="text-danger" /> : <Shield size={28} className="text-success" />}
          </div>
          <div>
            <h2 className="text-xl font-bold">{isBlocking ? "Blocking Active" : "All Clear"}</h2>
            <p className="text-text-secondary text-sm mt-0.5">
              {isBlocking
                ? `${sessions.length} active session${sessions.length > 1 ? "s" : ""} enforcing rules`
                : "No active blocks — distractions allowed"}
            </p>
          </div>
          <div className="ml-auto">
            {status && (
              <div className="flex items-center gap-2 text-xs text-text-muted">
                <span className="inline-block w-2 h-2 rounded-full bg-success animate-pulse" />
                Daemon v{status.version}
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Quick Actions */}
      <div>
        <h3 className="text-sm font-medium text-text-muted uppercase tracking-wider mb-4">Quick Start</h3>
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
          {blocks.map((b) => (
            <button
              key={b.id}
              onClick={() => handleStart(b.id, b.lock_type, b.lock_param)}
              className="group text-left p-5 rounded-xl border border-border bg-surface-raised hover:bg-surface-hover hover:border-accent/40 transition-all duration-200"
            >
              <div className="flex items-start justify-between mb-3">
                <div className="p-2 rounded-lg bg-accent-soft group-hover:bg-accent/20 transition-colors">
                  <Play size={18} className="text-accent" />
                </div>
                <div className="flex items-center gap-1.5 text-xs text-text-muted">
                  {b.lock_type === "timer" ? <Clock size={12} /> : <Lock size={12} />}
                  <span>{b.lock_type === "timer" ? `${Math.round(parseInt(b.lock_param) / 60)}min` : "Password"}</span>
                </div>
              </div>
              <h4 className="font-semibold text-text-primary group-hover:text-accent transition-colors">{b.name}</h4>
              <p className="text-xs text-text-muted mt-1">{b.list_ids.length} list{b.list_ids.length !== 1 ? "s" : ""} assigned</p>
            </button>
          ))}
          {blocks.length === 0 && (
            <div className="col-span-full text-center py-12 text-text-muted">
              <Zap size={32} className="mx-auto mb-3 opacity-40" />
              <p>No blocks configured yet</p>
              <p className="text-xs mt-1">Create blocks in the Blocks tab to get started</p>
            </div>
          )}
        </div>
      </div>

      {/* Active Sessions */}
      {sessions.length > 0 && (
        <div>
          <h3 className="text-sm font-medium text-text-muted uppercase tracking-wider mb-4">Active Sessions</h3>
          <div className="space-y-3">
            {sessions.map((s) => (
              <div
                key={s.id}
                className="flex items-center gap-4 p-5 rounded-xl border border-danger/20 bg-surface-raised"
              >
                <div className="p-2 rounded-lg bg-danger-soft">
                  <ShieldAlert size={20} className="text-danger" />
                </div>
                <div className="flex-1 min-w-0">
                  <div className="font-semibold">{blockName(s.block_id)}</div>
                  <div className="text-xs text-text-muted mt-0.5 flex items-center gap-3">
                    <span className="flex items-center gap-1">
                      {s.lock_type === "timer" ? <Clock size={11} /> : <Lock size={11} />}
                      {s.lock_type}
                    </span>
                    {s.expires_at && (
                      <span>Expires: {new Date(parseInt(s.expires_at) * 1000).toLocaleTimeString()}</span>
                    )}
                  </div>
                </div>
                <button
                  onClick={() => handleStop(s.id)}
                  disabled={stopping === s.id}
                  className="flex items-center gap-2 px-4 py-2 rounded-lg bg-danger-soft text-danger hover:bg-danger/20 transition-colors text-sm font-medium disabled:opacity-50"
                >
                  <Square size={14} />
                  {stopping === s.id ? "Stopping…" : "Stop"}
                </button>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}
