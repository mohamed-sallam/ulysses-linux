import { useState, useEffect, useCallback } from "react";
import { api, type UList, type ListEntry } from "../lib/api";
import { Dialog } from "./Dialog";
import { List, Plus, Trash2, Globe, AppWindow, PenLine, ShieldCheck, ShieldX } from "lucide-react";

const RULE_ICONS: Record<string, typeof Globe> = {
  domain: Globe, url: Globe, wildcard: Globe,
  app_name: AppWindow, window_title: AppWindow,
};

export function ListsPage() {
  const [lists, setLists] = useState<UList[]>([]);
  const [editList, setEditList] = useState<UList | null>(null);
  const [isNew, setIsNew] = useState(false);

  const refresh = useCallback(async () => {
    try { setLists(await api.getLists()); } catch {}
  }, []);
  useEffect(() => { refresh(); }, [refresh]);

  const openNew = () => { setEditList({ id: "", name: "", entries: [] }); setIsNew(true); };
  const openEdit = (l: UList) => { setEditList(JSON.parse(JSON.stringify(l))); setIsNew(false); };

  const handleSave = async () => {
    if (!editList || !editList.name.trim()) return;
    try {
      if (isNew) await api.createList({ name: editList.name, entries: editList.entries });
      else await api.updateList({ id: editList.id, name: editList.name, entries: editList.entries });
      setEditList(null); refresh();
    } catch (e) { alert("Error: " + e); }
  };

  const handleDelete = async (id: string) => {
    if (!confirm("Delete this list?")) return;
    try { await api.deleteList(id); refresh(); } catch (e) { alert("Error: " + e); }
  };

  const addEntry = () => {
    if (!editList) return;
    setEditList({ ...editList, entries: [...editList.entries, { type: "deny", value: "", rule_type: "domain" }] });
  };

  const updateEntry = (idx: number, field: keyof ListEntry, val: string) => {
    if (!editList) return;
    const e = [...editList.entries]; e[idx] = { ...e[idx], [field]: val };
    setEditList({ ...editList, entries: e });
  };

  const removeEntry = (idx: number) => {
    if (!editList) return;
    setEditList({ ...editList, entries: editList.entries.filter((_, i) => i !== idx) });
  };

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-xl font-bold">Lists</h2>
          <p className="text-sm text-text-secondary mt-0.5">Define what to block or allow</p>
        </div>
        <button onClick={openNew} className="flex items-center gap-2 px-4 py-2.5 rounded-xl bg-accent hover:bg-accent-hover text-white font-medium text-sm transition-colors">
          <Plus size={16} /> New List
        </button>
      </div>

      <div className="space-y-4">
        {lists.map((l) => {
          const dc = l.entries.filter(e => e.type === "deny").length;
          const ac = l.entries.filter(e => e.type === "allow").length;
          return (
            <div key={l.id} className="rounded-xl border border-border bg-surface-raised overflow-hidden">
              <div className="flex items-center gap-4 p-5">
                <div className="p-2.5 rounded-lg bg-accent-soft"><List size={20} className="text-accent" /></div>
                <div className="flex-1 min-w-0">
                  <h3 className="font-semibold truncate">{l.name}</h3>
                  <div className="flex items-center gap-3 text-xs text-text-muted mt-1">
                    {dc > 0 && <span className="flex items-center gap-1"><ShieldX size={11} className="text-danger" />{dc} denied</span>}
                    {ac > 0 && <span className="flex items-center gap-1"><ShieldCheck size={11} className="text-success" />{ac} allowed</span>}
                    {l.entries.length === 0 && <span>Empty</span>}
                  </div>
                </div>
                <button onClick={() => openEdit(l)} className="p-2 rounded-lg hover:bg-surface-hover text-text-muted hover:text-accent transition-colors"><PenLine size={16} /></button>
                <button onClick={() => handleDelete(l.id)} className="p-2 rounded-lg hover:bg-surface-hover text-text-muted hover:text-danger transition-colors"><Trash2 size={16} /></button>
              </div>
              {l.entries.length > 0 && (
                <div className="border-t border-border-subtle px-5 py-3 bg-surface-alt/50">
                  <div className="flex flex-wrap gap-2">
                    {l.entries.slice(0, 8).map((e, i) => {
                      const Icon = RULE_ICONS[e.rule_type] || Globe;
                      return (<span key={i} className={`inline-flex items-center gap-1.5 px-2.5 py-1 rounded-lg text-xs font-medium ${e.type === "deny" ? "bg-danger-soft text-danger" : "bg-success-soft text-success"}`}><Icon size={11} />{e.value}</span>);
                    })}
                    {l.entries.length > 8 && <span className="text-xs text-text-muted px-2 py-1">+{l.entries.length - 8} more</span>}
                  </div>
                </div>
              )}
            </div>
          );
        })}
        {lists.length === 0 && (
          <div className="text-center py-16 text-text-muted">
            <List size={40} className="mx-auto mb-4 opacity-30" /><p className="font-medium">No lists yet</p>
            <p className="text-sm mt-1">Create your first list to start defining what to block</p>
          </div>
        )}
      </div>

      <Dialog open={!!editList} onClose={() => setEditList(null)} title={isNew ? "Create List" : "Edit List"} width="max-w-2xl">
        {editList && (
          <div className="space-y-5">
            <div>
              <label className="block text-sm font-medium text-text-secondary mb-1.5">List Name</label>
              <input value={editList.name} onChange={e => setEditList({ ...editList, name: e.target.value })} placeholder="e.g. Social Media" className="w-full px-4 py-2.5 rounded-xl bg-surface-alt border border-border text-text-primary placeholder:text-text-muted focus:border-accent focus:outline-none" />
            </div>
            <div>
              <div className="flex items-center justify-between mb-3">
                <label className="text-sm font-medium text-text-secondary">Entries</label>
                <button onClick={addEntry} className="flex items-center gap-1.5 text-xs font-medium text-accent hover:text-accent-hover"><Plus size={14} /> Add</button>
              </div>
              <div className="space-y-2 max-h-72 overflow-y-auto">
                {editList.entries.map((entry, idx) => (
                  <div key={idx} className="flex items-center gap-2 p-3 rounded-xl bg-surface-alt border border-border-subtle">
                    <select value={entry.type} onChange={e => updateEntry(idx, "type", e.target.value)} className="px-2 py-1.5 rounded-lg bg-surface border border-border text-xs font-medium text-text-primary focus:outline-none">
                      <option value="deny">Deny</option><option value="allow">Allow</option>
                    </select>
                    <select value={entry.rule_type} onChange={e => updateEntry(idx, "rule_type", e.target.value)} className="px-2 py-1.5 rounded-lg bg-surface border border-border text-xs font-medium text-text-primary focus:outline-none">
                      <option value="domain">Domain</option><option value="url">URL</option><option value="wildcard">Wildcard</option><option value="app_name">App Name</option><option value="window_title">Window Title</option>
                    </select>
                    <input value={entry.value} onChange={e => updateEntry(idx, "value", e.target.value)} placeholder="e.g. facebook.com" className="flex-1 px-3 py-1.5 rounded-lg bg-surface border border-border text-sm text-text-primary placeholder:text-text-muted focus:outline-none focus:border-accent" />
                    <button onClick={() => removeEntry(idx)} className="p-1.5 rounded-lg hover:bg-danger-soft text-text-muted hover:text-danger"><Trash2 size={14} /></button>
                  </div>
                ))}
                {editList.entries.length === 0 && <p className="text-sm text-text-muted text-center py-6">No entries — add one above</p>}
              </div>
            </div>
            <div className="flex justify-end gap-3 pt-2 border-t border-border">
              <button onClick={() => setEditList(null)} className="px-4 py-2 rounded-xl text-sm font-medium text-text-secondary hover:bg-surface-hover">Cancel</button>
              <button onClick={handleSave} className="px-5 py-2 rounded-xl bg-accent hover:bg-accent-hover text-white text-sm font-medium">{isNew ? "Create" : "Save"}</button>
            </div>
          </div>
        )}
      </Dialog>
    </div>
  );
}
