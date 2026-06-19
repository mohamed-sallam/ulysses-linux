import { useState, useEffect, useCallback } from "react";
import { api, type Block, type UList } from "../lib/api";
import { Dialog } from "./Dialog";
import { Blocks, Plus, Trash2, PenLine, Clock, Lock } from "lucide-react";

export function BlocksPage() {
  const [blocks, setBlocks] = useState<Block[]>([]);
  const [lists, setLists] = useState<UList[]>([]);
  const [editBlock, setEditBlock] = useState<Block | null>(null);
  const [isNew, setIsNew] = useState(false);

  const refresh = useCallback(async () => {
    try {
      const [b, l] = await Promise.all([api.getBlocks(), api.getLists()]);
      setBlocks(b); setLists(l);
    } catch {}
  }, []);
  useEffect(() => { refresh(); }, [refresh]);

  const openNew = () => {
    setEditBlock({ id: "", name: "", lock_type: "timer", lock_param: "3600", list_ids: [] });
    setIsNew(true);
  };
  const openEdit = (b: Block) => { setEditBlock(JSON.parse(JSON.stringify(b))); setIsNew(false); };

  const handleSave = async () => {
    if (!editBlock || !editBlock.name.trim()) return;
    try {
      if (isNew) await api.createBlock({ name: editBlock.name, lock_type: editBlock.lock_type, lock_param: editBlock.lock_param, list_ids: editBlock.list_ids });
      else await api.updateBlock({ id: editBlock.id, name: editBlock.name, lock_type: editBlock.lock_type, lock_param: editBlock.lock_param, list_ids: editBlock.list_ids });
      setEditBlock(null); refresh();
    } catch (e) { alert("Error: " + e); }
  };

  const handleDelete = async (id: string) => {
    if (!confirm("Delete this block?")) return;
    try { await api.deleteBlock(id); refresh(); } catch (e) { alert("Error: " + e); }
  };

  const toggleList = (listId: string) => {
    if (!editBlock) return;
    const ids = editBlock.list_ids.includes(listId) ? editBlock.list_ids.filter(id => id !== listId) : [...editBlock.list_ids, listId];
    setEditBlock({ ...editBlock, list_ids: ids });
  };

  const listName = (id: string) => lists.find(l => l.id === id)?.name || id.slice(0, 8);

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-xl font-bold">Blocks</h2>
          <p className="text-sm text-text-secondary mt-0.5">Combine lists with a locking method to create enforceable blocks</p>
        </div>
        <button onClick={openNew} className="flex items-center gap-2 px-4 py-2.5 rounded-xl bg-accent hover:bg-accent-hover text-white font-medium text-sm transition-colors">
          <Plus size={16} /> New Block
        </button>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        {blocks.map(b => (
          <div key={b.id} className="rounded-xl border border-border bg-surface-raised p-5 flex flex-col hover:border-border/80 transition-colors">
            <div className="flex items-start justify-between mb-4">
              <div className="flex items-center gap-3">
                <div className="p-2.5 rounded-lg bg-accent-soft"><Blocks size={20} className="text-accent" /></div>
                <h3 className="font-semibold">{b.name}</h3>
              </div>
              <div className="flex gap-1">
                <button onClick={() => openEdit(b)} className="p-2 rounded-lg hover:bg-surface-hover text-text-muted hover:text-accent transition-colors"><PenLine size={14} /></button>
                <button onClick={() => handleDelete(b.id)} className="p-2 rounded-lg hover:bg-surface-hover text-text-muted hover:text-danger transition-colors"><Trash2 size={14} /></button>
              </div>
            </div>
            <div className="flex items-center gap-4 text-xs text-text-muted mb-4">
              <span className="flex items-center gap-1.5 px-2.5 py-1 rounded-lg bg-surface-alt">
                {b.lock_type === "timer" ? <Clock size={12} /> : <Lock size={12} />}
                {b.lock_type === "timer" ? `${Math.round(parseInt(b.lock_param) / 60)} min` : "Password"}
              </span>
            </div>
            {b.list_ids.length > 0 ? (
              <div className="flex flex-wrap gap-1.5 mt-auto">
                {b.list_ids.map(id => (
                  <span key={id} className="px-2 py-0.5 rounded-md bg-surface-alt text-xs text-text-secondary">{listName(id)}</span>
                ))}
              </div>
            ) : (
              <p className="text-xs text-text-muted mt-auto">No lists assigned</p>
            )}
          </div>
        ))}
        {blocks.length === 0 && (
          <div className="col-span-full text-center py-16 text-text-muted">
            <Blocks size={40} className="mx-auto mb-4 opacity-30" /><p className="font-medium">No blocks yet</p>
            <p className="text-sm mt-1">Create a block to bundle lists with a locking method</p>
          </div>
        )}
      </div>

      <Dialog open={!!editBlock} onClose={() => setEditBlock(null)} title={isNew ? "Create Block" : "Edit Block"}>
        {editBlock && (
          <div className="space-y-5">
            <div>
              <label className="block text-sm font-medium text-text-secondary mb-1.5">Block Name</label>
              <input value={editBlock.name} onChange={e => setEditBlock({ ...editBlock, name: e.target.value })} placeholder="e.g. Focus Mode" className="w-full px-4 py-2.5 rounded-xl bg-surface-alt border border-border text-text-primary placeholder:text-text-muted focus:border-accent focus:outline-none" />
            </div>
            <div className="grid grid-cols-2 gap-3">
              <div>
                <label className="block text-sm font-medium text-text-secondary mb-1.5">Lock Type</label>
                <select value={editBlock.lock_type} onChange={e => setEditBlock({ ...editBlock, lock_type: e.target.value as "timer" | "password" })} className="w-full px-4 py-2.5 rounded-xl bg-surface-alt border border-border text-text-primary focus:outline-none focus:border-accent">
                  <option value="timer">Timer</option><option value="password">Password</option>
                </select>
              </div>
              <div>
                <label className="block text-sm font-medium text-text-secondary mb-1.5">{editBlock.lock_type === "timer" ? "Duration (min)" : "Password"}</label>
                <input
                  type={editBlock.lock_type === "timer" ? "number" : "password"}
                  value={editBlock.lock_type === "timer" ? Math.round(parseInt(editBlock.lock_param || "3600") / 60) : editBlock.lock_param}
                  onChange={e => setEditBlock({ ...editBlock, lock_param: editBlock.lock_type === "timer" ? String(parseInt(e.target.value || "60") * 60) : e.target.value })}
                  className="w-full px-4 py-2.5 rounded-xl bg-surface-alt border border-border text-text-primary focus:outline-none focus:border-accent"
                />
              </div>
            </div>
            <div>
              <label className="block text-sm font-medium text-text-secondary mb-2">Assign Lists</label>
              <div className="space-y-1.5 max-h-48 overflow-y-auto">
                {lists.map(l => (
                  <label key={l.id} className={`flex items-center gap-3 p-3 rounded-xl cursor-pointer transition-colors ${editBlock.list_ids.includes(l.id) ? "bg-accent-soft border border-accent/30" : "bg-surface-alt border border-border-subtle hover:border-border"}`}>
                    <input type="checkbox" checked={editBlock.list_ids.includes(l.id)} onChange={() => toggleList(l.id)} className="accent-accent" />
                    <span className="text-sm">{l.name}</span>
                    <span className="text-xs text-text-muted ml-auto">{l.entries.length} entries</span>
                  </label>
                ))}
                {lists.length === 0 && <p className="text-sm text-text-muted text-center py-4">Create lists first</p>}
              </div>
            </div>
            <div className="flex justify-end gap-3 pt-2 border-t border-border">
              <button onClick={() => setEditBlock(null)} className="px-4 py-2 rounded-xl text-sm font-medium text-text-secondary hover:bg-surface-hover">Cancel</button>
              <button onClick={handleSave} className="px-5 py-2 rounded-xl bg-accent hover:bg-accent-hover text-white text-sm font-medium">{isNew ? "Create" : "Save"}</button>
            </div>
          </div>
        )}
      </Dialog>
    </div>
  );
}
