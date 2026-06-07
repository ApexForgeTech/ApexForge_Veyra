import { PersonaState, postVeyraAction, isVeyraHosted } from '../types/veyra'

interface Props {
  personas: PersonaState[]
  active: PersonaState
}

const MODE_COLORS: Record<string, string> = {
  casual:   'blue',
  hardened: 'yellow',
  ghost:    'purple',
  redteam:  'red',
  airgap:   'red',
}

const ROUTE_ICONS = {
  direct: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <path d="M5 12h14M12 5l7 7-7 7" />
    </svg>
  ),
  vpn: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <rect x="3" y="11" width="18" height="11" rx="2" ry="2" />
      <path d="M7 11V7a5 5 0 0 1 10 0v4" />
    </svg>
  ),
  tor: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <circle cx="12" cy="12" r="10" />
      <circle cx="12" cy="12" r="6" />
      <circle cx="12" cy="12" r="2" />
    </svg>
  ),
  chained: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <line x1="22" y1="2" x2="11" y2="13" />
      <polygon points="22 2 15 2 22 9 22 2" />
      <path d="M11 13a4 4 0 1 1-5.66-5.66L8 10" />
      <path d="M13 11a4 4 0 1 1 5.66 5.66L16 14" />
    </svg>
  ),
  default: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <polygon points="12 2 2 7 12 12 22 7 12 2" />
      <polyline points="2 17 12 22 22 17" />
      <polyline points="2 12 12 17 22 12" />
    </svg>
  )
}

function getRouteIcon(routeId: string) {
  if (routeId.includes('tor')) return ROUTE_ICONS.tor;
  if (routeId.includes('vpn')) return ROUTE_ICONS.vpn;
  if (routeId.includes('chained')) return ROUTE_ICONS.chained;
  if (routeId.includes('direct')) return ROUTE_ICONS.direct;
  return ROUTE_ICONS.default;
}

function modeColor(mode: string): string {
  return MODE_COLORS[mode] ?? 'muted'
}

export default function PersonaSwitcher({ personas, active }: Props) {
  const switchPersona = (id: string) => {
    if (!isVeyraHosted()) return
    console.log('Requesting switch context for persona:', id)
    postVeyraAction({ action: 'request_state_refresh' })
  }

  return (
    <div className="persona-switcher">
      <div className="section-title">Available Personas</div>
      <div className="persona-list">
        {personas.map(p => (
          <div
            key={p.id}
            className={`persona-row${p.active ? ' persona-row--active' : ''}`}
            onClick={() => switchPersona(p.id)}
            style={{ cursor: 'pointer' }}
          >
            <div className="persona-row__left">
              <span className={`dot dot--${p.active ? 'blue' : 'muted'}`} />
              <span className="persona-row__name">{p.display_name}</span>
              {p.ephemeral && <span className="badge badge--purple" style={{ fontSize: 9 }}>ephemeral</span>}
            </div>
            <div className="persona-row__right">
              <span className={`badge badge--${modeColor(p.security_mode)}`}>
                {p.security_mode}
              </span>
              <span className="persona-route-icon" title={p.route_profile_id} style={{ display: 'flex', alignItems: 'center', width: 16, height: 16 }}>
                {getRouteIcon(p.route_profile_id)}
              </span>
            </div>
          </div>
        ))}
      </div>

      <div className="divider" />

      <div className="section-title" style={{ marginTop: 14 }}>Active Profile Details</div>
      <div className="active-persona-card card">
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 14 }}>
          <span style={{ fontWeight: 800, color: 'var(--accent)', fontSize: 14, letterSpacing: '0.02em' }}>
            {active.display_name}
          </span>
          <span className={`badge badge--${modeColor(active.security_mode)}`} style={{ padding: '4px 10px', fontSize: 10 }}>
            {active.security_mode}
          </span>
        </div>

        <div className="persona-detail-row" style={{ borderBottom: '1px solid rgba(42, 166, 255, 0.05)', paddingBottom: 8, marginBottom: 8 }}>
          <span className="persona-detail-label">Security Shield</span>
          <span className="mono" style={{ color: 'var(--text-primary)' }}>{active.security_mode.toUpperCase()} MODE</span>
        </div>

        <div className="persona-detail-row" style={{ borderBottom: '1px solid rgba(42, 166, 255, 0.05)', paddingBottom: 8, marginBottom: 8 }}>
          <span className="persona-detail-label">Ghost Routing</span>
          <span className="mono" style={{ color: 'var(--accent-bright)', display: 'inline-flex', alignItems: 'center', gap: 6 }}>
            <span style={{ width: 12, height: 12, display: 'inline-block' }}>{getRouteIcon(active.route_profile_id)}</span>
            {active.route_profile_id}
          </span>
        </div>

        <div className="persona-detail-row" style={{ borderBottom: '1px solid rgba(42, 166, 255, 0.05)', paddingBottom: 8, marginBottom: 8 }}>
          <span className="persona-detail-label">Canvas Entropy</span>
          <span className="mono" style={{ color: 'var(--text-secondary)' }}>{active.fingerprint_profile_id}</span>
        </div>

        <div className="persona-detail-row" style={{ borderBottom: '1px solid rgba(42, 166, 255, 0.05)', paddingBottom: 8, marginBottom: 8 }}>
          <span className="persona-detail-label">Sandbox Policies</span>
          <span className="mono" style={{ color: 'var(--text-secondary)' }}>{active.extension_policy_id}</span>
        </div>

        <div className="persona-detail-row" style={{ paddingBottom: 4 }}>
          <span className="persona-detail-label">Memory Enclave</span>
          <span className={`badge badge--${active.ephemeral ? 'purple' : 'blue'}`} style={{ textTransform: 'uppercase', fontWeight: 700 }}>
            {active.ephemeral ? 'RAM Only' : 'Persistent Vault'}
          </span>
        </div>

        {isVeyraHosted() && (
          <div style={{ marginTop: 18, display: 'flex', gap: 8 }}>
            <button
              className="btn btn--ghost"
              style={{ flex: 1, justifyContent: 'center', paddingTop: 8, paddingBottom: 8 }}
              onClick={() => postVeyraAction({ action: 'request_state_refresh' })}
              title="Refresh state from shell"
            >
              <svg style={{ width: 14, height: 14, marginRight: 4 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <path d="M21.5 2v6h-6M21.34 15.57a10 10 0 1 1-.57-8.38l5.67-5.67" />
              </svg>
              Refresh
            </button>
            <button
              className="btn btn--primary"
              style={{ flex: 1, justifyContent: 'center', paddingTop: 8, paddingBottom: 8 }}
              onClick={() => postVeyraAction({ action: 'open_tab', url: 'about:blank' })}
              title="Open a new tab"
            >
              <svg style={{ width: 14, height: 14, marginRight: 4 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
                <line x1="12" y1="5" x2="12" y2="19"></line>
                <line x1="5" y1="12" x2="19" y2="12"></line>
              </svg>
              New Tab
            </button>
          </div>
        )}
      </div>
    </div>
  )
}
