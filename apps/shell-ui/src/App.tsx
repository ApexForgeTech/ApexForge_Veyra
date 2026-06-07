import { useState, useEffect, useCallback } from 'react'
import { VeyraState } from './types/veyra'
import PersonaSwitcher from './components/PersonaSwitcher'
import RoutePanel from './components/RoutePanel'
import SecurityDashboard from './components/SecurityDashboard'
import VaultMonitor from './components/VaultMonitor'
import ToolLauncher from './components/ToolLauncher'
import AiPanel from './components/AiPanel'
import OsintPanel from './components/OsintPanel'
import CommandPalette from './components/CommandPalette'

type Tab = 'persona' | 'route' | 'security' | 'vault' | 'tools' | 'ai' | 'osint'

const TABS: { id: Tab; label: string; icon: string }[] = [
  { id: 'persona',   label: 'Personas',  icon: '⊕' },
  { id: 'route',     label: 'Route',     icon: '⬡' },
  { id: 'security',  label: 'Guard',     icon: '⊙' },
  { id: 'vault',     label: 'Vault',     icon: '⊡' },
  { id: 'tools',     label: 'Tools',     icon: '⚙' },
  { id: 'ai',        label: 'AI',        icon: '✦' },
  { id: 'osint',     label: 'Recon',     icon: '⌖' },
]

const TAB_TITLES: Record<Tab, { title: string; subtitle: string }> = {
  persona:  { title: 'Digital Personas',    subtitle: 'Identity isolation & persona management' },
  route:    { title: 'GhostNet Routing',    subtitle: 'Network path control & leak prevention' },
  security: { title: 'Sentinel Guard',      subtitle: 'Security posture & permission control' },
  vault:    { title: 'BlackVault',          subtitle: 'Quarantine events & artifact staging' },
  tools:    { title: 'Tool Bridge',         subtitle: 'External tool invocation & management' },
  ai:       { title: 'Cortex AI',           subtitle: 'AI-powered analysis & threat detection' },
  osint:    { title: 'Recon Workspace',     subtitle: 'Intelligence gathering & investigation' },
}

function getPlaceholderState(): VeyraState {
  return {
    active_persona: {
      id: 'loading', display_name: 'Loading…',
      security_mode: 'casual', route_profile_id: 'direct_isp',
      fingerprint_profile_id: 'native_stable', extension_policy_id: 'trusted_daily',
      ephemeral: false, active: true,
    },
    all_personas: [],
    active_route: {
      persona_id: 'loading', route_profile_id: 'direct_isp', route_type: 'direct',
      health_status: 'unknown', proxy_uri: '', dns_resolver: 'system-resolver',
      leak_status: 'open', diagnostic_summary: 'Waiting for shell state…',
    },
    all_route_profiles: [],
    security_mode_id: 'casual', security_mode_name: 'Casual',
    fingerprint_profile_id: 'native_stable', extension_policy_id: 'trusted_daily',
    allow_eval: true, block_mixed_content: false, blocked_domains_count: 0,
    permissions: [], vault_events: [], tools: [],
    ai_policy: {
      id: 'loading', display_name: 'Loading…', enabled: false,
      model: 'none', endpoint: 'none',
      allow_page_content: false, allow_script_analysis: false,
      allow_phishing_check: false, retain_memory: false, max_input_chars: 0,
    },
    ai_result: null,
    osint_policy: {
      id: 'loading', display_name: 'Loading…', enabled: false,
      require_route: true, allow_whois: false, allow_dns: false,
      allow_archive: false, allow_username_search: false, allow_active_probing: false,
      max_username_sites: 0, retain_cases: false,
    },
    osint_case: {
      active: false, case_id: '', last_ok: true, last_summary: '', last_error: '',
      egress: '', route_type: '', entities: [], relationships: [], timeline: [],
    },
    runtime_root: '', shell_version: '0.11',
  }
}

