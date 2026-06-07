import { useState } from 'react'
import { RouteState, RouteProfile, postVeyraAction, isVeyraHosted } from '../types/veyra'

interface Props {
  route: RouteState
  allProfiles: RouteProfile[]
}

function healthColor(status: string): string {
  if (status === 'healthy') return 'green'
  if (status === 'degraded') return 'yellow'
  return 'red'
}

function leakColor(status: string): string {
  if (status === 'sealed') return 'green'
  if (status === 'guarded') return 'yellow'
  return 'red'
}

const TYPE_DESCRIPTIONS: Record<string, string> = {
  direct:           'System ISP — Direct connection without proxy layers.',
  vpn:              'Single-hop encrypted tunnel (SOCKS5/WireGuard).',
  tor:              'Multi-hop onion routing network with directory authority nodes.',
  i2p:              'Garlic-routing decentralized overlay network.',
  chained:          'Nested routing path: Local Client → VPN tunnel → Tor onion bridge.',
  residential_proxy:'Consumer-grade residential ISP proxy simulation node.',
}

export default function RoutePanel({ route, allProfiles }: Props) {
  const [switching, setSwitching] = useState(false)
  const [selectedProfile, setSelectedProfile] = useState(route.route_profile_id)

  const handleSwitch = () => {
    if (!isVeyraHosted()) return
    setSwitching(true)
    postVeyraAction({ action: 'switch_route', route_profile_id: selectedProfile })
    setTimeout(() => setSwitching(false), 2000)
  }

  // Generate visual route hops based on route type
  const renderVisualRoute = () => {
    const isTor = route.route_type.includes('tor');
    const isVpn = route.route_type.includes('vpn');
    const isChained = route.route_type.includes('chained');
    const isDirect = route.route_type.includes('direct');

    return (
      <div style={{
        margin: '18px 0',
        padding: '16px',
        background: 'rgba(2, 6, 12, 0.4)',
        border: '1px solid rgba(42, 166, 255, 0.08)',
        borderRadius: 'var(--radius-md)',
        display: 'flex',
        flexDirection: 'column',
        gap: 12
      }}>
        <div style={{ fontSize: 9, textTransform: 'uppercase', letterSpacing: '0.08em', color: 'var(--text-muted)', fontWeight: 700 }}>
          Active Link Topography
        </div>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', position: 'relative', padding: '0 8px' }}>
          
          {/* Node 1: Origin */}
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 4, zIndex: 2 }}>
            <div style={{
              width: 24, height: 24, borderRadius: '50%', 
              background: 'var(--accent-dim)', border: '1px solid var(--accent)',
              display: 'flex', alignItems: 'center', justifyContent: 'center',
              color: 'var(--accent-bright)', fontSize: 10, fontWeight: 700
            }}>
              TX
            </div>
            <span style={{ fontSize: 9, fontWeight: 600, color: 'var(--text-secondary)' }}>Enclave</span>
          </div>

          {/* Connection Line 1 */}
          <div style={{
            flex: 1, height: 2, 
            background: isDirect ? 'rgba(42, 166, 255, 0.1)' : 'var(--accent)',
            boxShadow: isDirect ? 'none' : '0 0 8px var(--accent)',
            margin: '0 -4px', zIndex: 1
          }} />

          {/* Node 2: Intermediate Hop */}
          {!isDirect && (
            <>
              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 4, zIndex: 2 }}>
                <div style={{
                  width: 24, height: 24, borderRadius: '50%',
                  background: isChained ? 'var(--purple-dim)' : 'var(--accent-dim)', 
                  border: isChained ? '1px solid var(--purple)' : '1px solid var(--accent)',
                  display: 'flex', alignItems: 'center', justifyContent: 'center',
                  color: isChained ? 'var(--purple-bright)' : 'var(--accent-bright)', fontSize: 10, fontWeight: 700
                }}>
                  {isChained ? 'VPN' : isVpn ? 'VPN' : isTor ? 'TOR' : 'PRX'}
                </div>
                <span style={{ fontSize: 9, fontWeight: 600, color: 'var(--text-secondary)' }}>
                  {isChained ? 'Proxy' : isVpn ? 'Tunnel' : isTor ? 'Node 1' : 'Proxy'}
                </span>
              </div>

              {/* Connection Line 2 */}
              <div style={{
                flex: 1, height: 2, 
                background: isChained ? 'var(--purple)' : 'var(--accent)',
                boxShadow: isChained ? '0 0 8px var(--purple)' : '0 0 8px var(--accent)',
                margin: '0 -4px', zIndex: 1
              }} />
            </>
          )}

          {/* Node 3: Exit/Target */}
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 4, zIndex: 2 }}>
            <div style={{
              width: 24, height: 24, borderRadius: '50%', 
              background: 'rgba(255, 59, 82, 0.1)', border: '1px solid var(--danger)',
              display: 'flex', alignItems: 'center', justifyContent: 'center',
              color: 'var(--danger)', fontSize: 10, fontWeight: 700
            }}>
              RX
            </div>
            <span style={{ fontSize: 9, fontWeight: 600, color: 'var(--text-secondary)' }}>WAN</span>
          </div>

        </div>
      </div>
    );
  };

  return (
    <div className="route-panel">
      <div className="section-title">Active Gateway</div>

      <div className="card" style={{ marginBottom: 12 }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 10 }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
            <span className={`dot dot--${healthColor(route.health_status)}`} />
            <span style={{ fontWeight: 800, fontSize: 14, color: 'var(--text-primary)', letterSpacing: '0.01em' }}>
              {route.route_profile_id.toUpperCase()}
            </span>
          </div>
          <span className={`badge badge--${healthColor(route.health_status)}`}>
            {route.health_status}
          </span>
        </div>

        <div style={{ color: 'var(--text-secondary)', fontSize: 11, lineHeight: 1.5, marginBottom: 14 }}>
          {TYPE_DESCRIPTIONS[route.route_type] ?? route.route_type}
        </div>

        {renderVisualRoute()}

        <div className="route-stat-grid" style={{ marginTop: 14 }}>
          <div className="route-stat">
            <span className="route-stat__label">Protocol Stack</span>
            <span className={`badge badge--blue`} style={{ alignSelf: 'flex-start' }}>{route.route_type}</span>
          </div>
          <div className="route-stat">
            <span className="route-stat__label">Leak Sentinel</span>
            <span className={`badge badge--${leakColor(route.leak_status)}`} style={{ alignSelf: 'flex-start' }}>
              {route.leak_status}
            </span>
          </div>
          <div className="route-stat" style={{ gridColumn: '1 / -1', borderTop: '1px solid rgba(42, 166, 255, 0.05)', paddingTop: 8, marginTop: 4 }}>
            <span className="route-stat__label">Resolver Policy</span>
            <span className="mono" style={{ color: 'var(--text-primary)', fontSize: 11 }}>
              {route.dns_resolver}
            </span>
          </div>
          {route.proxy_uri && (
            <div className="route-stat" style={{ gridColumn: '1 / -1', borderTop: '1px solid rgba(42, 166, 255, 0.05)', paddingTop: 8 }}>
              <span className="route-stat__label">Gateway Address</span>
              <span className="mono" style={{ color: 'var(--accent)', fontSize: 11, wordBreak: 'break-all' }}>
                {route.proxy_uri}
              </span>
            </div>
          )}
        </div>
      </div>

      {route.diagnostic_summary && (
        <div style={{
          fontSize: 11,
          color: 'var(--text-secondary)',
          lineHeight: 1.6,
          padding: '10px 14px',
          background: 'rgba(2, 6, 12, 0.3)',
          borderRadius: 'var(--radius-md)',
          border: '1px solid var(--border)',
          marginBottom: 12,
          display: 'flex',
          gap: 8,
          alignItems: 'flex-start'
        }}>
          <svg style={{ width: 14, height: 14, color: 'var(--accent-bright)', flexShrink: 0, marginTop: 1 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
            <circle cx="12" cy="12" r="10" />
            <line x1="12" y1="16" x2="12" y2="12" />
            <line x1="12" y1="8" x2="12.01" y2="8" />
          </svg>
          <div>{route.diagnostic_summary}</div>
        </div>
      )}

      {isVeyraHosted() && allProfiles.length > 0 && (
        <div className="route-switch-section" style={{ marginTop: 16 }}>
          <div className="section-title">Modify Operational Route</div>
          <div style={{ position: 'relative', width: '100%' }}>
            <select
              className="route-select"
              value={selectedProfile}
              onChange={e => setSelectedProfile(e.target.value)}
              style={{ width: '100%', paddingRight: 30 }}
            >
              {allProfiles.map(p => (
                <option key={p.id} value={p.id}>
                  {p.display_name} ({p.route_type.toUpperCase()})
                </option>
              ))}
            </select>
          </div>
          <button
            className={`btn btn--primary`}
            style={{ width: '100%', justifyContent: 'center', marginTop: 8 }}
            onClick={handleSwitch}
            disabled={switching || selectedProfile === route.route_profile_id}
          >
            {switching ? (
              <>
                <svg className="shimmer" style={{ width: 14, height: 14, animation: 'logoGlow 1s infinite alternate', marginRight: 6 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                  <path d="M21.5 2v6h-6M21.34 15.57a10 10 0 1 1-.57-8.38l5.67-5.67" />
                </svg>
                Applying Route Posture...
              </>
            ) : (
              <>
                <svg style={{ width: 14, height: 14, marginRight: 6 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                  <polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"></polygon>
                </svg>
                Apply Operational Path
              </>
            )}
          </button>
        </div>
      )}
    </div>
  )
}
