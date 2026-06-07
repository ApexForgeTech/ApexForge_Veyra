import { useState } from 'react'
import { OsintPolicy, OsintCase, OsintEntity, postVeyraAction, isVeyraHosted } from '../types/veyra'

interface Props {
  policy: OsintPolicy
  osintCase: OsintCase
}

const ENTITY_ICONS: Record<string, React.ReactNode> = {
  domain: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <circle cx="12" cy="12" r="10" />
      <line x1="2" y1="12" x2="22" y2="12" />
      <path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z" />
    </svg>
  ),
  ip: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <rect x="5" y="2" width="14" height="20" rx="2" ry="2" />
      <line x1="12" y1="18" x2="12.01" y2="18" />
    </svg>
  ),
  email: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <path d="M4 4h16c1.1 0 2 .9 2 2v12c0 1.1-.9 2-2 2H4c-1.1 0-2-.9-2-2V6c0-1.1.9-2 2-2z" />
      <polyline points="22,6 12,13 2,6" />
    </svg>
  ),
  username: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2" />
      <circle cx="12" cy="7" r="4" />
    </svg>
  ),
  url: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <path d="M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71" />
      <path d="M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71" />
    </svg>
  ),
  org: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <rect x="2" y="7" width="20" height="14" rx="2" ry="2" />
      <path d="M16 21V5a2 2 0 0 0-2-2h-4a2 2 0 0 0-2 2v16" />
    </svg>
  ),
  dns_record: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z" />
      <polyline points="14 2 14 8 20 8" />
      <line x1="16" y1="13" x2="8" y2="13" />
      <line x1="16" y1="17" x2="8" y2="17" />
      <polyline points="10 9 9 9 8 9" />
    </svg>
  ),
}

function getEntityIcon(type: string) {
  return ENTITY_ICONS[type] ?? (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <circle cx="12" cy="12" r="10" />
      <line x1="12" y1="8" x2="12" y2="12" />
      <line x1="12" y1="16" x2="12.01" y2="16" />
    </svg>
  );
}

function egressBadge(egress: string): { color: string; label: string } {
  if (egress === 'proxy')   return { color: 'green',  label: 'proxy-routed' }
  if (egress === 'direct')  return { color: 'yellow', label: 'direct egress' }
  if (egress === 'blocked') return { color: 'red',    label: 'egress blocked' }
  return { color: 'muted', label: egress || 'unknown egress' }
}

function fmtTime(ms: number): string {
  if (!ms) return ''
  try {
    const d = new Date(ms);
    return `${d.toLocaleDateString()} ${d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}`;
  } catch { return '' }
}

