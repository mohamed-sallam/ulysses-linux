import { useState } from "react";
import { DashboardPage } from "./components/DashboardPage";
import { ListsPage } from "./components/ListsPage";
import { BlocksPage } from "./components/BlocksPage";
import { TriggersPage } from "./components/TriggersPage";
import { NetworkPage } from "./components/NetworkPage";
import { LayoutDashboard, List, Blocks, Zap, Globe } from "lucide-react";

const NAV = [
  { id: "dashboard", label: "Dashboard", icon: LayoutDashboard },
  { id: "lists",     label: "Lists",     icon: List },
  { id: "blocks",    label: "Blocks",    icon: Blocks },
  { id: "triggers",  label: "Triggers",  icon: Zap },
  { id: "network",   label: "Network",   icon: Globe },
] as const;

type PageId = (typeof NAV)[number]["id"];

export default function App() {
  const [active, setActive] = useState<PageId>("dashboard");

  return (
    <div className="flex h-screen bg-surface">
      {/* Sidebar */}
      <nav className="w-56 flex flex-col bg-surface-alt border-r border-border shrink-0">
        <div className="px-5 py-6">
          <h1 className="text-lg font-bold tracking-tight text-text-primary">Ulysses</h1>
          <p className="text-[11px] text-text-muted mt-0.5">Distraction Blocker</p>
        </div>
        <div className="flex-1 px-3 space-y-0.5">
          {NAV.map(({ id, label, icon: Icon }) => (
            <button
              key={id}
              onClick={() => setActive(id)}
              className={`w-full flex items-center gap-3 px-3 py-2.5 rounded-xl text-sm font-medium transition-all duration-150 ${
                active === id
                  ? "bg-accent text-white shadow-md shadow-accent/20"
                  : "text-text-secondary hover:text-text-primary hover:bg-surface-hover"
              }`}
            >
              <Icon size={18} />
              {label}
            </button>
          ))}
        </div>
        <div className="px-5 py-4 border-t border-border">
          <p className="text-[10px] text-text-muted">v0.1.0 · Daemon Connected</p>
        </div>
      </nav>

      {/* Main */}
      <main className="flex-1 overflow-y-auto p-8">
        <div className="max-w-5xl mx-auto">
          {active === "dashboard" && <DashboardPage />}
          {active === "lists" && <ListsPage />}
          {active === "blocks" && <BlocksPage />}
          {active === "triggers" && <TriggersPage />}
          {active === "network" && <NetworkPage />}
        </div>
      </main>
    </div>
  );
}
