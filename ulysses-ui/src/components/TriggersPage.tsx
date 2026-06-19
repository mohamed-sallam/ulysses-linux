import { useState, useEffect, useCallback } from "react";
import { api, type Trigger, type Block } from "../lib/api";
import { Dialog } from "./Dialog";
import { Zap, Plus, Trash2, Play, Wifi, WifiOff } from "lucide-react";

export function TriggersPage() {
  const [triggers, setTriggers] = useState<Trigger[]>([]);
  const [blocks, setBlocks] = useState<Block[]>([]);
  const [showCreate, setShowCreate] = useState(false);
  const [newName, setNewName] = useState("");
  const [newBlockId, setNewBlockId] = useState("");
  const [newPropagate, setNewPropagate] = useState(false);

  const refresh = useCallback(async () => {
    try {
      const [t, b] = await Promise.all([api.getTriggers(), api.getBlocks()]);
      setTriggers(t); setBlocks(b);
    } catch {}
  }, []);
  useEffect(() => { refresh(); }, [refresh]);

  const blockName = (id: string) => blocks.find(b => b.id === id)?.name || "Unknown";

  const handleCreate = async () => {
    if (!newName.trim() || !newBlockId) return;
    try {
      await api.createTrigger({ name: newName, block_id: newBlockId, propagate_to_network: newPropagate });
      setShowCreate(false); setNewName(""); setNewBlockId(""); setNewPropagate(false);
      refresh();
    } catch (e) { alert("Error: " + e); }
  };

  const handleFire = async (uuid: string) => {
    try {
      const sid = await api.fireTrigger(uuid);
      if (sid) alert("Session started: " + sid);
      else alert("Failed to fire trigger");
    } catch (e) { alert("Error: " + e); }
  };

  const handleDelete = async (uuid: string) => {
    if (!confirm("Delete this trigger?")) return;
    try { await api.deleteTrigger(uuid); refresh(); } catch (e) { alert("Error: " + e); }
  };

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-xl font-bold">Triggers</h2>
          <p className="text-sm text-text-secondary mt-0.5">One-click actions to start block sessions — optionally synced across devices</p>
        </div>
        <button onClick={() => setShowCreate(true)} className="flex items-center gap-2 px-4 py-2.5 rounded-xl bg-accent hover:bg-accent-hover text-white font-medium text-sm transition-colors">
          <Plus size={16} /> New Trigger
        </button>
      </div>

      <div className="space-y-3">
        {triggers.map(t => (
          <div key={t.uuid} className="flex items-center gap-4 p-5 rounded-xl border border-border bg-surface-raised">
            <div className="p-2.5 rounded-lg bg-warning-soft"><Zap size={20} className="text-warning" /></div>
            <div className="flex-1 min-w-0">
              <h3 className="font-semibold">{t.name}</h3>
              <div className="flex items-center gap-3 text-xs text-text-muted mt-1">
                <span>Block: {blockName(t.block_id)}</span>
                <span className="flex items-center gap-1">
                  {t.propagate_to_network ? <><Wifi size={11} className="text-accent" /> Network</> : <><WifiOff size={11} /> Local</>}
                </span>
              </div>
            </div>
            <button onClick={() => handleFire(t.uuid)} className="flex items-center gap-2 px-4 py-2 rounded-lg bg-success-soft text-success hover:bg-success/20 text-sm font-medium transition-colors">
              <Play size={14} /> Fire
            </button>
            <button onClick={() => handleDelete(t.uuid)} className="p-2 rounded-lg hover:bg-surface-hover text-text-muted hover:text-danger transition-colors">
              <Trash2 size={16} />
            </button>
          </div>
        ))}
        {triggers.length === 0 && (
          <div className="text-center py-16 text-text-muted">
            <Zap size={40} className="mx-auto mb-4 opacity-30" /><p className="font-medium">No triggers yet</p>
            <p className="text-sm mt-1">Create a trigger to quickly start block sessions</p>
          </div>
        )}
      </div>

      <Dialog open={showCreate} onClose={() => setShowCreate(false)} title="Create Trigger">
        <div className="space-y-5">
          <div>
            <label className="block text-sm font-medium text-text-secondary mb-1.5">Trigger Name</label>
            <input value={newName} onChange={e => setNewName(e.target.value)} placeholder="e.g. Deep Work" className="w-full px-4 py-2.5 rounded-xl bg-surface-alt border border-border text-text-primary placeholder:text-text-muted focus:border-accent focus:outline-none" />
          </div>
          <div>
            <label className="block text-sm font-medium text-text-secondary mb-1.5">Block</label>
            <select value={newBlockId} onChange={e => setNewBlockId(e.target.value)} className="w-full px-4 py-2.5 rounded-xl bg-surface-alt border border-border text-text-primary focus:outline-none focus:border-accent">
              <option value="">Select a block...</option>
              {blocks.map(b => <option key={b.id} value={b.id}>{b.name}</option>)}
            </select>
          </div>
          <label className="flex items-center gap-3 p-4 rounded-xl bg-surface-alt border border-border-subtle cursor-pointer">
            <input type="checkbox" checked={newPropagate} onChange={e => setNewPropagate(e.target.checked)} className="accent-accent" />
            <div>
              <span className="text-sm font-medium">Propagate to network</span>
              <p className="text-xs text-text-muted mt-0.5">Fire on all paired devices simultaneously</p>
            </div>
          </label>
          <div className="flex justify-end gap-3 pt-2 border-t border-border">
            <button onClick={() => setShowCreate(false)} className="px-4 py-2 rounded-xl text-sm font-medium text-text-secondary hover:bg-surface-hover">Cancel</button>
            <button onClick={handleCreate} className="px-5 py-2 rounded-xl bg-accent hover:bg-accent-hover text-white text-sm font-medium">Create</button>
          </div>
        </div>
      </Dialog>
    </div>
  );
}