export default function OsintPanel({ policy, osintCase }: Props) {
  const [target, setTarget] = useState('')
  const [pending, setPending] = useState<string | null>(null)
  const hosted = isVeyraHosted()

  const run = (kind: 'whois' | 'dns' | 'archive' | 'username') => {
    if (!hosted || !target.trim()) return
    const t = target.trim()
    setPending(kind)
    if (kind === 'whois') postVeyraAction({ action: 'osint_whois', target: t })
    else if (kind === 'dns') postVeyraAction({ action: 'osint_dns', target: t })
    else if (kind === 'archive') postVeyraAction({ action: 'osint_archive', target: t })
    else postVeyraAction({ action: 'osint_username', target: t })
    setTimeout(() => setPending(null), 5000)
  }

  if (!policy.enabled) {
    return (
      <div className="osint-panel">
        <div className="section-title">Recon Workspace Status</div>
        <div className="ai-disabled card" style={{ padding: '40px 20px', textAlign: 'center' }}>
          <div style={{
            width: 48, height: 48, borderRadius: '50%',
            background: 'rgba(255, 59, 82, 0.05)', border: '1px solid rgba(255, 59, 82, 0.15)',
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            margin: '0 auto 14px', color: 'var(--danger)'
          }}>
            <svg style={{ width: 22, height: 22 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
              <polygon points="7.86 2 16.14 2 22 7.86 22 16.14 16.14 22 7.86 22 2 16.14 2 7.86 7.86 2"></polygon>
              <line x1="15" y1="9" x2="9" y2="15"></line>
              <line x1="9" y1="9" x2="15" y2="15"></line>
            </svg>
          </div>
          <div style={{ fontWeight: 700, fontSize: 13, color: 'var(--text-primary)', marginBottom: 4 }}>OSINT Operations Blocked</div>
          <div style={{ color: 'var(--text-muted)', fontSize: 11, maxWidth: 280, margin: '0 auto', lineHeight: 1.5 }}>
            The active persona's policy (<span className="mono">{policy.id}</span>) disables network queries for security containment.
          </div>
        </div>
      </div>
    )
  }

  const eg = egressBadge(osintCase.egress)
  const entities = osintCase.entities ?? []
  const timeline = [...(osintCase.timeline ?? [])].sort((a, b) => b.ts_ms - a.ts_ms)

  return (
    <div className="osint-panel">
      <div className="section-title">Active Recon Sandbox</div>

      {/* Policy + route posture */}
      <div className="card" style={{ marginBottom: 12 }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 8 }}>
          <span style={{ fontWeight: 800, color: 'var(--accent-bright)' }}>{policy.display_name.toUpperCase()}</span>
          <div style={{ display: 'flex', gap: 6 }}>
            {policy.require_route && (
              <span className="badge badge--blue" style={{ fontSize: 9 }}>ROUTE LOCK</span>
            )}
            {osintCase.egress && (
              <span className={`badge badge--${eg.color}`} style={{ fontSize: 9 }}>
                {eg.label.toUpperCase()}
              </span>
            )}
          </div>
        </div>
        <div style={{ fontSize: 11, color: 'var(--text-secondary)', lineHeight: 1.5 }}>
          All egress requests are locked to the target persona routing layout
          {osintCase.route_type ? ` (${osintCase.route_type.toUpperCase()})` : ''}.
        </div>
      </div>

      {/* Target Input */}
      <div className="section-title" style={{ marginTop: 16 }}>Intelligence Target</div>
      <input
        className="route-select"
        style={{ marginBottom: 8, fontFamily: 'var(--font-mono)' }}
        type="text"
        placeholder="Enter target IP, domain, username, or URL..."
        value={target}
        onChange={e => setTarget(e.target.value)}
        onKeyDown={e => e.key === 'Enter' && policy.allow_whois && run('whois')}
      />

      {/* Target Actions */}
      <div className="osint-actions" style={{ marginBottom: 12 }}>
        <button className="btn btn--ghost osint-mod-btn" style={{ fontSize: 11 }}
          disabled={!hosted || !policy.allow_whois || !target.trim() || pending === 'whois'}
          onClick={() => run('whois')}>
          WHOIS Lookup
        </button>
        <button className="btn btn--ghost osint-mod-btn" style={{ fontSize: 11 }}
          disabled={!hosted || !policy.allow_dns || !target.trim() || pending === 'dns'}
          onClick={() => run('dns')}>
          DNS Recon
        </button>
        <button className="btn btn--ghost osint-mod-btn" style={{ fontSize: 11 }}
          disabled={!hosted || !policy.allow_archive || !target.trim() || pending === 'archive'}
          onClick={() => run('archive')}>
          Wayback Hist
        </button>
        <button className="btn btn--ghost osint-mod-btn" style={{ fontSize: 11 }}
          disabled={!hosted || !policy.allow_username_search || policy.max_username_sites <= 0 || !target.trim() || pending === 'username'}
          onClick={() => run('username')}
          title={policy.allow_username_search ? 'Search username across sites' : 'Disabled by policy'}>
          Sherlock Username
        </button>
      </div>

      {/* Last status */}
      {osintCase.active && (osintCase.last_summary || osintCase.last_error) && (
        <div className={`card osint-status${osintCase.last_ok ? '' : ' osint-status--error'}`} style={{ marginBottom: 14, borderLeft: osintCase.last_ok ? '2px solid var(--success)' : '2px solid var(--danger)' }}>
          <div className="osint-status-text" style={{ fontFamily: 'var(--font-mono)', fontSize: 11 }}>
            {osintCase.last_ok ? `[+] ${osintCase.last_summary}` : `[!] ${osintCase.last_error}`}
          </div>
        </div>
      )}

      {/* Entities */}
      {entities.length > 0 && (
        <>
          <div className="osint-subhead" style={{ marginTop: 16, marginBottom: 8 }}>
            <span className="section-title">
              Extracted Entities ({entities.length})
            </span>
            {isVeyraHosted() && (
              <button className="btn btn--danger" style={{ fontSize: 10, padding: '2px 8px' }}
                onClick={() => postVeyraAction({ action: 'osint_clear' })}>
                Clear Investigation
              </button>
            )}
          </div>
          <div className="osint-entity-list" style={{ marginBottom: 14 }}>
            {entities.map((e: OsintEntity, i) => (
              <div key={i} className="osint-entity" style={{ display: 'flex', alignItems: 'center' }}>
                <span className="osint-entity-icon" style={{ display: 'flex', alignItems: 'center', width: 16, height: 16, color: 'var(--accent-bright)' }}>
                  {getEntityIcon(e.type)}
                </span>
                <div className="osint-entity-body" style={{ marginLeft: 8 }}>
                  <div className="osint-entity-value mono" style={{ fontSize: 11, color: 'var(--text-primary)', fontWeight: 600 }}>{e.value}</div>
                  <div className="osint-entity-meta">
                    <span className="badge badge--muted" style={{ fontSize: 9 }}>{e.type.toUpperCase()}</span>
                    <span style={{ color: 'var(--text-muted)', fontSize: 10 }}>source: {e.source}</span>
                  </div>
                </div>
              </div>
            ))}
          </div>
        </>
      )}

      {/* Timeline */}
      {timeline.length > 0 && (
        <>
          <div className="section-title" style={{ marginTop: 18 }}>Activity Timeline ({timeline.length})</div>
          <div className="osint-timeline">
            {timeline.slice(0, 30).map((t, i) => (
              <div key={i} className="osint-tl-event">
                <span className="osint-tl-date mono">{fmtTime(t.ts_ms)}</span>
                <div className="osint-tl-body">
                  <span className="osint-tl-kind">{t.event.toUpperCase()}</span>
                  <span className="osint-tl-detail" style={{ fontFamily: 'var(--font-mono)' }}>{t.detail}</span>
                </div>
              </div>
            ))}
          </div>
        </>
      )}

      {!hosted && (
        <div style={{ marginTop: 16, fontSize: 11, color: 'var(--text-muted)', textAlign: 'center', lineHeight: 1.5 }}>
          OSINT queries require connection to the local command host.
        </div>
      )}
    </div>
  )
}