export default function App() {
  const [state, setState] = useState<VeyraState>(() => {
    return window.__VEYRA_STATE__ ?? getPlaceholderState()
  })
  const [activeTab, setActiveTab] = useState<Tab>('persona')
  const [paletteOpen, setPaletteOpen] = useState(false)

  // Called by C++ shell to push updated state. Keep window.__VEYRA_STATE__ in
  // sync too, so any code that reads it later sees the freshest value.
  useEffect(() => {
    window.__VEYRA_UPDATE__ = (newState: VeyraState) => {
      window.__VEYRA_STATE__ = newState
      setState(newState)
    }
    // If C++ pushed state between document-start and this effect, adopt it.
    if (window.__VEYRA_STATE__) {
      setState(window.__VEYRA_STATE__)
    }
    return () => { window.__VEYRA_UPDATE__ = undefined }
  }, [])

  const handleKeyDown = useCallback((e: KeyboardEvent) => {
    if ((e.ctrlKey || e.metaKey) && e.key === 'k') {
      e.preventDefault()
      setPaletteOpen(p => !p)
    }
    if (e.key === 'Escape' && paletteOpen) {
      setPaletteOpen(false)
    }
  }, [paletteOpen])

  useEffect(() => {
    window.addEventListener('keydown', handleKeyDown)
    return () => window.removeEventListener('keydown', handleKeyDown)
  }, [handleKeyDown])

  const vaultCount = state.vault_events.filter(
    e => e.event_type === 'quarantine-complete' || e.event_type === 'intercepted'
  ).length

  const routeHealthColor =
    state.active_route.health_status === 'healthy' ? 'var(--success)' :
    state.active_route.health_status === 'degraded' ? 'var(--warning)' : 'var(--danger)'

  const tabInfo = TAB_TITLES[activeTab]

  return (
    <div className="app">
      {/* Sidebar */}
      <aside className="app-sidebar">
        <header className="app-header">
          <div className="app-header__brand">
            <span className="app-header__logo">⬡</span>
            <span className="app-header__name">VEYRA</span>
          </div>
          <div className="app-header__status">
            <span className="dot"
              style={{ background: routeHealthColor, boxShadow: `0 0 8px ${routeHealthColor}` }} />
            <span className="app-header__persona">{state.active_persona.display_name}</span>
          </div>
          <button
            className="app-header__palette-btn"
            onClick={() => setPaletteOpen(true)}
            title="Command palette (Ctrl+K)"
          >
            ⌘K
          </button>
        </header>

        {/* Tab bar - vertical */}
        <nav className="app-tabs">
          {TABS.map(tab => (
            <button
              key={tab.id}
              className={`app-tab${activeTab === tab.id ? ' app-tab--active' : ''}`}
              onClick={() => setActiveTab(tab.id)}
            >
              <span className="app-tab__icon">{tab.icon}</span>
              <span className="app-tab__label">{tab.label}</span>
              {tab.id === 'vault' && vaultCount > 0 && (
                <span className="app-tab__badge">{vaultCount}</span>
              )}
            </button>
          ))}
        </nav>
      </aside>

      {/* Main content */}
      <div className="app-main">
        <div className="app-main__topbar">
          <div className="app-main__topbar-left">
            <div>
              <div className="app-main__topbar-title">{tabInfo.title}</div>
              <div className="app-main__topbar-subtitle">{tabInfo.subtitle}</div>
            </div>
          </div>
        </div>

        <main className="app-panel">
          {activeTab === 'persona'  && (
            <PersonaSwitcher personas={state.all_personas} active={state.active_persona} />
          )}
          {activeTab === 'route'    && (
            <RoutePanel route={state.active_route} allProfiles={state.all_route_profiles} />
          )}
          {activeTab === 'security' && (
            <SecurityDashboard state={state} />
          )}
          {activeTab === 'vault'    && (
            <VaultMonitor events={state.vault_events} />
          )}
          {activeTab === 'tools'    && (
            <ToolLauncher tools={state.tools} />
          )}
          {activeTab === 'ai'       && (
            <AiPanel policy={state.ai_policy} result={state.ai_result} />
          )}
          {activeTab === 'osint'    && (
            <OsintPanel policy={state.osint_policy} osintCase={state.osint_case} />
          )}
        </main>
      </div>

      {/* Command palette */}
      {paletteOpen && (
        <CommandPalette state={state} onClose={() => setPaletteOpen(false)} />
      )}
    </div>
  )
}
